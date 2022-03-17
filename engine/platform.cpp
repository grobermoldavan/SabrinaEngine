
#include "platform.hpp"

#ifdef _WIN32

#include <Windows.h>
#include <string.h>
#include "debug.hpp"

static HMODULE se_platform_hmodule_from_se_handle(SeHandle handle)
{
    HMODULE result = {0};
    memcpy(&result, &handle, sizeof(result));
    return result;
}

static HANDLE se_platform_handle_from_se_handle(SeHandle handle)
{
    HANDLE result = {0};
    memcpy(&result, &handle, sizeof(result));
    return result;
}

static SeHandle se_platform_load_dynamic_library(const char** possibleLibPaths, size_t numPossibleLibPaths)
{
    HMODULE handle = nullptr;
    for (size_t it = 0; it < numPossibleLibPaths; it++)
    {
        handle = LoadLibraryA(possibleLibPaths[it]);
        if (handle) break;
    }
    se_assert_msg(handle, possibleLibPaths[0]);
    SeHandle result = {0};
    memcpy(&result, &handle, sizeof(handle));
    return result;
}

static void se_platform_unload_dynamic_library(SeHandle handle)
{
    HMODULE module = se_platform_hmodule_from_se_handle(handle);
    FreeLibrary(module);
}

static void* se_platform_get_dynamic_library_function_address(SeHandle handle, const char* functionName)
{
    HMODULE module = se_platform_hmodule_from_se_handle(handle);
    return GetProcAddress(module, functionName);
}

static size_t se_platform_get_mem_page_size()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    return sysInfo.dwPageSize;
}

static void* se_platform_mem_reserve(size_t size)
{
    void* res = VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
    se_assert(res);
    return res;
}

static void* se_platform_mem_commit(void* ptr, size_t size)
{
    se_assert((size % se_platform_get_mem_page_size()) == 0);
    void* res = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    se_assert(res);
    return res;
}

static void se_platform_mem_release(void* ptr, size_t size)
{
    VirtualFree(ptr, /*size*/ 0, MEM_RELEASE);
}

uint64_t se_platform_atomic_64_bit_increment(uint64_t* val)
{
    return InterlockedIncrement64((LONGLONG*)val);
}

uint64_t se_platform_atomic_64_bit_decrement(uint64_t* val)
{
    return InterlockedDecrement64((LONGLONG*)val);
}

uint64_t se_platform_atomic_64_bit_add(uint64_t* val, uint64_t other)
{
    return InterlockedAdd64((LONGLONG*)val, other);
}

uint64_t se_platform_atomic_64_bit_load(uint64_t* val, SeMemoryOrder memoryOrder)
{
    se_assert
    (
        memoryOrder != SE_RELAXED &&
        memoryOrder != SE_CONSUME &&
        memoryOrder != SE_ACQUIRE &&
        memoryOrder != SE_SEQUENTIALLY_CONSISTENT
    );
    uint64_t loaded = *val;
    if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
    return loaded;
}

uint64_t se_platform_atomic_64_bit_store(uint64_t* val, uint64_t newValue, SeMemoryOrder memoryOrder)
{
    se_assert
    (
        memoryOrder != SE_RELAXED &&
        memoryOrder != SE_RELEASE &&
        memoryOrder != SE_SEQUENTIALLY_CONSISTENT
    );
    *val = newValue;
    if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
    return newValue;
}

bool se_platform_atomic_64_bit_cas(uint64_t* atomic, uint64_t* expected, uint64_t newValue, SeMemoryOrder memoryOrder)
{
    uint64_t val = *expected;
    uint64_t casResult = InterlockedCompareExchange64((LONGLONG*)atomic, newValue, *expected);
    *expected = casResult;
    return val == *expected;
}

uint32_t se_platform_atomic_32_bit_increment(uint32_t* val)
{
    return InterlockedIncrement(val);
}

uint32_t se_platform_atomic_32_bit_decrement(uint32_t* val)
{
    return InterlockedDecrement(val);
}

uint32_t se_platform_atomic_32_bit_add(uint32_t* val, uint32_t other)
{
    return InterlockedAdd((LONG*)val, other);
}

uint32_t se_platform_atomic_32_bit_load(uint32_t* val, SeMemoryOrder memoryOrder)
{
    se_assert
    (
        memoryOrder != SE_RELAXED &&
        memoryOrder != SE_CONSUME &&
        memoryOrder != SE_ACQUIRE &&
        memoryOrder != SE_SEQUENTIALLY_CONSISTENT
    );
    uint32_t loaded = *val;
    if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
    return loaded;
}

