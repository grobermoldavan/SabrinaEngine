
#include "se_platform.hpp"
#include "engine/engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

HANDLE se_platform_handle_from_se_handle(SeFileHandle handle)
{
    HANDLE result;
    memcpy(&result, &handle, sizeof(result));
    return result;
}

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

    SeFile file_load(const char* path, SeFileLoadMode loadMode)
    {
        const HANDLE handle = CreateFileA
        (
            path,
            loadMode == SE_FILE_READ ? GENERIC_READ : GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            loadMode == SE_FILE_READ ? OPEN_EXISTING : CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL /*| FILE_FLAG_WRITE_THROUGH // ? */,
            nullptr
        );
        se_assert_msg(handle != INVALID_HANDLE_VALUE, path);
        SeFileHandle seHandle;
        memcpy(&seHandle, &handle, sizeof(HANDLE));

        const size_t BUFFER_LENGTH = 256;
        char buffer[BUFFER_LENGTH];
        const DWORD result = GetFullPathNameA(path, BUFFER_LENGTH, buffer, nullptr);
        se_assert_msg(result != 0, "GetFullPathNameA failed");
        se_assert_msg(result != BUFFER_LENGTH, "wtf");
        se_assert_msg(result < BUFFER_LENGTH, "Buffer is too small to hold full path name");

        return
        {
            .fullPath   = string::create(buffer, SeStringLifetime::Persistent),
            .handle     = seHandle,
            .loadMode   = loadMode,
            .flags      = SE_FILE_IS_LOADED,
        };
    }

    void file_unload(SeFile* file)
    {
        CloseHandle(se_platform_handle_from_se_handle(file->handle));
        string::destroy(file->fullPath);
    }

    bool file_is_valid(const SeFile* file)
    {
        const HANDLE handle = se_platform_handle_from_se_handle(file->handle);
        return handle != INVALID_HANDLE_VALUE && file->flags & SE_FILE_IS_LOADED;
    }

    SeFileContent file_read(const SeFile* file, AllocatorBindings allocator)
    {
        HANDLE handle = se_platform_handle_from_se_handle(file->handle);
        uint64_t fileSize = 0;
        {
            LARGE_INTEGER size = { };
            const BOOL result = GetFileSizeEx(handle, &size);
            se_assert(result);
            fileSize = size.QuadPart;
        }
        void* buffer = allocator.alloc(allocator.allocator, fileSize + 1, se_default_alignment, se_alloc_tag);
        {
            DWORD bytesRead = 0;
            const BOOL result = ReadFile
            (
                handle,
                buffer,
                (DWORD)fileSize, // @TODO : safe cast
                &bytesRead,
                nullptr
            );
            se_assert(result);
        }
        ((char*)buffer)[fileSize] = 0;
        return
        {
            allocator,
            buffer,
            fileSize,
        };
    }

    void file_free_content(SeFileContent* content)
    {
        content->allocator.dealloc(content->allocator.allocator, content->memory, content->size + 1);
    }

    void file_write(const SeFile* file, const void* data, size_t size)
    {
        DWORD bytesWritten;
        const BOOL result = WriteFile
        (
            se_platform_handle_from_se_handle(file->handle),
            data,
            (DWORD)size, // @TODO : safe cast
            &bytesWritten,
            nullptr
        );
        se_assert(bytesWritten == size);
    }

    SeString get_full_path(const char* path, SeStringLifetime lifetime)
    {
        const size_t BUFFER_LENGTH = 256;
        char buffer[BUFFER_LENGTH];
        const DWORD result = GetFullPathNameA(path, BUFFER_LENGTH, buffer, nullptr);
        se_assert_msg(result != 0, "GetFullPathNameA failed");
        se_assert_msg(result != BUFFER_LENGTH, "wtf");
        se_assert_msg(result < BUFFER_LENGTH, "Buffer is too small to hold full path name");
        return string::create(buffer, lifetime);
    }
} // namespace platform

#else // ifdef _WIN32
#   error Unsupported platform
#endif

