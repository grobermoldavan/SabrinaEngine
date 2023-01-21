
#include "se_file_system.hpp"
#include "engine/engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>
#include <ShlObj_core.h>
#include <Shlwapi.h>
#include <PathCch.h>

#define SE_FS_SEPARATOR "\\"
#define SE_FS_SEPARATOR_W L"\\"
#define SE_FS_SEPARATOR_CHAR '\\'

#define se_fs_is_separator_c(c) (((c) == '\\') || ((c) == '/'))

#ifdef SE_DEBUG
#   define se_fs_validate_path(path) ::fs::impl::validate_path(path)
#else
#   define se_fs_validate_path(path)
#endif

struct SeUtf16String
{
    WCHAR* buffer;
    size_t length;
};

constexpr const size_t SE_FS_MAX_FILES = 4095;
constexpr const size_t SE_FS_MAX_FOLDERS = 1023;

using SeFileMask = SeBitMask<SE_FS_MAX_FILES + 1>;
using SeFolderMask = SeBitMask<SE_FS_MAX_FOLDERS + 1>;

struct SeFileSystemFile
{
    SeString        fullPath;
    SeUtf16String   fullPathW;

    SeFolderHandle  folder;
    const char*     extension; // pointer to a fillPath string
};

struct SeFileSystemFolder
{
    SeString        fullPath;
    SeUtf16String   fullPathW;

    SeFolderHandle  parentFolder;
    SeFileMask      filesRecursive;
    SeFileMask      filesNonRecursive;
    SeFolderMask    foldersRecursive;
    SeFolderMask    foldersNonRecursive;
};

struct SeFileSystem
{
    // + 1 because first entry is an invalid folder/file
    SeFileSystemFile files[SE_FS_MAX_FILES + 1];
    SeFileSystemFolder folders[SE_FS_MAX_FOLDERS + 1];
    
    size_t numFiles;
    size_t numFolders;
} g_fileSystem = {};

namespace fs
{
    namespace utf16_string
    {
        namespace impl
        {
            template<typename Arg>
            void process(const WCHAR** elements, size_t index, const Arg& arg)
            {
                elements[index] = (const WCHAR*)arg;
            }

            template<typename Arg, typename ... Args>
            void process(const WCHAR** elements, size_t index, const Arg& arg, const Args& ... args)
            {
                elements[index] = (const WCHAR*)arg;
                process(elements, index + 1, args...);
            }
        }

        template<typename ... Args>
        SeUtf16String create(const AllocatorBindings& allocator, const Args& ... args)
        {
            const WCHAR* argsCstr[sizeof...(args)];
            impl::process(argsCstr, 0, args...);

            size_t argsLength[sizeof...(args)];
            size_t length = 0;
            for (size_t it = 0; it < sizeof...(args); it++)
            {
                const size_t argLength = wcslen(argsCstr[it]);
                length += argLength;
                argsLength[it] = argLength;
            }

            WCHAR* const buffer = (WCHAR*)allocator_bindings::alloc(allocator, sizeof(WCHAR) * (length + 1), se_alloc_tag);
            size_t pivot = 0;
            for (size_t it = 0; it < sizeof...(args); it++)
            {
                memcpy(buffer + pivot, argsCstr[it], sizeof(WCHAR) * argsLength[it]);
                pivot += argsLength[it];
            }
            buffer[length] = 0;
            return { buffer, length };
        }

        inline const WCHAR* cstr(const SeUtf16String& str)
        {
            return str.buffer;
        }

        SeString to_utf8(const SeUtf16String& str)
        {
            const int length = WideCharToMultiByte(CP_UTF8, 0, str.buffer, int(str.length), NULL, 0, NULL, NULL);
            se_assert_msg(length, "Unable to convert string from utf16 to utf8. Error code is : {}", GetLastError());

            // string::create allocates string with null-terminated character, so we don't need to set it manually
            SeString result = string::create(length, SeStringLifetime::PERSISTENT);
            char* resultCstr = string::cstr(result);
            const int converisonResult = WideCharToMultiByte(CP_UTF8, 0, str.buffer, int(str.length), resultCstr, length, NULL, NULL);
            se_assert_msg(converisonResult, "Unable to convert string from utf16 to utf8. Error code is : {}", GetLastError());
            se_assert_msg(converisonResult == length, "Length of utf8 string chaged between WideCharToMultiByte calls. Something went terribly wrong!");
            
            return result;
        }

