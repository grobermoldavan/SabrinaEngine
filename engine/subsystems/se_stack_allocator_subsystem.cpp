
#include <string.h>
#include "se_stack_allocator_subsystem.hpp"
#include "engine/engine.hpp"

static SeStackAllocatorSubsystemInterface g_iface;

static void* se_stack_alloc(SeStackAllocator* allocator, size_t size, size_t alignment, const char* allocTag)
{
    se_assert(((alignment - 1) & alignment) == 0 && "Alignment must be a power of two");
    //
    // Align pointer
    //
    const intptr_t stackTopPtr = allocator->base + allocator->cur;
    size_t additionalAlignment = (size_t)(stackTopPtr & (alignment - 1));
    if (additionalAlignment != 0) additionalAlignment = alignment - additionalAlignment;
    const intptr_t alignedPtr = stackTopPtr + additionalAlignment;
    const size_t alignedAllocationSize = size + additionalAlignment;
    //
    // Try allocate
    //
    if ((allocator->cur + alignedAllocationSize) > allocator->reservedMax)
    {
        return nullptr;
    }
    if ((allocator->cur + alignedAllocationSize) > allocator->commitedMax)
    {
        const intptr_t allocationSize = alignedAllocationSize - (allocator->commitedMax - allocator->cur);
        const size_t pageSize = platform::get()->get_mem_page_size();
        const size_t numPages = allocationSize / pageSize + (allocationSize % pageSize == 0 ? 0 : 1);
        void* commitedTopPtr = (void*)(allocator->base + allocator->commitedMax);
        platform::get()->mem_commit(commitedTopPtr, numPages * pageSize);
        allocator->commitedMax += numPages * pageSize;
    }
    allocator->cur += alignedAllocationSize;
    return (void*)alignedPtr;
}

static void se_stack_reset(SeStackAllocator& allocator)
{
    allocator.cur = 0;
}

static void se_stack_dealloc(SeStackAllocator* allocator, void* ptr, size_t size)
{
    // nothing
}

static void se_stack_construct_allocator(SeStackAllocator& allocator, size_t capacity)
{
    allocator =
    {
        .base           = (intptr_t)platform::get()->mem_reserve(capacity),
        .cur            = 0,
        .reservedMax    = capacity,
        .commitedMax    = 0,
    };
}

static void se_stack_destruct_allocator(SeStackAllocator& allocator)
{
    platform::get()->mem_release((void*)allocator.base, allocator.reservedMax);
    memset(&allocator, 0, sizeof(SeStackAllocator));
}

static SeAllocatorBindings se_stack_to_allocator_bindings(SeStackAllocator& allocator)
{
    return
    {
        .allocator  = &allocator,
        .alloc      = [](void* allocator, size_t size, size_t alignment, const char* tag) { return se_stack_alloc((SeStackAllocator*)allocator, size, alignment, tag); },
        .dealloc    = [](void* allocator, void* ptr, size_t size) { se_stack_dealloc((SeStackAllocator*)allocator, ptr, size); },
    };
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .construct              = se_stack_construct_allocator,
        .destroy                = se_stack_destruct_allocator,
        .to_allocator_bindings  = se_stack_to_allocator_bindings,
        .reset                  = se_stack_reset,
    };
    se_init_global_subsystem_pointers(engine);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}
