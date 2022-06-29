#ifndef _SE_PLATFORM_H_
#define _SE_PLATFORM_H_

#include "engine/subsystems/se_string_subsystem.hpp"
#include "engine/common_includes.hpp"
#include "engine/allocator_bindings.hpp"

enum SeMemoryOrder
{
    SE_RELAXED,
    SE_CONSUME,
    SE_ACQUIRE,
    SE_RELEASE,
    SE_ACQUIRE_RELEASE,
    SE_SEQUENTIALLY_CONSISTENT,
};

enum SeFileLoadMode : uint32_t
{
    SE_FILE_READ,
    SE_FILE_WRITE,
};

enum SeFileFlags : uint32_t
{
    SE_FILE_IS_LOADED = 0x00000001,
    SE_FILE_STD_HANDLE = 0x00000002,
};

using SeFileHandle = uint64_t;

struct SeFile
{
    SeString        fullPath;
    SeFileHandle    handle;
    uint32_t        loadMode;
    uint32_t        flags;
};

struct SeFileContent
{
    AllocatorBindings   allocator;
    void*               memory;
    size_t              size;
};

struct SePlatformSubsystemInterface
{
    static constexpr const char* NAME = "SePlatformSubsystemInterface";

    size_t          (*get_mem_page_size)        ();
    void*           (*mem_reserve)              (size_t size);
    void*           (*mem_commit)               (void* ptr, size_t size);
    void            (*mem_release)              (void* ptr, size_t size);

    uint64_t        (*atomic_64_bit_increment)  (uint64_t* val);
    uint64_t        (*atomic_64_bit_decrement)  (uint64_t* val);
    uint64_t        (*atomic_64_bit_add)        (uint64_t* val, uint64_t other);
    uint64_t        (*atomic_64_bit_load)       (const uint64_t* val, SeMemoryOrder memoryOrder);
    uint64_t        (*atomic_64_bit_store)      (uint64_t* val, uint64_t newValue, SeMemoryOrder memoryOrder);
    bool            (*atomic_64_bit_cas)        (uint64_t* atomic, uint64_t* expected, uint64_t newValue, SeMemoryOrder memoryOrder);

    uint32_t        (*atomic_32_bit_increment)  (uint32_t* val);
    uint32_t        (*atomic_32_bit_decrement)  (uint32_t* val);
    uint32_t        (*atomic_32_bit_add)        (uint32_t* val, uint32_t other);
    uint32_t        (*atomic_32_bit_load)       (const uint32_t* val, SeMemoryOrder memoryOrder);
    uint32_t        (*atomic_32_bit_store)      (uint32_t* val, uint32_t newValue, SeMemoryOrder memoryOrder);
    bool            (*atomic_32_bit_cas)        (uint32_t* atomic, uint32_t* expected, uint32_t newValue, SeMemoryOrder memoryOrder);   

    SeFile          (*file_load)                (const char* path, SeFileLoadMode loadMode);
    void            (*file_unload)              (SeFile* file);
    bool            (*file_is_valid)            (const SeFile* file);
    SeFileContent   (*file_read)                (const SeFile* file, AllocatorBindings allocator);
    void            (*file_free_content)        (SeFileContent* content);
    void            (*file_write)               (const SeFile* file, const void* data, size_t size);
    SeString        (*get_full_path)            (const char* path, SeStringLifetime lifetime);
};

struct SePlatformSubsystem
{
    using Interface = SePlatformSubsystemInterface;
    static constexpr const char* NAME = "se_platform_subsystem";
};

#define SE_PLATFORM_SUBSYSTEM_GLOBAL_NAME g_platformIface
const SePlatformSubsystemInterface* SE_PLATFORM_SUBSYSTEM_GLOBAL_NAME;

namespace platform
{
    inline const SePlatformSubsystemInterface* get()
    {
        return SE_PLATFORM_SUBSYSTEM_GLOBAL_NAME;
    }
}

#endif