        SeUtf16String from_utf8_tmp(const char* source)
        {
            const int sourceLength = int(strlen(source));
            const int length = MultiByteToWideChar(CP_UTF8, 0, source, sourceLength, NULL, 0);
            se_assert_msg(length, "Unable to convert string from utf8 to utf16. Error code is : {}", GetLastError());

            WCHAR* const buffer = (WCHAR*)allocator_bindings::alloc(allocators::frame(), sizeof(WCHAR) * (length + 1), se_alloc_tag);
            const int converisonResult = MultiByteToWideChar(CP_UTF8, 0, source, sourceLength, buffer, length);
            se_assert_msg(converisonResult, "Unable to convert string from utf8 to utf16. Error code is : {}", GetLastError());
            se_assert_msg(converisonResult == length, "Length of utf16 string chaged between MultiByteToWideChar calls. Something went terribly wrong!");

            buffer[length] = 0;
            return { buffer, size_t(length) };
        }

        void destroy(SeUtf16String& str)
        {
            allocator_bindings::dealloc(allocators::persistent(), str.buffer, sizeof(WCHAR) * (str.length + 1));
            str.buffer = nullptr;
            str.length = 0;
        }
    }

    namespace impl
    {
        inline SeFileSystemFolder& from_handle(SeFolderHandle handle)
        {
            using Type = SeFolderHandle::NumericType;
            const Type index = Type(handle);
            se_assert_msg(index, "Can't get folder from an invalid handle");
            se_assert_msg(index <= g_fileSystem.numFolders, "Can't get folder from an invalid handle");
            return g_fileSystem.folders[index];
        }

        inline SeFileSystemFile& from_handle(SeFileHandle handle)
        {
            using Type = SeFileHandle::NumericType;
            const Type index = Type(handle);
            se_assert_msg(index, "Can't get file from an invalid handle");
            se_assert_msg(index <= g_fileSystem.numFiles, "Can't get file from an invalid handle");
            return g_fileSystem.files[index];
        }

        bool check_name(const char* name, const char* fullPath)
        {
            const size_t nameLength = strlen(name);
            const size_t pathLength = strlen(fullPath);
            if (nameLength > pathLength) return false;

            const bool result = strcmp(fullPath + (pathLength - nameLength), name) == 0;
            return result;
        }

        void set_folder_child_recursive(SeFolderHandle parentFolderHandle, size_t childFolderIndex)
        {
            if (!parentFolderHandle) return;
            SeFileSystemFolder& parentFolder = from_handle(parentFolderHandle);
            utils::bm_set(parentFolder.foldersRecursive, childFolderIndex);
            set_folder_child_recursive(parentFolder.parentFolder, childFolderIndex);
        }

        void set_file_child_recursive(SeFolderHandle parentFolderHandle, size_t childFileIndex)
        {
            if (!parentFolderHandle) return;
            SeFileSystemFolder& parentFolder = from_handle(parentFolderHandle);
            utils::bm_set(parentFolder.filesRecursive, childFileIndex);
            set_file_child_recursive(parentFolder.parentFolder, childFileIndex);
        }

        const char* file_name_get_extension(const SeString& filePath)
        {
            const char* filePathCstr = string::cstr(filePath);
            const size_t filePathLength = string::length(filePath);
            const char* extensionCstr = nullptr;
            for (size_t it = filePathLength; it > 0; it--)
            {
                const char c = filePathCstr[it - 1];
                if (c == '.')
                {
                    extensionCstr = filePathCstr + it;
                    break;
                }
                else if (c == SE_FS_SEPARATOR_CHAR)
                {
                    extensionCstr = filePathCstr + filePathLength;
                    break;
                }
            }
            return extensionCstr;
        }

