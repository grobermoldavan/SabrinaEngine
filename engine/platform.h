#ifndef _SE_PLATFORM_H_
#define _SE_PLATFORM_H_

#include <inttypes.h>
#include <stdbool.h>

#include "allocator_bindings.h"

struct SeHandle
{
    uint64_t value;
};

enum SeMemoryOrder
{
    SE_RELAXED,
    SE_CONSUME,
    SE_ACQUIRE,
    SE_RELEASE,
    SE_ACQUIRE_RELEASE,
    SE_SEQUENTIALLY_CONSISTENT,
};

enum SeFileLoadMode
{
    SE_FILE_READ,
    SE_FILE_WRITE,
};

enum SeFileFlags
{
    SE_FILE_IS_LOADED = 0x00000001,
    SE_FILE_STD_HANDLE = 0x00000002,
};

struct SeFile
{
    struct SeHandle handle;
    uint32_t loadMode;
    uint32_t flags;
};

struct SeFileContent
{
    struct SeAllocatorBindings allocator;
    void* memory;
    size_t size;
};

struct SePlatformInterface
{
    struct SeHandle (*load_dynamic_library)(const char* name);
    void            (*unload_dynamic_library)(struct SeHandle handle);
    void*           (*get_dynamic_library_function_address)(struct SeHandle handle, const char* functionName);
    size_t          (*get_mem_page_size)();
    void*           (*mem_reserve)(size_t size);
    void*           (*mem_commit)(void* ptr, size_t size);
    void            (*mem_release)(void* ptr, size_t size);

    uint64_t (*atomic_64_bit_increment) (uint64_t* val);
    uint64_t (*atomic_64_bit_decrement) (uint64_t* val);
    uint64_t (*atomic_64_bit_add)       (uint64_t* val, uint64_t other);
    uint64_t (*atomic_64_bit_load)      (uint64_t* val, enum SeMemoryOrder memoryOrder);
    uint64_t (*atomic_64_bit_store)     (uint64_t* val, uint64_t newValue, enum SeMemoryOrder memoryOrder);
    bool     (*atomic_64_bit_cas)       (uint64_t* atomic, uint64_t* expected, uint64_t newValue, enum SeMemoryOrder memoryOrder);

    uint32_t (*atomic_32_bit_increment) (uint32_t* val);
    uint32_t (*atomic_32_bit_decrement) (uint32_t* val);
    uint32_t (*atomic_32_bit_add)       (uint32_t* val, uint32_t other);
    uint32_t (*atomic_32_bit_load)      (uint32_t* val, enum SeMemoryOrder memoryOrder);
    uint32_t (*atomic_32_bit_store)     (uint32_t* val, uint32_t newValue, enum SeMemoryOrder memoryOrder);
    bool     (*atomic_32_bit_cas)       (uint32_t* atomic, uint32_t* expected, uint32_t newValue, enum SeMemoryOrder memoryOrder);

    void (*file_get_std_out)    (struct SeFile* file);
    void (*file_load)           (struct SeFile* file, const char* path, enum SeFileLoadMode loadMode);
    void (*file_unload)         (struct SeFile* file);
    bool (*file_is_valid)       (struct SeFile* file);
    void (*file_read)           (struct SeFileContent* content, struct SeFile* file, struct SeAllocatorBindings* allocator);
    void (*file_free_content)   (struct SeFileContent* content);
    void (*file_write)          (struct SeFile* file, const void* data, size_t size);
};

void se_get_platform_interface(struct SePlatformInterface* iface);

#endif
