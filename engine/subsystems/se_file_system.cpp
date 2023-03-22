
#include "se_file_system.hpp"
#include "engine/se_engine.hpp"

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
#   define se_fs_validate_path(path) ::se_fs__se_fs_validate_path(path)
#else
#   define se_fs_validate_path(path)
#endif

constexpr const size_t SE_FS_MAX_FILES = 4095;
constexpr const size_t SE_FS_MAX_FOLDERS = 1023;

using SeFileMask = SeBitMask<SE_FS_MAX_FILES + 1>;
using SeFolderMask = SeBitMask<SE_FS_MAX_FOLDERS + 1>;

struct SeFileSystemFile
{
    enum struct IOState
    {
        NOT_OPEN,
        OPEN_FOR_READ,
        OPEN_FOR_WRITE,
    };

    SeString        fullPath;
    SeStringW       fullPathW;
    const char*     extension; // pointer to a fullPath string
    const char*     name; // pointer to a fullPath string

    SeFolderHandle  folder;
    IOState         ioState;
    HANDLE          writeHandle;
};

struct SeFileSystemFolder
{
    SeString        fullPath;
    SeStringW       fullPathW;
    const char*     name; // pointer to a fullPath string

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

inline SeFileSystemFolder& _se_fs_from_handle(SeFolderHandle handle)
{
    using Type = SeFolderHandle::NumericType;
    const Type index = Type(handle);
    se_assert_msg(index, "Can't get folder from an invalid handle");
    se_assert_msg(index <= g_fileSystem.numFolders, "Can't get folder from an invalid handle");
    return g_fileSystem.folders[index];
}

inline SeFileSystemFile& _se_fs_from_handle(SeFileHandle handle)
{
    using Type = SeFileHandle::NumericType;
    const Type index = Type(handle);
    se_assert_msg(index, "Can't get file from an invalid handle");
    se_assert_msg(index <= g_fileSystem.numFiles, "Can't get file from an invalid handle");
    return g_fileSystem.files[index];
}

bool _se_fs_check_name(const char* name, const char* fullPath)
{
    const size_t nameLength = se_cstr_len(name);
    const size_t pathLength = se_cstr_len(fullPath);
    if (nameLength > pathLength) return false;

    const bool result = se_compare(fullPath + (pathLength - nameLength), name);
    return result;
}

void _se_fs_set_folder_child_recursive(SeFolderHandle parentFolderHandle, size_t childFolderIndex)
{
    if (!parentFolderHandle) return;
    SeFileSystemFolder& parentFolder = _se_fs_from_handle(parentFolderHandle);
    se_bm_set(parentFolder.foldersRecursive, childFolderIndex);
    _se_fs_set_folder_child_recursive(parentFolder.parentFolder, childFolderIndex);
}

void _se_fs_set_file_child_recursive(SeFolderHandle parentFolderHandle, size_t childFileIndex)
{
    if (!parentFolderHandle) return;
    SeFileSystemFolder& parentFolder = _se_fs_from_handle(parentFolderHandle);
    se_bm_set(parentFolder.filesRecursive, childFileIndex);
    _se_fs_set_file_child_recursive(parentFolder.parentFolder, childFileIndex);
}

const char* _se_fs_get_name_from_path(const SeString& filePath)
{
    const char* filePathCstr = se_string_cstr(filePath);
    const size_t filePathLength = se_string_length(filePath);
    const char* result = nullptr;
    for (size_t it = filePathLength; it > 0; it--)
    {
        const char c = filePathCstr[it - 1];
        if (c == SE_FS_SEPARATOR_CHAR)
        {
            result = filePathCstr + it;
            break;
        }
    }
    return result;
}

const char* _se_fs_get_extension_from_path(const SeString& filePath)
{
    const char* filePathCstr = se_string_cstr(filePath);
    const size_t filePathLength = se_string_length(filePath);
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

void _se_fs_process_folder_recursive(SeFolderHandle folderHandle)
{
    SeFileSystemFolder& folder = _se_fs_from_handle(folderHandle);

    se_string_cast(nullptr);

    const SeStringW searchPathW = se_stringw_create(se_allocator_frame(), se_stringw_cstr(folder.fullPathW), SE_FS_SEPARATOR_W, L"*");
    WIN32_FIND_DATAW findResult;
    const HANDLE searchHandle = FindFirstFileW(se_stringw_cstr(searchPathW), &findResult);

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
            const SeStringW newFolderFullPathW = se_stringw_create(se_allocator_persistent(), se_stringw_cstr(folder.fullPathW), SE_FS_SEPARATOR_W, findResult.cFileName);
            const SeString newFolderFullPath = se_stringw_to_utf8(newFolderFullPathW);
            g_fileSystem.folders[newFolderIndex] =
            {
                .fullPath               = newFolderFullPath,
                .fullPathW              = newFolderFullPathW,
                .name                   = _se_fs_get_name_from_path(newFolderFullPath),
                .parentFolder           = folderHandle,
                .filesRecursive         = { },
                .filesNonRecursive      = { },
                .foldersRecursive       = { },
                .foldersNonRecursive    = { },
            };
            _se_fs_set_folder_child_recursive(folderHandle, newFolderIndex);
            se_bm_set(folder.foldersNonRecursive, newFolderIndex);

            const SeFolderHandle newFolderHandle { SeFolderHandle::NumericType(newFolderIndex) };
            _se_fs_process_folder_recursive(newFolderHandle);
        }
        else
        {
            se_assert(g_fileSystem.numFiles < SE_FS_MAX_FILES);
            const size_t fileIndex = ++g_fileSystem.numFiles;
            const SeStringW fileFullPathW = se_stringw_create(se_allocator_persistent(), se_stringw_cstr(folder.fullPathW), SE_FS_SEPARATOR_W, findResult.cFileName);
            const SeString fileFullPath = se_stringw_to_utf8(fileFullPathW);
            g_fileSystem.files[fileIndex] =
            {
                .fullPath       = fileFullPath,
                .fullPathW      = fileFullPathW,
                .extension      = _se_fs_get_extension_from_path(fileFullPath),
                .name           = _se_fs_get_name_from_path(fileFullPath),
                .folder         = folderHandle,
                .ioState        = SeFileSystemFile::IOState::NOT_OPEN,
                .writeHandle    = INVALID_HANDLE_VALUE,
            };
            se_bm_set(folder.filesNonRecursive, fileIndex);
            _se_fs_set_file_child_recursive(folderHandle, fileIndex);
        }
    }
}