        void process_folder_recursive(SeFolderHandle folderHandle)
        {
            SeFileSystemFolder& folder = from_handle(folderHandle);

            string::cast(nullptr);

            const SeUtf16String searchPathW = utf16_string::create(allocators::frame(), utf16_string::cstr(folder.fullPathW), SE_FS_SEPARATOR_W, L"*");
            WIN32_FIND_DATAW findResult;
            const HANDLE searchHandle = FindFirstFileW(utf16_string::cstr(searchPathW), &findResult);

            while (FindNextFileW(searchHandle, &findResult))
            {
                if (findResult.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (StrCmpW(L".", findResult.cFileName) == 0 || StrCmpW(L"..", findResult.cFileName) == 0)
                    {
                        continue;
                    }

                    se_assert(g_fileSystem.numFolders < SE_FS_MAX_FOLDERS);
                    const size_t newFolderIndex = ++g_fileSystem.numFolders;
                    const SeUtf16String newFolderFullPathW = utf16_string::create(allocators::persistent(), utf16_string::cstr(folder.fullPathW), SE_FS_SEPARATOR_W, findResult.cFileName);
                    g_fileSystem.folders[newFolderIndex] =
                    {
                        .fullPath               = utf16_string::to_utf8(newFolderFullPathW),
                        .fullPathW              = newFolderFullPathW,
                        .parentFolder           = folderHandle,
                        .filesRecursive         = { },
                        .filesNonRecursive      = { },
                        .foldersRecursive       = { },
                        .foldersNonRecursive    = { },
                    };
                    set_folder_child_recursive(folderHandle, newFolderIndex);
                    utils::bm_set(folder.foldersNonRecursive, newFolderIndex);

                    const SeFolderHandle newFolderHandle { SeFolderHandle::NumericType(newFolderIndex) };
                    process_folder_recursive(newFolderHandle);
                }
                else
                {
                    se_assert(g_fileSystem.numFiles < SE_FS_MAX_FILES);
                    const size_t fileIndex = ++g_fileSystem.numFiles;
                    const SeUtf16String fileFullPathW = utf16_string::create(allocators::persistent(), utf16_string::cstr(folder.fullPathW), SE_FS_SEPARATOR_W, findResult.cFileName);
                    const SeString fileFullPath = utf16_string::to_utf8(fileFullPathW);
                    g_fileSystem.files[fileIndex] =
                    {
                        .fullPath   = fileFullPath,
                        .fullPathW  = fileFullPathW,
                        .folder     = folderHandle,
                        .extension  = file_name_get_extension(fileFullPath),
                    };
                    utils::bm_set(folder.filesNonRecursive, fileIndex);
                    set_file_child_recursive(folderHandle, fileIndex);
                }
            }
        }

        SeFolderHandle perform_folder_search(const SeFolderMask& mask, const char* name)
        {
            for (size_t it = 0; it < g_fileSystem.numFolders; it++)
            {
                if (!utils::bm_get(mask, it + 1)) continue;

                const SeFolderHandle candidateHandle { SeFolderHandle::NumericType(it + 1) };
                const SeFileSystemFolder& candidate = impl::from_handle(candidateHandle);
                const char* fullPath = string::cstr(candidate.fullPath);
                if (!impl::check_name(name, fullPath)) continue;
                
                return candidateHandle;
            }
            return { 0 };
        }

        SeFileHandle perform_file_search(const SeFileMask& mask, const char* name)
        {
            for (size_t it = 0; it < g_fileSystem.numFiles; it++)
            {
                if (!utils::bm_get(mask, it + 1)) continue;

                const SeFileHandle candidateHandle { SeFileHandle::NumericType(it + 1) };
                const SeFileSystemFile& candidate = impl::from_handle(candidateHandle);
                const char* fullPath = string::cstr(candidate.fullPath);
                if (!impl::check_name(name, fullPath)) continue;
                
                return candidateHandle;
            }
            return { 0 };
        }

        template<typename Arg>
        void process_path_elements(const char** elements, size_t it, const Arg& arg)
        {
            const SeString argStr = string::cast(arg, SeStringLifetime::TEMPORARY);
            const char* const argCstr = string::cstr(argStr);
            const size_t argLength = string::length(argStr);

            se_assert_msg(argLength > 0, "All fs::path arguments must be a non-empty strings");
            se_assert_msg(!utils::compare(argCstr, "."), "fs::path argument can't be a \".\" symbol");
            se_assert_msg(!utils::compare(argCstr, ".."), "fs::path argument can't be a \"..\" symbol");
            se_assert_msg(argCstr[0] != SE_FS_SEPARATOR_CHAR, "fs::path argument can't start with \"" SE_FS_SEPARATOR "\" symbol");
            se_assert_msg(argCstr[0] != '/', "fs::path argument can't start with \"/\" symbol");

            elements[it] = argCstr;
        }

