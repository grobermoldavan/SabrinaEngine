
#ifdef _WIN32

#include <Windows.h>

#include "platform.h"

static HMODULE se_platform_hmodule_from_se_handle(struct SeHandle handle)
{
    HMODULE result = {0};
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
    };
}
