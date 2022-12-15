
#include "se_platform.hpp"
#include "engine/engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

namespace platform
{
    size_t get_mem_page_size()
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwPageSize;
    }

    void* mem_reserve(size_t size)
    {
        void* res = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
        se_assert(res);
        return res;
    }

    void* mem_commit(void* ptr, size_t size)
    {
        se_assert((size % get_mem_page_size()) == 0);
        void* res = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
        se_assert(res);
        return res;
    }

    void mem_release(void* ptr, size_t size)
    {
        VirtualFree(ptr, /*size*/ 0, MEM_RELEASE);
    }

    uint64_t atomic_64_bit_increment(uint64_t* val)
    {
        return InterlockedIncrement64((LONGLONG*)val);
    }

    uint64_t atomic_64_bit_decrement(uint64_t* val)
    {
        return InterlockedDecrement64((LONGLONG*)val);
    }

    uint64_t atomic_64_bit_add(uint64_t* val, uint64_t other)
    {
        return InterlockedAdd64((LONGLONG*)val, other);
    }

    uint64_t atomic_64_bit_load(const uint64_t* val, SeMemoryOrder memoryOrder)
    {
        se_assert
        (
            memoryOrder == SE_RELAXED ||
            memoryOrder == SE_CONSUME ||
            memoryOrder == SE_ACQUIRE ||
            memoryOrder == SE_SEQUENTIALLY_CONSISTENT
        );
        const uint64_t loaded = *val;
        if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
        return loaded;
    }

    uint64_t atomic_64_bit_store(uint64_t* val, uint64_t newValue, SeMemoryOrder memoryOrder)
    {
        se_assert
        (
            memoryOrder == SE_RELAXED ||
            memoryOrder == SE_RELEASE ||
            memoryOrder == SE_SEQUENTIALLY_CONSISTENT
        );
        *val = newValue;
        if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
        return newValue;
    }

    bool atomic_64_bit_cas(uint64_t* atomic, uint64_t* expected, uint64_t newValue, SeMemoryOrder memoryOrder)
    {
        const uint64_t val = *expected;
        const uint64_t casResult = InterlockedCompareExchange64((LONGLONG*)atomic, newValue, *expected);
        *expected = casResult;
        return val == *expected;
    }

    uint32_t atomic_32_bit_increment(uint32_t* val)
    {
        return InterlockedIncrement(val);
    }

    uint32_t atomic_32_bit_decrement(uint32_t* val)
    {
        return InterlockedDecrement(val);
    }

    uint32_t atomic_32_bit_add(uint32_t* val, uint32_t other)
    {
        return InterlockedAdd((LONG*)val, other);
    }

    uint32_t atomic_32_bit_load(const uint32_t* val, SeMemoryOrder memoryOrder)
    {
        se_assert
        (
            memoryOrder == SE_RELAXED ||
            memoryOrder == SE_CONSUME ||
            memoryOrder == SE_ACQUIRE ||
            memoryOrder == SE_SEQUENTIALLY_CONSISTENT
        );
        const uint32_t loaded = *val;
        if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
        return loaded;
    }

    uint32_t atomic_32_bit_store(uint32_t* val, uint32_t newValue, SeMemoryOrder memoryOrder)
    {
        se_assert
        (
            memoryOrder == SE_RELAXED ||
            memoryOrder == SE_RELEASE ||
            memoryOrder == SE_SEQUENTIALLY_CONSISTENT
        );
        *val = newValue;
        if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
        return newValue;
    }

    bool atomic_32_bit_cas(uint32_t* atomic, uint32_t* expected, uint32_t newValue, SeMemoryOrder memoryOrder)
    {
        const uint32_t val = *expected;
        const uint32_t casResult = InterlockedCompareExchange(atomic, newValue, *expected);
        *expected = casResult;
        return val == *expected;
    }
} // namespace platform

#else // ifdef _WIN32
#   error Unsupported platform
#endif