        template<typename Arg, typename ... Args>
        void process_path_elements(const char** elements, size_t it, const Arg& arg, const Args& ... args)
        {
            process_path_elements(elements, it, arg);
            process_path_elements(elements, it + 1, args...);
        }

        void validate_path(const char* path)
        {
            se_assert_msg(path, "Path is null");

            const size_t pathLength = strlen(path);
            se_assert_msg(pathLength, "Path length is zero");
            se_assert_msg(pathLength < MAX_PATH, "Path length is too big, windows supports only paths with length below {} characters", MAX_PATH);

            bool isLastSymbolSeparator = false;
            for (size_t it = 0; it < pathLength; it++)
            {
                const char c = path[it];
                if (se_fs_is_separator_c(c))
                {
                    se_assert_msg(!isLastSymbolSeparator, "Paths are not allowed to have multiple separators in the row. Path is : {}", path);
                    isLastSymbolSeparator = true;
                }
                else
                {
                    isLastSymbolSeparator = false;
                }
            }
        }

        inline bool is_child_of(SeFolderHandle parentHandle, SeFolderHandle childHandle)
        {
            const SeFileSystemFolder& parentFolder = from_handle(parentHandle);
            return utils::bm_get(parentFolder.foldersNonRecursive, childHandle.value);
        }

        inline bool is_child_of(SeFolderHandle parentHandle, SeFileHandle childHandle)
        {
            const SeFileSystemFolder& parentFolder = from_handle(parentHandle);
            return utils::bm_get(parentFolder.filesNonRecursive, childHandle.value);
        }
    }

    inline SeFileHandle file_find(const char* filePath, SeFolderHandle handle)
    {
        se_fs_validate_path(filePath);
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_file_search(folder.filesNonRecursive, filePath);
    }
    
    inline SeFolderHandle folder_find(const char* folderPath, SeFolderHandle handle)
    {
        se_fs_validate_path(folderPath);
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_folder_search(folder.foldersNonRecursive, folderPath);
    }

    inline SeFileHandle file_find_recursive(const char* filePath, SeFolderHandle handle)
    {
        se_fs_validate_path(filePath);
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_file_search(folder.filesRecursive, filePath);
    }

    inline SeFolderHandle folder_find_recursive(const char* folderPath, SeFolderHandle handle)
    {
        se_fs_validate_path(folderPath);
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_folder_search(folder.foldersRecursive, folderPath);
    }

    inline SeFolderHandle file_get_folder(SeFileHandle handle)
    {
        const SeFileSystemFile& file = impl::from_handle(handle);
        se_assert_msg(file.folder, "Each file must have folder. Something went terribly wrong!");
        return file.folder;
    }
    
    inline const char* file_extension(SeFileHandle handle)
    {
        const SeFileSystemFile& file = impl::from_handle(handle);
        return file.extension;
    }

    SeFileContent file_read(SeFileHandle handle, const AllocatorBindings& bindings)
    {
        const SeFileSystemFile& file = impl::from_handle(handle);

        const HANDLE winHandle = CreateFileA
        (
            string::cstr(file.fullPath),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL /*| FILE_FLAG_WRITE_THROUGH // ? */,
            nullptr
        );
        se_assert_msg(winHandle != INVALID_HANDLE_VALUE, "Can't read file. Error code is {}", GetLastError());

        uint64_t fileSize = 0;
        {
            LARGE_INTEGER size = { };
            const BOOL result = GetFileSizeEx(winHandle, &size);
            se_assert(result);
            fileSize = size.QuadPart;
        }

        void* const buffer = allocator_bindings::alloc(bindings, fileSize + 1, se_alloc_tag);
        {
            DWORD bytesRead = 0;
            const BOOL result = ReadFile
            (
                winHandle,
                buffer,
                (DWORD)fileSize, // @TODO : safe cast
                &bytesRead,
                nullptr
            );
            se_assert(result);
        }
        ((char*)buffer)[fileSize] = 0;

        CloseHandle(winHandle);

        return
        {
            bindings,
            buffer,
            fileSize,
        };
    }

