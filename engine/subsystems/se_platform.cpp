
#include "se_platform.hpp"
#include "engine/se_engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

inline size_t se_platform_get_mem_page_size()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwPageSize;
}

inline void* se_platform_mem_reserve(size_t size)
{
    void* res = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
    se_assert(res);
    return res;
}

inline void* se_platform_mem_commit(void* ptr, size_t size)
{
    se_assert((size % se_platform_get_mem_page_size()) == 0);
    void* res = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    se_assert(res);
    return res;
}

inline void se_platform_mem_release(void* ptr, size_t size)
{
    VirtualFree(ptr, /*size*/ 0, MEM_RELEASE);
}

inline uint64_t se_platform_atomic_64_bit_increment(uint64_t* val)
{
    return InterlockedIncrement64((LONGLONG*)val);
}

inline uint64_t se_platform_atomic_64_bit_decrement(uint64_t* val)
{
    return InterlockedDecrement64((LONGLONG*)val);
}

inline uint64_t se_platform_atomic_64_bit_add(uint64_t* val, uint64_t other)
{
    return InterlockedAdd64((LONGLONG*)val, other);
}

inline uint64_t se_platform_atomic_64_bit_load(const uint64_t* val, SeMemoryOrder memoryOrder)
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

inline uint64_t se_platform_atomic_64_bit_store(uint64_t* val, uint64_t newValue, SeMemoryOrder memoryOrder)
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

inline bool se_platform_atomic_64_bit_cas(uint64_t* atomic, uint64_t* expected, uint64_t newValue, SeMemoryOrder memoryOrder)
{
    const uint64_t val = *expected;
    const uint64_t casResult = InterlockedCompareExchange64((LONGLONG*)atomic, newValue, *expected);
    *expected = casResult;
    return val == *expected;
}

inline uint32_t se_platform_atomic_32_bit_increment(uint32_t* val)
{
    return InterlockedIncrement(val);
}

inline uint32_t se_platform_atomic_32_bit_decrement(uint32_t* val)
{
    return InterlockedDecrement(val);
}

inline uint32_t se_platform_atomic_32_bit_add(uint32_t* val, uint32_t other)
{
    return InterlockedAdd((LONG*)val, other);
}

inline uint32_t se_platform_atomic_32_bit_load(const uint32_t* val, SeMemoryOrder memoryOrder)
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

inline uint32_t se_platform_atomic_32_bit_store(uint32_t* val, uint32_t newValue, SeMemoryOrder memoryOrder)
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

inline bool se_platform_atomic_32_bit_cas(uint32_t* atomic, uint32_t* expected, uint32_t newValue, SeMemoryOrder memoryOrder)
{
    const uint32_t val = *expected;
    const uint32_t casResult = InterlockedCompareExchange(atomic, newValue, *expected);
    *expected = casResult;
    return val == *expected;
}

inline size_t se_platform_wchar_to_utf8_required_length(const wchar_t* source, size_t sourceLength)
{
    const int requiredLength = WideCharToMultiByte(CP_UTF8, 0, source, int(sourceLength), NULL, 0, NULL, NULL);
    return size_t(requiredLength);
}

inline void se_platform_wchar_to_utf8(const wchar_t* source, size_t sourceLength, char* target, size_t targetLength)
{
    const int converisonResult = WideCharToMultiByte(CP_UTF8, 0, source, int(sourceLength), target, int(targetLength), NULL, NULL);
    se_assert_msg(converisonResult, "Unable to convert string from utf16 to utf8. Error code is : {}", GetLastError());
}

inline size_t se_platform_utf8_to_wchar_required_length(const char* source, size_t sourceLength)
{
    const int requiredLength = MultiByteToWideChar(CP_UTF8, 0, source, int(sourceLength), NULL, 0);
    return size_t(requiredLength);
}

inline void se_platform_utf8_to_wchar(const char* source, size_t sourceLength, wchar_t* target, size_t targetLength)
{
    const int converisonResult = MultiByteToWideChar(CP_UTF8, 0, source, int(sourceLength), target, int(targetLength));
    se_assert_msg(converisonResult, "Unable to convert string from utf8 to utf16. Error code is : {}", GetLastError());
}

#else // ifdef _WIN32
#   error Unsupported platform
#endif

