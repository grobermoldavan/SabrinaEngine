#ifndef _SE_STACK_ALLOCATOR_SUBSYSTEM_H_
#define _SE_STACK_ALLOCATOR_SUBSYSTEM_H_

#include <inttypes.h>

#define SE_STACK_ALLOCATOR_SUBSYSTEM_NAME "se_stack_allocator_subsystem"

typedef struct SeStackAllocator
{
    struct SePlatformInterface* platformIface;
    intptr_t base;      // actual pointer
    size_t cur;         // offset form base
    size_t reservedMax; // offset form base
    size_t commitedMax; // offset form base
    void* (*alloc)(void* allocator, size_t sizeBytes, size_t alignment);
    void  (*reset)(void* allocator);
} SeStackAllocator;

typedef struct SeStackAllocatorSubsystemInterface
{
    void (*construct_allocator)(struct SePlatformInterface* platformIface, SeStackAllocator* allocator, size_t capacity);
    void (*destruct_allocator)(SeStackAllocator* allocator);
    void (*to_allocator_bindings)(SeStackAllocator* allocator, struct SeAllocatorBindings* bindings);
} SeStackAllocatorSubsystemInterface;

#endif
