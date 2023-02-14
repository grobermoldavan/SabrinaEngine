#ifndef _SE_PLATFORM_HPP_
#define _SE_PLATFORM_HPP_

#include "engine/se_common_includes.hpp"

enum SeMemoryOrder
{
    SE_RELAXED,
    SE_CONSUME,
    SE_ACQUIRE,
    SE_RELEASE,
    SE_ACQUIRE_RELEASE,
    SE_SEQUENTIALLY_CONSISTENT,
};

namespace platform
{
    size_t          get_mem_page_size       ();
    void*           mem_reserve             (size_t size);
    void*           mem_commit              (void* ptr, size_t size);
    void            mem_release             (void* ptr, size_t size);
    uint64_t        atomic_64_bit_increment (uint64_t* val);
    uint64_t        atomic_64_bit_decrement (uint64_t* val);
    uint64_t        atomic_64_bit_add       (uint64_t* val, uint64_t other);
    uint64_t        atomic_64_bit_load      (const uint64_t* val, SeMemoryOrder memoryOrder);
    uint64_t        atomic_64_bit_store     (uint64_t* val, uint64_t newValue, SeMemoryOrder memoryOrder);
    bool            atomic_64_bit_cas       (uint64_t* atomic, uint64_t* expected, uint64_t newValue, SeMemoryOrder memoryOrder);
    uint32_t        atomic_32_bit_increment (uint32_t* val);
    uint32_t        atomic_32_bit_decrement (uint32_t* val);
    uint32_t        atomic_32_bit_add       (uint32_t* val, uint32_t other);
    uint32_t        atomic_32_bit_load      (const uint32_t* val, SeMemoryOrder memoryOrder);
    uint32_t        atomic_32_bit_store     (uint32_t* val, uint32_t newValue, SeMemoryOrder memoryOrder);
    bool            atomic_32_bit_cas       (uint32_t* atomic, uint32_t* expected, uint32_t newValue, SeMemoryOrder memoryOrder);


    size_t          wchar_to_utf8_required_length   (const wchar_t* source, size_t sourceLength);
    void            wchar_to_utf8                   (const wchar_t* source, size_t sourceLength, char* target, size_t targetLength);
    size_t          utf8_to_wchar_required_length   (const char* source, size_t sourceLength);
    void            utf8_to_wchar                   (const char* source, size_t sourceLength, wchar_t* target, size_t targetLength);
}

#endif