    void file_free(SeFileContent& content)
    {
        if (!content.data) return;
        allocator_bindings::dealloc(content.bindings, content.data, content.dataSize + 1);
        content = {};
    }

    SeFileHandle file_create(const char* filePath, SeFolderHandle folderHandle)
    {
        se_assert_msg(filePath, "fs::file_create failed : filePath is null");
        se_assert_msg(strlen(filePath) > 0, "fs::file_create failed : filePath is an empty string");
        se_assert_msg(!utils::contains(filePath, SE_FS_SEPARATOR_CHAR), "fs::file_create doesn't accept paths with separators. Incorrect path is : {}", filePath);
        se_assert_msg(!utils::contains(filePath, '/'), "fs::file_create doesn't accept paths with separators. Incorrect path is : {}", filePath);
        se_assert_msg(impl::is_child_of(SE_USER_DATA_FOLDER, folderHandle), "fs::file_create can create files only in SE_USER_DATA_FOLDER or it's subfolders");

        const SeUtf16String filePathW = utf16_string::from_utf8_tmp(filePath);
        se_assert_msg(PathIsRelativeW(utf16_string::cstr(filePathW)), "fs::file_create accepts only relative paths. Incorrect path is : {}", filePath);

        if (const SeFileHandle foundHandle = file_find(filePath, folderHandle))
        {
            return foundHandle;
        }

        SeFileSystemFolder& folder = impl::from_handle(folderHandle);
        const SeUtf16String fileFullPathW = utf16_string::create(allocators::persistent(), utf16_string::cstr(folder.fullPathW), SE_FS_SEPARATOR_W, utf16_string::cstr(filePathW));
        const SeString fileFullPath = utf16_string::to_utf8(fileFullPathW);
        const size_t fileIndex = ++g_fileSystem.numFiles;
        g_fileSystem.files[fileIndex] =
        {
            .fullPath   = fileFullPath,
            .fullPathW  = fileFullPathW,
            .folder     = folderHandle,
            .extension  = impl::file_name_get_extension(fileFullPath),
        };

        utils::bm_set(folder.filesNonRecursive, fileIndex);
        impl::set_file_child_recursive(folderHandle, fileIndex);

        return { SeFileHandle::NumericType(fileIndex) };
    }

    SeFolderHandle folder_create(const char* folderPath, SeFolderHandle parentFolderHandle)
    {
        se_assert_msg(folderPath, "fs::folder_create failed : folderPath is null");
        se_assert_msg(strlen(folderPath) > 0, "fs::folder_create failed : folderPath is an empty string");
        se_assert_msg(!utils::contains(folderPath, SE_FS_SEPARATOR_CHAR), "fs::folder_create doesn't accept paths with separators. Incorrect path is : {}", folderPath);
        se_assert_msg(!utils::contains(folderPath, '/'), "fs::folder_create doesn't accept paths with separators. Incorrect path is : {}", folderPath);
        se_assert_msg(impl::is_child_of(SE_USER_DATA_FOLDER, parentFolderHandle), "fs::folder_create can create files only in SE_USER_DATA_FOLDER or it's subfolders");

        const SeUtf16String folderPathW = utf16_string::from_utf8_tmp(folderPath);
        se_assert_msg(PathIsRelativeW(utf16_string::cstr(folderPathW)), "fs::folder_create accepts only relative paths. Incorrect path is : {}", folderPath);

        if (const SeFolderHandle foundHandle = folder_find_recursive(folderPath, parentFolderHandle))
        {
            return foundHandle;
        }

        SeFileSystemFolder& parentFolder = impl::from_handle(parentFolderHandle);
        const SeUtf16String folderFullPathW = utf16_string::create(allocators::persistent(), utf16_string::cstr(parentFolder.fullPathW), SE_FS_SEPARATOR_W, utf16_string::cstr(folderPathW));
        const size_t folderIndex = ++g_fileSystem.numFolders;
        g_fileSystem.folders[folderIndex] =
        {
            .fullPath               = utf16_string::to_utf8(folderFullPathW),
            .fullPathW              = folderFullPathW,
            .parentFolder           = parentFolderHandle,
            .filesRecursive         = { },
            .filesNonRecursive      = { },
            .foldersRecursive       = { },
            .foldersNonRecursive    = { },
        };

        utils::bm_set(parentFolder.foldersNonRecursive, folderIndex);
        impl::set_folder_child_recursive(parentFolderHandle, folderIndex);

        const int cdResult = SHCreateDirectoryExW(NULL, utf16_string::cstr(folderFullPathW), NULL);
        se_assert_msg(cdResult != ERROR_BAD_PATHNAME && cdResult != ERROR_FILENAME_EXCED_RANGE && cdResult != ERROR_CANCELLED, "Failed to crete folder with path : {}", g_fileSystem.folders[folderIndex].fullPath);

        return { SeFolderHandle::NumericType(folderIndex) };
    }

