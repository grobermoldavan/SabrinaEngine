#ifndef _SE_PLATFORM_H_
#define _SE_PLATFORM_H_

#include "engine/common_includes.h"

struct SeHandle
{
    uint64_t value;
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
};

void se_get_platform_interface(struct SePlatformInterface* iface);

#endif
