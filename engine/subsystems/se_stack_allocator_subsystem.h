#ifndef _SE_STACK_ALLOCATOR_SUBSYSTEM_H_
#define _SE_STACK_ALLOCATOR_SUBSYSTEM_H_

#include "engine/common_includes.h"

#define SE_STACK_ALLOCATOR_SUBSYSTEM_NAME "se_stack_allocator_subsystem"

SE_DLL_EXPORT void  se_init(struct SabrinaEngine*);
SE_DLL_EXPORT void* se_get_interface(struct SabrinaEngine*);

struct SeStackAllocator
{
    struct SePlatformInterface* platformIface;
    intptr_t base;      // actual pointer
    size_t cur;         // offset form base
    size_t reservedMax; // offset form base
    size_t commitedMax; // offset form base
    void* (*alloc)(void* allocator, size_t sizeBytes, size_t alignment);
    void  (*reset)(void* allocator);
};

struct SeStackAllocatorSubsystemInterface
{
    void (*construct_allocator)(struct SePlatformInterface* platformIface, struct SeStackAllocator* allocator, size_t capacity);
    void (*destruct_allocator)(struct SeStackAllocator* allocator);
    void (*to_allocator_bindings)(struct SeStackAllocator* allocator, struct SeAllocatorBindings* bindings);
};

#endif
