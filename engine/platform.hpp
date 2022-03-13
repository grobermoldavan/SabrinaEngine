#ifndef _SE_PLATFORM_H_
#define _SE_PLATFORM_H_

#include <inttypes.h>
#include <stdbool.h>
#include "allocator_bindings.hpp"
#include "debug.hpp"

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
    SeHandle    (*dynamic_library_load)                 (const char** possibleLibPaths, size_t numPossibleLibPaths);
    void        (*dynamic_library_unload)               (SeHandle handle);
    void*       (*dynamic_library_get_function_address) (SeHandle handle, const char* functionName);
    size_t      (*get_mem_page_size)                    ();
    void*       (*mem_reserve)                          (size_t size);
    void*       (*mem_commit)                           (void* ptr, size_t size);
    void        (*mem_release)                          (void* ptr, size_t size);

    uint64_t    (*atomic_64_bit_increment)  (uint64_t* val);
    uint64_t    (*atomic_64_bit_decrement)  (uint64_t* val);
    uint64_t    (*atomic_64_bit_add)        (uint64_t* val, uint64_t other);
    uint64_t    (*atomic_64_bit_load)       (uint64_t* val, SeMemoryOrder memoryOrder);
    uint64_t    (*atomic_64_bit_store)      (uint64_t* val, uint64_t newValue, SeMemoryOrder memoryOrder);
    bool        (*atomic_64_bit_cas)        (uint64_t* atomic, uint64_t* expected, uint64_t newValue, SeMemoryOrder memoryOrder);

    uint32_t    (*atomic_32_bit_increment)  (uint32_t* val);
    uint32_t    (*atomic_32_bit_decrement)  (uint32_t* val);
    uint32_t    (*atomic_32_bit_add)        (uint32_t* val, uint32_t other);
    uint32_t    (*atomic_32_bit_load)       (uint32_t* val, SeMemoryOrder memoryOrder);
    uint32_t    (*atomic_32_bit_store)      (uint32_t* val, uint32_t newValue, SeMemoryOrder memoryOrder);
    bool        (*atomic_32_bit_cas)        (uint32_t* atomic, uint32_t* expected, uint32_t newValue, SeMemoryOrder memoryOrder);   

    void        (*file_get_std_out)         (SeFile* file);
    void        (*file_load)                (SeFile* file, const char* path, SeFileLoadMode loadMode);
    void        (*file_unload)              (SeFile* file);
    bool        (*file_is_valid)            (SeFile* file);
    void        (*file_read)                (SeFileContent* content, SeFile* file, SeAllocatorBindings* allocator);
    void        (*file_free_content)        (SeFileContent* content);
    void        (*file_write)               (SeFile* file, const void* data, size_t size);

    uint64_t    (*get_perf_counter)         ();
    uint64_t    (*get_perf_frequency)       ();
};

void se_get_platform_interface(SePlatformInterface* iface);

#endif