    inline SeFolderHandle parent_folder(SeFolderHandle handle)
    {
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return folder.parentFolder;
    }

    inline SeFolderHandle parent_folder(SeFileHandle handle)
    {
        const SeFileSystemFile& file = impl::from_handle(handle);
        return file.folder;
    }
    
    const char* full_path(SeFileHandle handle)
    {
        const SeFileSystemFile& file = impl::from_handle(handle);
        return string::cstr(file.fullPath);
    }

    const char* full_path(SeFolderHandle handle)
    {
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return string::cstr(folder.fullPath);
    }

    template<typename ... Args>
    inline const char* path(const Args& ... args)
    {
        constexpr const size_t NUM_ARGS = sizeof...(args);
        static_assert(NUM_ARGS > 0, "Can't have empty path");

        const char* pathElements[NUM_ARGS];
        impl::process_path_elements(pathElements, 0, args...);

        SeStringBuilder builder = string_builder::begin(nullptr, SeStringLifetime::TEMPORARY);
        for (size_t it = 0; it < NUM_ARGS; it++)
        {
            if (it != 0) string_builder::append(builder, SE_FS_SEPARATOR);
            string_builder::append(builder, pathElements[it]);
        }

        const SeString pathStr = string_builder::end(builder);
        const char* const pathCstr = string::cstr(pathStr);
        se_fs_validate_path(pathCstr);

        return pathCstr;
    }
    
