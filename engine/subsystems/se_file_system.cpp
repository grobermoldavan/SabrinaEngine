
#include "se_file_system.hpp"
#include "engine/engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

#define SE_FS_SEPARATOR "\\"
#define SE_FS_SEPARATOR_CHAR '\\'

constexpr const size_t SE_FS_MAX_FILES = 4095;
constexpr const size_t SE_FS_MAX_FOLDERS = 1023;

using SeFileMask = SeBitMask<SE_FS_MAX_FILES + 1>;
using SeFolderMask = SeBitMask<SE_FS_MAX_FOLDERS + 1>;

struct SeFileSystemFile
{
    SeString fullPath;
    SeFolderHandle folder;
    const char* extension; // pointer to a fillPath string
};

struct SeFileSystemFolder
{
    SeString        fullPath;
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

        void process_folder_recursive(SeFolderHandle parentHandle, SeString fullPath)
        {
            se_assert(g_fileSystem.numFolders < SE_FS_MAX_FOLDERS);
            const size_t folderIndex = g_fileSystem.numFolders + 1;
            const SeFolderHandle folderHandle { SeFolderHandle::NumericType(folderIndex) };
            g_fileSystem.folders[folderIndex] =
            {
                .fullPath               = fullPath,
                .parentFolder           = parentHandle,
                .filesRecursive         = { },
                .filesNonRecursive      = { },
                .foldersRecursive       = { },
                .foldersNonRecursive    = { },
            };
            SeFileSystemFolder& folder = g_fileSystem.folders[folderIndex];
            g_fileSystem.numFolders += 1;

            set_folder_child_recursive(parentHandle, folderIndex);
            if (parentHandle)
            {
                SeFileSystemFolder& parentFolder = from_handle(parentHandle);
                utils::bm_set(parentFolder.foldersNonRecursive, folderIndex);
            }

            const SeString searchPath = string::create_fmt(SeStringLifetime::TEMPORARY, "{}" SE_FS_SEPARATOR "*", fullPath);
            WIN32_FIND_DATAA findResult;
            const HANDLE searchHandle = FindFirstFileA(string::cstr(searchPath), &findResult);

            while (FindNextFileA(searchHandle, &findResult))
            {
                if (findResult.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (strcmp(".", findResult.cFileName) == 0 || strcmp("..", findResult.cFileName) == 0)
                    {
                        continue;
                    }
                    process_folder_recursive(folderHandle, string::create_fmt(SeStringLifetime::PERSISTENT, "{}" SE_FS_SEPARATOR "{}", fullPath, findResult.cFileName));
                }
                else
                {
                    se_assert(g_fileSystem.numFiles < SE_FS_MAX_FILES);
                    const size_t fileIndex = g_fileSystem.numFiles + 1;
                    const SeString fileFullPath = string::create_fmt(SeStringLifetime::PERSISTENT, "{}" SE_FS_SEPARATOR "{}", fullPath, findResult.cFileName);
                    const char* fileFullPathCstr = string::cstr(fileFullPath);
                    const size_t fileFullPathLength = string::length(fileFullPath);
                    const char* extensionCstr = nullptr;
                    for (size_t it = fileFullPathLength; it > 0; it--)
                    {
                        const char c = fileFullPathCstr[it - 1];
                        if (c == '.')
                        {
                            extensionCstr = fileFullPathCstr + it;
                            break;
                        }
                        else if (c == SE_FS_SEPARATOR_CHAR)
                        {
                            extensionCstr = fileFullPathCstr + fileFullPathLength;
                            break;
                        }
                    }

                    g_fileSystem.files[fileIndex] =
                    {
                        .fullPath = fileFullPath,
                        .folder = folderHandle,
                        .extension = extensionCstr,
                    };
                    g_fileSystem.numFiles += 1;
                    utils::bm_set(folder.filesNonRecursive, fileIndex);
                    set_file_child_recursive(parentHandle, fileIndex);
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
    }

    inline SeFileHandle file_find(const char* fileName, SeFolderHandle handle)
    {
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_file_search(folder.filesNonRecursive, fileName);
    }
    
    inline SeFolderHandle folder_find(const char* folderName, SeFolderHandle handle)
    {
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_folder_search(folder.foldersNonRecursive, folderName);
    }

    inline SeFileHandle file_find_recursive(const char* fileName, SeFolderHandle handle)
    {
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_file_search(folder.filesRecursive, fileName);
    }

    inline SeFolderHandle folder_find_recursive(const char* folderName, SeFolderHandle handle)
    {
        const SeFileSystemFolder& folder = impl::from_handle(handle);
        return impl::perform_folder_search(folder.foldersRecursive, folderName);
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

    SeFileContent file_load(SeFileHandle handle, const AllocatorBindings& bindings)
    {
        const SeFileSystemFile& file = impl::from_handle(handle);

        const HANDLE winHandle = CreateFileA
        (
            string::cstr(file.fullPath),
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL /*| FILE_FLAG_WRITE_THROUGH // ? */,
            nullptr
        );

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
    SeString path(const Args& ... args)
    {
        SeStringBuilder builder = string_builder::begin(nullptr, SeStringLifetime::TEMPORARY);
        string_builder::append_with_separator(builder, SE_FS_SEPARATOR, args...);
        return string_builder::end(builder);
    }
    
    void engine::init()
    {
        constexpr const DWORD FULL_PATH_NAME_BUFFER_SIZE = 2048;
        static char rootNameBuffer[FULL_PATH_NAME_BUFFER_SIZE];
        
        const DWORD gcdResult = GetCurrentDirectoryA(FULL_PATH_NAME_BUFFER_SIZE, rootNameBuffer);
        se_assert_msg(gcdResult > 0, "Can't get root directory path - internal error. Error code : {}", GetLastError());
        se_assert_msg(gcdResult < FULL_PATH_NAME_BUFFER_SIZE, "Can't get root directory path - buffer is too small. Required size (with terminating character) is {}", gcdResult);

        impl::process_folder_recursive({ 0 }, string::create(rootNameBuffer, SeStringLifetime::PERSISTENT) );
    }

    void engine::terminate()
    {

    }

} // namespace file_system

#else // ifdef _WIN32
#   error Unsupported platform
#endif
