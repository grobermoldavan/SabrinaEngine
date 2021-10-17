#ifndef _SE_WINDOW_SUBSYSTEM_H_
#define _SE_WINDOW_SUBSYSTEM_H_

#include "engine/common_includes.h"

#define SE_WINDOW_SUBSYSTEM_NAME "se_window_subsystem"

SE_DLL_EXPORT void  se_init(struct SabrinaEngine*);
SE_DLL_EXPORT void  se_terminate(struct SabrinaEngine*);
SE_DLL_EXPORT void  se_update(struct SabrinaEngine*);
SE_DLL_EXPORT void* se_get_interface(struct SabrinaEngine*);

struct SeWindowSubsystemCreateInfo
{
    const char* name;
    bool isFullscreen;
    bool isResizable;
    uint32_t width;
    uint32_t height;
};

struct SeWindowSubsystemInterface
{
    void* (*create)(struct SeAllocatorBindings* allocator, struct SeWindowSubsystemCreateInfo* createInfo);
    void  (*destroy)(void* handle);
};

#endif