    void engine::init(const SeSettings& settings)
    {
        se_assert(g_fileSystem.numFolders == 0);
        g_fileSystem.numFolders = 2; // first and second folders are reserved for SE_APPLICATION_FOLDER and SE_USER_DATA_FOLDER

        static WCHAR applicationFolderBuffer[MAX_PATH];

        const DWORD gmfnResult = GetModuleFileNameW(NULL, applicationFolderBuffer, MAX_PATH);
        se_assert_msg(gmfnResult > 0, "Can't get root directory path - internal error. Error code : {}", GetLastError());
        se_assert_msg(gmfnResult < MAX_PATH, "Can't get root directory path - buffer is too small. Required size (with terminating character) is {}", gmfnResult);

        const BOOL prfsResult = PathRemoveFileSpecW(applicationFolderBuffer);
        se_assert_msg(prfsResult, "Can't get root directory path - PathRemoveFileSpecA failed with code {}", prfsResult);

        const SeUtf16String applicationFolderFullPathW = utf16_string::create(allocators::persistent(), applicationFolderBuffer);
        SeFileSystemFolder& applicationFolder = impl::from_handle(SE_APPLICATION_FOLDER);
        applicationFolder =
        {
            .fullPath               = utf16_string::to_utf8(applicationFolderFullPathW),
            .fullPathW              = applicationFolderFullPathW,
            .parentFolder           = { 0 },
            .filesRecursive         = { },
            .filesNonRecursive      = { },
            .foldersRecursive       = { },
            .foldersNonRecursive    = { },
        };
        impl::process_folder_recursive(SE_APPLICATION_FOLDER);

        if (settings.createUserDataFolder)
        {
            static WCHAR userDataFolderBuffer[MAX_PATH];
            const HRESULT gfpResult = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, userDataFolderBuffer);
            se_assert_msg(SUCCEEDED(gfpResult), "Can't get user data folder path. Error code : {}", gfpResult);

            const SeUtf16String applicationNameW = utf16_string::from_utf8_tmp(settings.applicationName);
            const SeUtf16String userDataFolderFullPathW = utf16_string::create(allocators::persistent(), userDataFolderBuffer, SE_FS_SEPARATOR_W, utf16_string::cstr(applicationNameW));

            const int cdResult = SHCreateDirectoryExW(NULL, utf16_string::cstr(userDataFolderFullPathW), NULL);
            se_assert_msg(cdResult != ERROR_BAD_PATHNAME && cdResult != ERROR_FILENAME_EXCED_RANGE && cdResult != ERROR_CANCELLED, "Can't create user data folder");

            SeFileSystemFolder& userDataFolder = impl::from_handle(SE_USER_DATA_FOLDER);
            userDataFolder =
            {
                .fullPath               = utf16_string::to_utf8(userDataFolderFullPathW),
                .fullPathW              = userDataFolderFullPathW,
                .parentFolder           = { 0 },
                .filesRecursive         = { },
                .filesNonRecursive      = { },
                .foldersRecursive       = { },
                .foldersNonRecursive    = { },
            };
            impl::process_folder_recursive(SE_USER_DATA_FOLDER);
        }
    }

    void engine::terminate()
    {
        for (size_t it = 0; it < g_fileSystem.numFiles; it++)
        {
            SeFileSystemFile& file = g_fileSystem.files[it];
            string::destroy(file.fullPath);
            utf16_string::destroy(file.fullPathW);
        }

        for (size_t it = 0; it < g_fileSystem.numFolders; it++)
        {
            SeFileSystemFolder& folder = g_fileSystem.folders[it];
            string::destroy(folder.fullPath);
            utf16_string::destroy(folder.fullPathW);
        }
    }

} // namespace fs

bool SeFolderIteratorInstance::operator != (const SeFolderIteratorInstance& other) const
{
    return index != other.index;
}

SeFolderHandle SeFolderIteratorInstance::operator * () const
{
    return { SeFolderHandle::NumericType(index) };
}

SeFolderIteratorInstance& SeFolderIteratorInstance::operator ++ ()
{
    const SeFileSystemFolder& folder = fs::impl::from_handle(folderHandle);
    const SeFolderMask& bitMask = isRecursive ? folder.foldersRecursive : folder.foldersNonRecursive;
    for (index += 1; index < (SE_FS_MAX_FOLDERS + 1); index++)
    {
        if (utils::bm_get(bitMask, index)) break;
    }
    return *this;
}

bool SeFileIteratorInstance::operator != (const SeFileIteratorInstance& other) const
{
    return index != other.index;
}

SeFileHandle SeFileIteratorInstance::operator * () const
{
    return { SeFileHandle::NumericType(index) };
}

SeFileIteratorInstance& SeFileIteratorInstance::operator ++ ()
{
    const SeFileSystemFolder& folder = fs::impl::from_handle(folderHandle);
    const SeFileMask& bitMask = isRecursive ? folder.filesRecursive : folder.filesNonRecursive;
    for (index += 1; index < (SE_FS_MAX_FILES + 1); index++)
    {
        if (utils::bm_get(bitMask, index)) break;
    }
    return *this;
}

inline SeFolderIteratorInstance begin(SeFolderIterator settings)
{
    return ++(SeFolderIteratorInstance{ settings.folderHandle, 0, settings.isRecursive });
}

inline SeFolderIteratorInstance end(SeFolderIterator settings)
{
    return { settings.folderHandle, SE_FS_MAX_FOLDERS + 1, settings.isRecursive };
}

inline SeFileIteratorInstance begin(SeFileIterator settings)
{
    return ++(SeFileIteratorInstance{ settings.folderHandle, 0, settings.isRecursive });
}

inline SeFileIteratorInstance end(SeFileIterator settings)
{
    return { settings.folderHandle, SE_FS_MAX_FILES + 1, settings.isRecursive };
}

#else // ifdef _WIN32
#   error Unsupported platform
#endif
