#ifndef _SE_STACK_ALLOCATOR_SUBSYSTEM_H_
#define _SE_STACK_ALLOCATOR_SUBSYSTEM_H_

#include <inttypes.h>

struct SeStackAllocator
{
    intptr_t base;      // actual pointer
    size_t cur;         // offset form base
    size_t reservedMax; // offset form base
    size_t commitedMax; // offset form base
    void* (*alloc)(struct SeStackAllocator* allocator, size_t sizeBytes, size_t alignment, const char* allocTag);
    void  (*reset)(struct SeStackAllocator* allocator);
};

struct SeStackAllocatorSubsystemInterface
{
    static constexpr const char* NAME = "SeStackAllocatorSubsystemInterface";

    void (*construct)(SeStackAllocator* allocator, size_t capacity);
    void (*destroy)(SeStackAllocator* allocator);
    void (*to_allocator_bindings)(SeStackAllocator* allocator, struct SeAllocatorBindings* bindings);
};

struct SeStackAllocatorSubsystem
{
    using Interface = SeStackAllocatorSubsystemInterface;
    static constexpr const char* NAME = "se_stack_allocator_subsystem";
};

#endif
