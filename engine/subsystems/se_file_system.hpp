#ifndef _SE_FILE_SYSTEM_HPP_
#define _SE_FILE_SYSTEM_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_allocator_bindings.hpp"

enum struct SeFsHandleType
{
    FILE,
    FOLDER,
};

template<SeFsHandleType type>
struct SeFsHandle
{
    using NumericType = uint64_t;
    NumericType value;
    inline operator NumericType () const { return value; }
    inline operator bool () const { return value != 0; }
};

using SeFileHandle = SeFsHandle<SeFsHandleType::FILE>;
using SeFolderHandle = SeFsHandle<SeFsHandleType::FOLDER>;

struct SeFileContent
{
    AllocatorBindings bindings;
    void* data;
    size_t dataSize;
};

constexpr SeFolderHandle SE_ROOT_FOLDER = { 1 };

namespace fs
{
    SeFileHandle    file_find(const char* fileName, SeFolderHandle handle = SE_ROOT_FOLDER);
    SeFolderHandle  folder_find(const char* folderName, SeFolderHandle handle = SE_ROOT_FOLDER);

    SeFileHandle    file_find_recursive(const char* fileName, SeFolderHandle handle = SE_ROOT_FOLDER);
    SeFolderHandle  folder_find_recursive(const char* folderName, SeFolderHandle handle = SE_ROOT_FOLDER);

    SeFolderHandle  file_get_folder(SeFileHandle handle);
    const char*     file_extension(SeFileHandle handle);

    SeFileContent   file_load(SeFileHandle handle, const AllocatorBindings& bindings);
    void            file_free(SeFileContent& content);

    const char*     full_path(SeFileHandle handle);
    const char*     full_path(SeFolderHandle handle);

    template<typename ... Args> SeString path(const Args& ... args);

    namespace engine
    {
        void init();
        void terminate();
    }
}

#endif
