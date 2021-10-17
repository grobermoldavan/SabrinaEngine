#ifndef _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_
#define _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_

#include "engine/common_includes.h"

#define SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME "se_application_allocators_subsystems"

SE_DLL_EXPORT void  se_init(struct SabrinaEngine*);
SE_DLL_EXPORT void  se_terminate(struct SabrinaEngine*);
SE_DLL_EXPORT void  se_update(struct SabrinaEngine*);
SE_DLL_EXPORT void* se_get_interface(struct SabrinaEngine*);

struct SeApplicationAllocatorsSubsystemInterface
{
    struct SeAllocatorBindings* frameAllocator;
    struct SeAllocatorBindings* sceneAllocator;
    struct SeAllocatorBindings* appAllocator;
};

#endif