SeFolderHandle _se_fs_perform_folder_search(const SeFolderMask& mask, const char* name)
{
    for (size_t it = 0; it < g_fileSystem.numFolders; it++)
    {
        if (!se_bm_get(mask, it + 1)) continue;

        const SeFolderHandle candidateHandle { SeFolderHandle::NumericType(it + 1) };
        const SeFileSystemFolder& candidate = _se_fs_from_handle(candidateHandle);
        const char* fullPath = se_string_cstr(candidate.fullPath);
        if (!_se_fs_check_name(name, fullPath)) continue;
        
        return candidateHandle;
    }
    return { 0 };
}

SeFileHandle _se_fs_perform_file_search(const SeFileMask& mask, const char* name)
{
    for (size_t it = 0; it < g_fileSystem.numFiles; it++)
    {
        if (!se_bm_get(mask, it + 1)) continue;

        const SeFileHandle candidateHandle { SeFileHandle::NumericType(it + 1) };
        const SeFileSystemFile& candidate = _se_fs_from_handle(candidateHandle);
        const char* fullPath = se_string_cstr(candidate.fullPath);
        if (!_se_fs_check_name(name, fullPath)) continue;
        
        return candidateHandle;
    }
    return { 0 };
}

template<typename Arg>
void _se_fs_process_path_elements(const char** elements, size_t it, const Arg& arg)
{
    const SeString argStr = se_string_cast(arg, SeStringLifetime::TEMPORARY);
    const char* const argCstr = se_string_cstr(argStr);
    const size_t argLength = se_string_length(argStr);

    se_assert_msg(argLength > 0, "All se_fs_path arguments must be a non-empty strings");
    se_assert_msg(!se_compare(argCstr, "."), "se_fs_path argument can't be a \".\" symbol");
    se_assert_msg(!se_compare(argCstr, ".."), "se_fs_path argument can't be a \"..\" symbol");
    se_assert_msg(argCstr[0] != SE_FS_SEPARATOR_CHAR, "se_fs_path argument can't start with \"" SE_FS_SEPARATOR "\" symbol");
    se_assert_msg(argCstr[0] != '/', "se_fs_path argument can't start with \"/\" symbol");

    elements[it] = argCstr;
}

