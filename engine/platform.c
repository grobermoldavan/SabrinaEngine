
#ifdef _WIN32

#include <Windows.h>
#include <string.h>

#include "platform.h"
#include "debug.h"

static HMODULE se_platform_hmodule_from_se_handle(struct SeHandle handle)
{
    HMODULE result = {0};
    memcpy(&result, &handle, sizeof(result));
    return result;
}

static HANDLE se_platform_handle_from_se_handle(struct SeHandle handle)
{
    HANDLE result = {0};
    memcpy(&result, &handle, sizeof(result));
    return result;
}

static struct SeHandle se_platform_load_dynamic_library(const char* name)
{
    HMODULE handle = LoadLibraryA(name);
    struct SeHandle result = {0};
    memcpy(&result, &handle, sizeof(handle));
    return result;
}

static void se_platform_unload_dynamic_library(struct SeHandle handle)
{
    HMODULE module = se_platform_hmodule_from_se_handle(handle);
    FreeLibrary(module);
}

static void* se_platform_get_dynamic_library_function_address(struct SeHandle handle, const char* functionName)
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
    void* res = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_READWRITE);
    return res;
}

static void* se_platform_mem_commit(void* ptr, size_t size)
{
    void* res = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
    return res;
}

static void se_platform_mem_release(void* ptr, size_t size)
{
    BOOL res = VirtualFree(ptr, /*size*/ 0, MEM_RELEASE);
    int i = 0;
}

uint64_t se_platform_atomic_64_bit_increment(uint64_t* val)
{
    return InterlockedIncrement64(val);
}

uint64_t se_platform_atomic_64_bit_decrement(uint64_t* val)
{
    return InterlockedDecrement64(val);
}

uint64_t se_platform_atomic_64_bit_add(uint64_t* val, uint64_t other)
{
    return InterlockedAdd64(val, other);
}

uint64_t se_platform_atomic_64_bit_load(uint64_t* val, enum SeMemoryOrder memoryOrder)
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

uint64_t se_platform_atomic_64_bit_store(uint64_t* val, uint64_t newValue, enum SeMemoryOrder memoryOrder)
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

bool se_platform_atomic_64_bit_cas(uint64_t* atomic, uint64_t* expected, uint64_t newValue, enum SeMemoryOrder memoryOrder)
{
    uint64_t val = *expected;
    uint64_t casResult = InterlockedCompareExchange64(atomic, newValue, *expected);
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
    return InterlockedAdd(val, other);
}

uint32_t se_platform_atomic_32_bit_load(uint32_t* val, enum SeMemoryOrder memoryOrder)
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

uint32_t se_platform_atomic_32_bit_store(uint32_t* val, uint32_t newValue, enum SeMemoryOrder memoryOrder)
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

bool se_platform_atomic_32_bit_cas(uint32_t* atomic, uint32_t* expected, uint32_t newValue, enum SeMemoryOrder memoryOrder)
{
    uint32_t val = *expected;
    uint32_t casResult = InterlockedCompareExchange(atomic, newValue, *expected);
    *expected = casResult;
    return val == *expected;
}

void se_platform_file_get_std_out(struct SeFile* file)
{
    memset(file, 0, sizeof(struct SeFile));
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    file->loadMode = SE_FILE_WRITE;
    file->flags = SE_FILE_STD_HANDLE;
    memcpy(&file->handle, &handle, sizeof(HANDLE));
}

void se_platform_file_load(struct SeFile* file, const char* path, enum SeFileLoadMode loadMode)
{
    memset(file, 0, sizeof(struct SeFile));
    file->loadMode = loadMode;
    HANDLE handle = CreateFileA
    (
        path,
        loadMode == SE_FILE_READ ? GENERIC_READ : GENERIC_WRITE,
        FILE_SHARE_READ,
        NULL,
        loadMode == SE_FILE_READ ? OPEN_EXISTING : CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL /*| FILE_FLAG_WRITE_THROUGH // ? */,
        NULL
    );
    se_assert(handle != INVALID_HANDLE_VALUE);
    file->flags = SE_FILE_IS_LOADED;
    memcpy(&file->handle, &handle, sizeof(HANDLE));
}

void se_platform_file_unload(struct SeFile* file)
{
    CloseHandle(se_platform_handle_from_se_handle(file->handle));
}

bool se_platform_file_is_valid(struct SeFile* file)
{
    HANDLE handle = se_platform_handle_from_se_handle(file->handle);
    return handle != INVALID_HANDLE_VALUE && file->flags & SE_FILE_IS_LOADED;
}

void se_platform_file_read(struct SeFileContent* content, struct SeFile* file, struct SeAllocatorBindings* allocator)
{
    HANDLE handle = se_platform_handle_from_se_handle(file->handle);
    uint64_t fileSize = 0;
    {
        LARGE_INTEGER size = {0};
        const BOOL result = GetFileSizeEx(handle, &size);
        se_assert(result);
        fileSize = size.QuadPart;
    }
    void* buffer = allocator->alloc(allocator->allocator, fileSize + 1, se_default_alignment);
    {
        DWORD bytesRead = 0;
        const BOOL result = ReadFile
        (
            handle,
            buffer,
            (DWORD)fileSize, // @TODO : safe cast
            &bytesRead,
            NULL
        );
        se_assert(result);
    }
    ((char*)buffer)[fileSize] = 0;
    content->allocator = *allocator;
    content->memory = buffer;
    content->size = fileSize;
}

void se_platform_file_free_content(struct SeFileContent* content)
{
    content->allocator.dealloc(content->allocator.allocator, content->memory, content->size + 1);
}

void se_platform_file_write(struct SeFile* file, const void* data, size_t size)
{
    DWORD bytesWritten;
    const BOOL result = WriteFile
    (
        se_platform_handle_from_se_handle(file->handle),
        data,
        (DWORD)size, // @TODO : safe cast
        &bytesWritten,
        NULL
    );
    se_assert(bytesWritten == size);
}

#else
#   error Unsupported platform
#endif // ifdef _WIN32

void se_get_platform_interface(struct SePlatformInterface* iface)
{
    *iface = (struct SePlatformInterface)
    {
        .load_dynamic_library                   = se_platform_load_dynamic_library,
        .unload_dynamic_library                 = se_platform_unload_dynamic_library,
        .get_dynamic_library_function_address   = se_platform_get_dynamic_library_function_address,
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
    };
}
