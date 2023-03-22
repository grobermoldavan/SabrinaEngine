#ifndef _SE_FILE_SYSTEM_HPP_
#define _SE_FILE_SYSTEM_HPP_

/*
    Implementation notes

    File system gives access to two main folders - application (executable) folder and user data folder
    Application folder is read-only and is used to store application assets
    User data folder has read-write access and can be used to store save files, logs and other stuff

    Application folder is referred via SE_APPLICATION_FOLDER constant
    User data folder is referred via SE_USER_DATA_FOLDER constant

    file_create and folder_create functions create respective objects in user data folder
    It's forbiddent to create folders and files in application folder
*/

#include "engine/se_common_includes.hpp"
#include "engine/se_allocator_bindings.hpp"
#include "engine/se_data_providers.hpp"

#include "file_system/se_file_system_data_types.hpp"

constexpr SeFolderHandle SE_APPLICATION_FOLDER = { 1 };
constexpr SeFolderHandle SE_USER_DATA_FOLDER = { 2 };

SeFileHandle    se_fs_file_find(const char* filePath, SeFolderHandle handle = SE_APPLICATION_FOLDER);
SeFolderHandle  se_fs_folder_find(const char* folderPath, SeFolderHandle handle = SE_APPLICATION_FOLDER);
SeFileHandle    se_fs_file_find_recursive(const char* filePath, SeFolderHandle handle = SE_APPLICATION_FOLDER);
SeFolderHandle  se_fs_folder_find_recursive(const char* folderPath, SeFolderHandle handle = SE_APPLICATION_FOLDER);

SeFolderHandle  se_fs_file_get_folder(SeFileHandle handle);

SeFileContent   se_fs_file_read(SeFileHandle handle, const SeAllocatorBindings& bindings);
void            se_fs_file_content_free(SeFileContent& content);

void            se_fs_file_write_begin(SeFileHandle handle, SeFileWriteOpenMode openMode);
void            se_fs_file_write(SeFileHandle handle, SeDataProvider data);
void            se_fs_file_write_end(SeFileHandle handle);

SeFileHandle    se_fs_file_create(const char* filePath, SeFolderHandle handle = SE_USER_DATA_FOLDER);
SeFolderHandle  se_fs_folder_create(const char* folderPath, SeFolderHandle handle = SE_USER_DATA_FOLDER);

SeFolderHandle  se_fs_parent_folder(SeFolderHandle handle);
SeFolderHandle  se_fs_parent_folder(SeFileHandle handle);

const char*     se_fs_full_path(SeFileHandle handle);
const char*     se_fs_full_path(SeFolderHandle handle);

const char*     se_fs_extension(SeFileHandle handle);

const char*     se_fs_name(SeFileHandle handle);
const char*     se_fs_name(SeFolderHandle handle);

template<typename ... Args> const char* se_fs_path(const Args& ... args);

void _se_fs_init(const SeSettings& settings);
void _se_fs_terminate();

struct SeFolderIterator
{
    SeFolderHandle folderHandle;
    bool isRecursive;
};

struct SeFileIterator
{
    SeFolderHandle folderHandle;
    bool isRecursive;
};

struct SeFolderIteratorInstance
{
    SeFolderHandle folderHandle;
    size_t index;
    bool isRecursive;

    bool                        operator != (const SeFolderIteratorInstance& other) const;
    SeFolderHandle              operator *  () const;
    SeFolderIteratorInstance&   operator ++ ();
};

struct SeFileIteratorInstance
{
    SeFolderHandle folderHandle;
    size_t index;
    bool isRecursive;

    bool                    operator != (const SeFileIteratorInstance& other) const;
    SeFileHandle            operator *  () const;
    SeFileIteratorInstance& operator ++ ();
};

SeFolderIteratorInstance begin(SeFolderIterator settings);
SeFolderIteratorInstance end(SeFolderIterator settings);

SeFileIteratorInstance begin(SeFileIterator settings);
SeFileIteratorInstance end(SeFileIterator settings);

#endif
