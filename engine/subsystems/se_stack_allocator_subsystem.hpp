#ifndef _SE_STACK_ALLOCATOR_SUBSYSTEM_H_
#define _SE_STACK_ALLOCATOR_SUBSYSTEM_H_

#include <inttypes.h>
#include "engine/allocator_bindings.hpp"

struct SeStackAllocator
{
    intptr_t base;      // actual pointer
    size_t cur;         // offset form base
    size_t reservedMax; // offset form base
    size_t commitedMax; // offset form base
};

struct SeStackAllocatorSubsystemInterface
{
    static constexpr const char* NAME = "SeStackAllocatorSubsystemInterface";

    void                (*construct)            (SeStackAllocator& allocator, size_t capacity);
    void                (*destroy)              (SeStackAllocator& allocator);
    SeAllocatorBindings (*to_allocator_bindings)(SeStackAllocator& allocator);
    void                (*reset)                (SeStackAllocator& allocator);
};

struct SeStackAllocatorSubsystem
{
    using Interface = SeStackAllocatorSubsystemInterface;
    static constexpr const char* NAME = "se_stack_allocator_subsystem";
};

#define SE_STACK_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME g_stackAllocatorIface
const SeStackAllocatorSubsystemInterface* SE_STACK_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME;

namespace stack_allocator
{
    inline void construct(SeStackAllocator& allocator, size_t capacity)
    {
        SE_STACK_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->construct(allocator, capacity);
    }

    inline SeStackAllocator create(size_t capacity)
    {
        SeStackAllocator allocator;
        SE_STACK_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->construct(allocator, capacity);
        return allocator;
    }

    inline void destroy(SeStackAllocator& allocator)
    {
        SE_STACK_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->destroy(allocator);
    }
    
    inline SeAllocatorBindings to_bindings(SeStackAllocator& allocator)
    {
        return SE_STACK_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->to_allocator_bindings(allocator);
    }

    inline void reset(SeStackAllocator& allocator)
    {
        SE_STACK_ALLOCATOR_SUBSYSTEM_GLOBAL_NAME->reset(allocator);
    }
}

#endif
