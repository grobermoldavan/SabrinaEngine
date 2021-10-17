
#include <string.h>

#include "se_stack_allocator_subsystem.h"
#include "engine/engine.h"

void se_stack_construct_allocator(struct SePlatformInterface* platformIface, struct SeStackAllocator* allocator, size_t capacity);
void se_stack_destruct_allocator(struct SeStackAllocator* allocator);
void* se_stack_alloc(struct SeStackAllocator* allocator, size_t size, size_t alignment);
void se_stack_reset(struct SeStackAllocator* allocator);

static struct SeStackAllocatorSubsystemInterface iface;

SE_DLL_EXPORT void se_init(struct SabrinaEngine* engine)
{
    iface = (struct SeStackAllocatorSubsystemInterface)
    {
        .construct_allocator = se_stack_construct_allocator,
        .destruct_allocator = se_stack_destruct_allocator,
    };
}

SE_DLL_EXPORT void* se_get_interface(struct SabrinaEngine* engine)
{
    return &iface;
}

static void se_stack_construct_allocator(struct SePlatformInterface* platformIface, struct SeStackAllocator* allocator, size_t capacity)
{
    *allocator = (struct SeStackAllocator)
    {
        .platformIface  = platformIface,
        .base           = (intptr_t)platformIface->mem_reserve(capacity),
        .cur            = 0,
        .reservedMax    = capacity,
        .commitedMax    = 0,
        .alloc          = se_stack_alloc,
        .reset          = se_stack_reset,
    };
}

static void se_stack_destruct_allocator(struct SeStackAllocator* allocator)
{
    allocator->platformIface->mem_release((void*)allocator->base, allocator->reservedMax);
    memset(allocator, 0, sizeof(struct SeStackAllocator));
}

static void se_stack_to_allocator_bindings(struct SeStackAllocator* allocator, struct SeAllocatorBindings* bindings)
{
    *bindings = (struct SeAllocatorBindings)
    {
        .allocator = allocator,
        .alloc = allocator->alloc,
        .dealloc = NULL,
    };
}

static void* se_stack_alloc(struct SeStackAllocator* allocator, size_t size, size_t alignment)
{
    // Align pointer
    const intptr_t stackTopPtr = allocator->base + allocator->cur;
    size_t additionalAlignment = (size_t)(stackTopPtr & (alignment - 1));
    if (additionalAlignment != 0) additionalAlignment = alignment - additionalAlignment;
    const intptr_t alignedPtr = stackTopPtr + additionalAlignment;
    const size_t alignedAllocationSize = size + additionalAlignment;
    // Try allocate
    if ((allocator->cur + alignedAllocationSize) > allocator->reservedMax)
    {
        return NULL;
    }
    if ((allocator->cur + alignedAllocationSize) > allocator->commitedMax)
    {
        const intptr_t allocationSize = alignedAllocationSize - (allocator->commitedMax - allocator->cur);
        const size_t pageSize = allocator->platformIface->get_mem_page_size();
        const size_t numPages = allocationSize / pageSize + (allocationSize % pageSize == 0 ? 0 : 1);
        void* commitedTopPtr = (void*)(allocator->base + allocator->commitedMax);
        allocator->platformIface->mem_commit(commitedTopPtr, numPages * pageSize);
        allocator->commitedMax += numPages * pageSize;
    }
    allocator->cur += alignedAllocationSize;
    return (void*)alignedPtr;
}

static void se_stack_reset(struct SeStackAllocator* allocator)
{
    allocator->cur = 0;
}
