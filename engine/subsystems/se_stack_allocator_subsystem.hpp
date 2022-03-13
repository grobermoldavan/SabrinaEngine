#ifndef _SE_STACK_ALLOCATOR_SUBSYSTEM_H_
#define _SE_STACK_ALLOCATOR_SUBSYSTEM_H_

#include <inttypes.h>

#define SE_STACK_ALLOCATOR_SUBSYSTEM_NAME "se_stack_allocator_subsystem"

struct SeStackAllocator
{
    struct SePlatformInterface* platformIface;
    intptr_t base;      // actual pointer
    size_t cur;         // offset form base
    size_t reservedMax; // offset form base
    size_t commitedMax; // offset form base
    void* (*alloc)(struct SeStackAllocator* allocator, size_t sizeBytes, size_t alignment, const char* allocTag);
    void  (*reset)(struct SeStackAllocator* allocator);
};

struct SeStackAllocatorSubsystemInterface
{
    void (*construct)(struct SePlatformInterface* platformIface, SeStackAllocator* allocator, size_t capacity);
    void (*destroy)(SeStackAllocator* allocator);
    void (*to_allocator_bindings)(SeStackAllocator* allocator, struct SeAllocatorBindings* bindings);
};

#endif