uint32_t se_platform_atomic_32_bit_store(uint32_t* val, uint32_t newValue, SeMemoryOrder memoryOrder)
{
    se_assert
    (
        memoryOrder != SE_RELAXED &&
        memoryOrder != SE_RELEASE &&
        memoryOrder != SE_SEQUENTIALLY_CONSISTENT
    );
    *val = newValue;
    if (memoryOrder == SE_SEQUENTIALLY_CONSISTENT) MemoryBarrier();
    return newValue;
}

bool se_platform_atomic_32_bit_cas(uint32_t* atomic, uint32_t* expected, uint32_t newValue, SeMemoryOrder memoryOrder)
{
    uint32_t val = *expected;
    uint32_t casResult = InterlockedCompareExchange(atomic, newValue, *expected);
    *expected = casResult;
    return val == *expected;
}

void se_platform_file_get_std_out(SeFile* file)
{
    memset(file, 0, sizeof(SeFile));
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    file->loadMode = SE_FILE_WRITE;
    file->flags = SE_FILE_STD_HANDLE;
    memcpy(&file->handle, &handle, sizeof(HANDLE));
}

void se_platform_file_load(SeFile* file, const char* path, SeFileLoadMode loadMode)
{
    memset(file, 0, sizeof(SeFile));
    file->loadMode = loadMode;
    HANDLE handle = CreateFileA
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
    file->flags = SE_FILE_IS_LOADED;
    memcpy(&file->handle, &handle, sizeof(HANDLE));
}

void se_platform_file_unload(SeFile* file)
{
    CloseHandle(se_platform_handle_from_se_handle(file->handle));
}

bool se_platform_file_is_valid(SeFile* file)
{
    HANDLE handle = se_platform_handle_from_se_handle(file->handle);
    return handle != INVALID_HANDLE_VALUE && file->flags & SE_FILE_IS_LOADED;
}

void se_platform_file_read(SeFileContent* content, SeFile* file, SeAllocatorBindings* allocator)
{
    HANDLE handle = se_platform_handle_from_se_handle(file->handle);
    uint64_t fileSize = 0;
    {
        LARGE_INTEGER size = {0};
        const BOOL result = GetFileSizeEx(handle, &size);
        se_assert(result);
        fileSize = size.QuadPart;
    }
    void* buffer = allocator->alloc(allocator->allocator, fileSize + 1, se_default_alignment, se_alloc_tag);
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
    content->allocator = *allocator;
    content->memory = buffer;
    content->size = fileSize;
}

void se_platform_file_free_content(SeFileContent* content)
{
    content->allocator.dealloc(content->allocator.allocator, content->memory, content->size + 1);
}

void se_platform_file_write(SeFile* file, const void* data, size_t size)
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

uint64_t se_platform_get_perf_counter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

uint64_t se_platform_get_perf_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

#else // ifdef _WIN32
#   error Unsupported platform
#endif

void se_get_platform_interface(SePlatformInterface* iface)
{
    *iface =
    {
        .dynamic_library_load                   = se_platform_load_dynamic_library,
        .dynamic_library_unload                 = se_platform_unload_dynamic_library,
        .dynamic_library_get_function_address   = se_platform_get_dynamic_library_function_address,
        .get_mem_page_size                      = se_platform_get_mem_page_size,
        .mem_reserve                            = se_platform_mem_reserve,
        .mem_commit                             = se_platform_mem_commit,
        .mem_release                            = se_platform_mem_release,
        .atomic_64_bit_increment                = se_platform_atomic_64_bit_increment,
        .atomic_64_bit_decrement                = se_platform_atomic_64_bit_decrement,
        .atomic_64_bit_add                      = se_platform_atomic_64_bit_add,
        .atomic_64_bit_load                     = se_platform_atomic_64_bit_load,
        .atomic_64_bit_store                    = se_platform_atomic_64_bit_store,
        .atomic_64_bit_cas                      = se_platform_atomic_64_bit_cas,
        .atomic_32_bit_increment                = se_platform_atomic_32_bit_increment,
        .atomic_32_bit_decrement                = se_platform_atomic_32_bit_decrement,
        .atomic_32_bit_add                      = se_platform_atomic_32_bit_add,
        .atomic_32_bit_load                     = se_platform_atomic_32_bit_load,
        .atomic_32_bit_store                    = se_platform_atomic_32_bit_store,
        .atomic_32_bit_cas                      = se_platform_atomic_32_bit_cas,
        .file_get_std_out                       = se_platform_file_get_std_out,
        .file_load                              = se_platform_file_load,
        .file_unload                            = se_platform_file_unload,
        .file_is_valid                          = se_platform_file_is_valid,
        .file_read                              = se_platform_file_read,
        .file_free_content                      = se_platform_file_free_content,
        .file_write                             = se_platform_file_write,
        .get_perf_counter                       = se_platform_get_perf_counter,
        .get_perf_frequency                     = se_platform_get_perf_frequency,
    };
}
