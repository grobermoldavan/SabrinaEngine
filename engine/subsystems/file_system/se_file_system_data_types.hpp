#ifndef _SE_FILE_SYSTEM_DATA_TYPES_HPP_
#define _SE_FILE_SYSTEM_DATA_TYPES_HPP_

#include "engine/se_common_includes.hpp"

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

enum struct SeFileWriteOpenMode
{
    CLEAR,
    LOAD,
};

#endif