template<typename Arg, typename ... Args>
void _se_fs_process_path_elements(const char** elements, size_t it, const Arg& arg, const Args& ... args)
{
    _se_fs_process_path_elements(elements, it, arg);
    _se_fs_process_path_elements(elements, it + 1, args...);
}

void _se_fs_validate_path(const char* path)
{
    se_assert_msg(path, "Path is null");

    const size_t pathLength = se_cstr_len(path);
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

inline bool _se_fs_is_child_of(SeFolderHandle parentHandle, SeFolderHandle childHandle)
{
    const SeFileSystemFolder& parentFolder = _se_fs_from_handle(parentHandle);
    return se_bm_get(parentFolder.foldersRecursive, childHandle.value) || (parentHandle.value == childHandle.value);
}

inline bool _se_fs_is_child_of(SeFolderHandle parentHandle, SeFileHandle childHandle)
{
    const SeFileSystemFolder& parentFolder = _se_fs_from_handle(parentHandle);
    return se_bm_get(parentFolder.filesRecursive, childHandle.value);
}

inline SeFileHandle se_fs_file_find(const char* filePath, SeFolderHandle handle)
{
    _se_fs_validate_path(filePath);
    const SeFileSystemFolder& folder = _se_fs_from_handle(handle);
    return _se_fs_perform_file_search(folder.filesNonRecursive, filePath);
}

inline SeFolderHandle se_fs_folder_find(const char* folderPath, SeFolderHandle handle)
{
    _se_fs_validate_path(folderPath);
    const SeFileSystemFolder& folder = _se_fs_from_handle(handle);
    return _se_fs_perform_folder_search(folder.foldersNonRecursive, folderPath);
}

inline SeFileHandle se_fs_file_find_recursive(const char* filePath, SeFolderHandle handle)
{
    _se_fs_validate_path(filePath);
    const SeFileSystemFolder& folder = _se_fs_from_handle(handle);
    return _se_fs_perform_file_search(folder.filesRecursive, filePath);
}

inline SeFolderHandle se_fs_folder_find_recursive(const char* folderPath, SeFolderHandle handle)
{
    _se_fs_validate_path(folderPath);
    const SeFileSystemFolder& folder = _se_fs_from_handle(handle);
    return _se_fs_perform_folder_search(folder.foldersRecursive, folderPath);
}

inline SeFolderHandle se_fs_file_get_folder(SeFileHandle handle)
{
    const SeFileSystemFile& file = _se_fs_from_handle(handle);
    se_assert_msg(file.folder, "Each file must have folder. Something went terribly wrong!");
    return file.folder;
}

SeFileContent se_fs_file_read(SeFileHandle handle, const SeAllocatorBindings& bindings)
{
    const SeFileSystemFile& file = _se_fs_from_handle(handle);
    se_assert_msg(file.ioState != SeFileSystemFile::IOState::OPEN_FOR_WRITE, "Can't read file with path {}. It's already open for wrtie", file.fullPath);

    const HANDLE winHandle = CreateFileW
    (
        se_stringw_cstr(file.fullPathW),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    se_assert_msg(winHandle != INVALID_HANDLE_VALUE, "Can't read file with path {}. Error code is {}", file.fullPath, GetLastError());

    uint64_t fileSize = 0;
    {
        LARGE_INTEGER size = { };
        const BOOL result = GetFileSizeEx(winHandle, &size);
        se_assert(result);
        fileSize = size.QuadPart;
    }

    void* const buffer = se_alloc(bindings, fileSize + 1, se_alloc_tag);
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

void se_fs_file_content_free(SeFileContent& content)
{
    if (!content.data) return;
    se_dealloc(content.bindings, content.data, content.dataSize + 1);
    content = {};
}

void se_fs_file_write_begin(SeFileHandle handle, SeFileWriteOpenMode openMode)
{
    SeFileSystemFile& file = _se_fs_from_handle(handle);
    se_assert_msg(_se_fs_is_child_of(SE_USER_DATA_FOLDER, handle), "Can't write to file with path {}. File must be in user data fodler", file.fullPath);
    se_assert_msg(file.ioState != SeFileSystemFile::IOState::OPEN_FOR_READ, "Can't write to file with path {}. It's already open for read", file.fullPath);
    if (file.ioState == SeFileSystemFile::IOState::OPEN_FOR_WRITE)
    {
        return;
    }

    se_assert(file.writeHandle == INVALID_HANDLE_VALUE);
    const HANDLE winHandle = CreateFileW
    (
        se_stringw_cstr(file.fullPathW),
        FILE_APPEND_DATA,
        0, // not a shared file
        nullptr,
        openMode == SeFileWriteOpenMode::CLEAR ? CREATE_ALWAYS : OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL /*| FILE_FLAG_WRITE_THROUGH // ? */,
        nullptr
    );
    se_assert_msg(winHandle != INVALID_HANDLE_VALUE, "Can't read file with path {}. Error code is {}", file.fullPath, GetLastError());

    file.ioState = SeFileSystemFile::IOState::OPEN_FOR_WRITE;
    file.writeHandle = winHandle;
}

void se_fs_file_write(SeFileHandle handle, SeDataProvider data)
{
    SeFileSystemFile& file = _se_fs_from_handle(handle);
    se_assert_msg(file.ioState == SeFileSystemFile::IOState::OPEN_FOR_WRITE, "Can't write to file with path {}. It's not open for write", file.fullPath);
    se_assert_msg(file.writeHandle != INVALID_HANDLE_VALUE, "Can't write to file with path {}. File HANDLE is invalid", file.fullPath);

    const auto [memory, size] = se_data_provider_get(data);
    se_assert_msg(memory, "Can't write to file with path {}. Data is null", file.fullPath);

    DWORD bytesWritten = 0;
    const BOOL result = WriteFile
    (
        file.writeHandle,
        memory,
        DWORD(size),
        &bytesWritten,
        nullptr
    );

    se_assert_msg(result, "Write file failed. File path is {}. Error code is {}", file.fullPath, GetLastError());
    se_assert_msg(bytesWritten == DWORD(size), "Write file failed - number of written bytes is not equal to requested number of bytes. File path is {}", file.fullPath);
}

void se_fs_file_write_end(SeFileHandle handle)
{
    SeFileSystemFile& file = _se_fs_from_handle(handle);
    if (file.ioState != SeFileSystemFile::IOState::OPEN_FOR_WRITE)
    {
        return;
    }

    se_assert_msg(file.writeHandle != INVALID_HANDLE_VALUE, "Can't stop writing to file with path {}. File HANDLE is invalid", file.fullPath);
    CloseHandle(file.writeHandle);
    file.writeHandle = INVALID_HANDLE_VALUE;
    file.ioState = SeFileSystemFile::IOState::NOT_OPEN;
}

SeFileHandle se_fs_file_create(const char* filePath, SeFolderHandle folderHandle)
{
    se_assert_msg(filePath, "se_fs_file_create failed : filePath is null");
    se_assert_msg(se_cstr_len(filePath) > 0, "se_fs_file_create failed : filePath is an empty string");
    se_assert_msg(!se_contains(filePath, SE_FS_SEPARATOR_CHAR), "se_fs_file_create doesn't accept paths with separators. Incorrect path is : {}", filePath);
    se_assert_msg(!se_contains(filePath, '/'), "se_fs_file_create doesn't accept paths with separators. Incorrect path is : {}", filePath);
    se_assert_msg(_se_fs_is_child_of(SE_USER_DATA_FOLDER, folderHandle), "se_fs_file_create can create files only in SE_USER_DATA_FOLDER or it's subfolders");

    const SeStringW filePathW = se_stringw_from_utf8(se_allocator_frame(), filePath);
    se_assert_msg(PathIsRelativeW(se_stringw_cstr(filePathW)), "se_fs_file_create accepts only relative paths. Incorrect path is : {}", filePath);

    if (const SeFileHandle foundHandle = se_fs_file_find(filePath, folderHandle))
    {
        return foundHandle;
    }

    SeFileSystemFolder& folder = _se_fs_from_handle(folderHandle);
    const SeStringW fileFullPathW = se_stringw_create(se_allocator_persistent(), se_stringw_cstr(folder.fullPathW), SE_FS_SEPARATOR_W, se_stringw_cstr(filePathW));
    const SeString fileFullPath = se_stringw_to_utf8(fileFullPathW);
    const size_t fileIndex = ++g_fileSystem.numFiles;
    g_fileSystem.files[fileIndex] =
    {
        .fullPath       = fileFullPath,
        .fullPathW      = fileFullPathW,
        .extension      = _se_fs_get_extension_from_path(fileFullPath),
        .name           = _se_fs_get_name_from_path(fileFullPath),
        .folder         = folderHandle,
        .ioState        = SeFileSystemFile::IOState::NOT_OPEN,
        .writeHandle    = INVALID_HANDLE_VALUE,
    };

    se_bm_set(folder.filesNonRecursive, fileIndex);
    _se_fs_set_file_child_recursive(folderHandle, fileIndex);

    return { SeFileHandle::NumericType(fileIndex) };
}

SeFolderHandle se_fs_folder_create(const char* folderPath, SeFolderHandle parentFolderHandle)
{
    se_assert_msg(folderPath, "se_fs_folder_create failed : folderPath is null");
    se_assert_msg(se_cstr_len(folderPath) > 0, "se_fs_folder_create failed : folderPath is an empty string");
    se_assert_msg(!se_contains(folderPath, SE_FS_SEPARATOR_CHAR), "se_fs_folder_create doesn't accept paths with separators. Incorrect path is : {}", folderPath);
    se_assert_msg(!se_contains(folderPath, '/'), "se_fs_folder_create doesn't accept paths with separators. Incorrect path is : {}", folderPath);
    se_assert_msg(_se_fs_is_child_of(SE_USER_DATA_FOLDER, parentFolderHandle), "se_fs_folder_create can create files only in SE_USER_DATA_FOLDER or it's subfolders");

    const SeStringW folderPathW = se_stringw_from_utf8(se_allocator_frame(), folderPath);
    se_assert_msg(PathIsRelativeW(se_stringw_cstr(folderPathW)), "se_fs_folder_create accepts only relative paths. Incorrect path is : {}", folderPath);

    if (const SeFolderHandle foundHandle = se_fs_folder_find_recursive(folderPath, parentFolderHandle))
    {
        return foundHandle;
    }

    SeFileSystemFolder& parentFolder = _se_fs_from_handle(parentFolderHandle);
    const SeStringW folderFullPathW = se_stringw_create(se_allocator_persistent(), se_stringw_cstr(parentFolder.fullPathW), SE_FS_SEPARATOR_W, se_stringw_cstr(folderPathW));
    const size_t folderIndex = ++g_fileSystem.numFolders;
    g_fileSystem.folders[folderIndex] =
    {
        .fullPath               = se_stringw_to_utf8(folderFullPathW),
        .fullPathW              = folderFullPathW,
        .parentFolder           = parentFolderHandle,
        .filesRecursive         = { },
        .filesNonRecursive      = { },
        .foldersRecursive       = { },
        .foldersNonRecursive    = { },
    };

    se_bm_set(parentFolder.foldersNonRecursive, folderIndex);
    _se_fs_set_folder_child_recursive(parentFolderHandle, folderIndex);

    const int cdResult = SHCreateDirectoryExW(NULL, se_stringw_cstr(folderFullPathW), NULL);
    se_assert_msg(cdResult != ERROR_BAD_PATHNAME && cdResult != ERROR_FILENAME_EXCED_RANGE && cdResult != ERROR_CANCELLED, "Failed to crete folder with path : {}", g_fileSystem.folders[folderIndex].fullPath);

    return { SeFolderHandle::NumericType(folderIndex) };
}

inline SeFolderHandle se_fs_parent_folder(SeFolderHandle handle)
{
    const SeFileSystemFolder& folder = _se_fs_from_handle(handle);
    return folder.parentFolder;
}

inline SeFolderHandle se_fs_parent_folder(SeFileHandle handle)
{
    const SeFileSystemFile& file = _se_fs_from_handle(handle);
    return file.folder;
}

inline const char* se_fs_full_path(SeFileHandle handle)
{
    const SeFileSystemFile& file = _se_fs_from_handle(handle);
    return se_string_cstr(file.fullPath);
}

inline const char* se_fs_full_path(SeFolderHandle handle)
{
    const SeFileSystemFolder& folder = _se_fs_from_handle(handle);
    return se_string_cstr(folder.fullPath);
}

inline const char* se_fs_extension(SeFileHandle handle)
{
    const SeFileSystemFile& file = _se_fs_from_handle(handle);
    return file.extension;
}

inline const char* se_fs_name(SeFileHandle handle)
{
    const SeFileSystemFile& file = _se_fs_from_handle(handle);
    return file.name;
}

inline const char* se_fs_name(SeFolderHandle handle)
{
    const SeFileSystemFolder& folder = _se_fs_from_handle(handle);
    return folder.name;
}

template<typename ... Args>
inline const char* se_fs_path(const Args& ... args)
{
    constexpr const size_t NUM_ARGS = sizeof...(args);
    static_assert(NUM_ARGS > 0, "Can't have empty path");

    const char* pathElements[NUM_ARGS];
    _se_fs_process_path_elements(pathElements, 0, args...);

    SeStringBuilder builder = se_string_builder_begin(nullptr, SeStringLifetime::TEMPORARY);
    for (size_t it = 0; it < NUM_ARGS; it++)
    {
        if (it != 0) se_string_builder_append(builder, SE_FS_SEPARATOR);
        se_string_builder_append(builder, pathElements[it]);
    }

    const SeString pathStr = se_string_builder_end(builder);
    const char* const pathCstr = se_string_cstr(pathStr);
    se_fs_validate_path(pathCstr);

    return pathCstr;
}

void _se_fs_init(const SeSettings& settings)
{
    se_assert(g_fileSystem.numFolders == 0);
    g_fileSystem.numFolders = 2; // first and second folders are reserved for SE_APPLICATION_FOLDER and SE_USER_DATA_FOLDER

    static WCHAR applicationFolderBuffer[MAX_PATH];

    const DWORD gmfnResult = GetModuleFileNameW(NULL, applicationFolderBuffer, MAX_PATH);
    se_assert_msg(gmfnResult > 0, "Can't get root directory path - internal error. Error code : {}", GetLastError());
    se_assert_msg(gmfnResult < MAX_PATH, "Can't get root directory path - buffer is too small. Required size (with terminating character) is {}", gmfnResult);

    const BOOL prfsResult = PathRemoveFileSpecW(applicationFolderBuffer);
    se_assert_msg(prfsResult, "Can't get root directory path - PathRemoveFileSpecA failed with code {}", prfsResult);

    const SeStringW applicationFolderFullPathW = se_stringw_create(se_allocator_persistent(), applicationFolderBuffer);
    const SeString applicationFolderFullPath = se_stringw_to_utf8(applicationFolderFullPathW);
    SeFileSystemFolder& applicationFolder = _se_fs_from_handle(SE_APPLICATION_FOLDER);
    applicationFolder =
    {
        .fullPath               = applicationFolderFullPath,
        .fullPathW              = applicationFolderFullPathW,
        .name                   = _se_fs_get_name_from_path(applicationFolderFullPath),
        .parentFolder           = { 0 },
        .filesRecursive         = { },
        .filesNonRecursive      = { },
        .foldersRecursive       = { },
        .foldersNonRecursive    = { },
    };
    _se_fs_process_folder_recursive(SE_APPLICATION_FOLDER);

    if (settings.createUserDataFolder)
    {
        static WCHAR userDataFolderBuffer[MAX_PATH];
        const HRESULT gfpResult = SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, userDataFolderBuffer);
        se_assert_msg(SUCCEEDED(gfpResult), "Can't get user data folder path. Error code : {}", gfpResult);

        const SeStringW applicationNameW = se_stringw_from_utf8(se_allocator_frame(), settings.applicationName);
        const SeStringW userDataFolderFullPathW = se_stringw_create(se_allocator_persistent(), userDataFolderBuffer, SE_FS_SEPARATOR_W, se_stringw_cstr(applicationNameW));
        const SeString userDataFolderFullPath = se_stringw_to_utf8(userDataFolderFullPathW);

        const int cdResult = SHCreateDirectoryExW(NULL, se_stringw_cstr(userDataFolderFullPathW), NULL);
        se_assert_msg(cdResult != ERROR_BAD_PATHNAME && cdResult != ERROR_FILENAME_EXCED_RANGE && cdResult != ERROR_CANCELLED, "Can't create user data folder");

        SeFileSystemFolder& userDataFolder = _se_fs_from_handle(SE_USER_DATA_FOLDER);
        userDataFolder =
        {
            .fullPath               = userDataFolderFullPath,
            .fullPathW              = userDataFolderFullPathW,
            .name                   = _se_fs_get_name_from_path(userDataFolderFullPath),
            .parentFolder           = { 0 },
            .filesRecursive         = { },
            .filesNonRecursive      = { },
            .foldersRecursive       = { },
            .foldersNonRecursive    = { },
        };
        _se_fs_process_folder_recursive(SE_USER_DATA_FOLDER);
    }
}

void _se_fs_terminate()
{
    for (size_t it = 0; it < g_fileSystem.numFiles; it++)
    {
        SeFileSystemFile& file = g_fileSystem.files[it];
        se_string_destroy(file.fullPath);
        se_stringw_destroy(file.fullPathW);
    }

    for (size_t it = 0; it < g_fileSystem.numFolders; it++)
    {
        SeFileSystemFolder& folder = g_fileSystem.folders[it];
        se_string_destroy(folder.fullPath);
        se_stringw_destroy(folder.fullPathW);
    }
}

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
    const SeFileSystemFolder& folder = _se_fs_from_handle(folderHandle);
    const SeFolderMask& bitMask = isRecursive ? folder.foldersRecursive : folder.foldersNonRecursive;
    for (index += 1; index < (SE_FS_MAX_FOLDERS + 1); index++)
    {
        if (se_bm_get(bitMask, index)) break;
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
    const SeFileSystemFolder& folder = _se_fs_from_handle(folderHandle);
    const SeFileMask& bitMask = isRecursive ? folder.filesRecursive : folder.filesNonRecursive;
    for (index += 1; index < (SE_FS_MAX_FILES + 1); index++)
    {
        if (se_bm_get(bitMask, index)) break;
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
