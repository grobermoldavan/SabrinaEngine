
#include <string.h>
#include "se_stack_allocator_subsystem.hpp"
#include "engine/engine.hpp"

static void se_stack_construct_allocator(SeStackAllocator* allocator, size_t capacity);
static void se_stack_destruct_allocator(SeStackAllocator* allocator);
static void se_stack_to_allocator_bindings(SeStackAllocator* allocator, SeAllocatorBindings* bindings);

static void* se_stack_alloc(SeStackAllocator* allocator, size_t size, size_t alignment, const char* allocTag);
static void se_stack_reset(SeStackAllocator* allocator);
static void se_stack_dealloc(SeStackAllocator* allocator, void* ptr, size_t size);

static SeStackAllocatorSubsystemInterface g_Iface;
static SePlatformSubsystemInterface* g_platformIface;

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_Iface =
    {
        .construct = se_stack_construct_allocator,
        .destroy = se_stack_destruct_allocator,
        .to_allocator_bindings = se_stack_to_allocator_bindings,
    };
    g_platformIface = se_get_subsystem_interface<SePlatformSubsystemInterface>(engine);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_Iface;
}

static void se_stack_construct_allocator(SeStackAllocator* allocator, size_t capacity)
{
    *allocator =
    {
        .base           = (intptr_t)g_platformIface->mem_reserve(capacity),
        .cur            = 0,
        .reservedMax    = capacity,
        .commitedMax    = 0,
        .alloc          = se_stack_alloc,
        .reset          = se_stack_reset,
    };
}

static void se_stack_destruct_allocator(SeStackAllocator* allocator)
{
    g_platformIface->mem_release((void*)allocator->base, allocator->reservedMax);
    memset(allocator, 0, sizeof(SeStackAllocator));
}

static void se_stack_to_allocator_bindings(SeStackAllocator* allocator, SeAllocatorBindings* bindings)
{
    *bindings =
    {
        .allocator  = allocator,
        .alloc      = [](void* allocator, size_t size, size_t alignment, const char* tag) { return se_stack_alloc((SeStackAllocator*)allocator, size, alignment, tag); },
        .dealloc    = [](void* allocator, void* ptr, size_t size) { se_stack_dealloc((SeStackAllocator*)allocator, ptr, size); },
    };
}

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
        const size_t pageSize = g_platformIface->get_mem_page_size();
        const size_t numPages = allocationSize / pageSize + (allocationSize % pageSize == 0 ? 0 : 1);
        void* commitedTopPtr = (void*)(allocator->base + allocator->commitedMax);
        g_platformIface->mem_commit(commitedTopPtr, numPages * pageSize);
        allocator->commitedMax += numPages * pageSize;
    }
    allocator->cur += alignedAllocationSize;
    return (void*)alignedPtr;
}

static void se_stack_reset(SeStackAllocator* allocator)
{
    allocator->cur = 0;
}

static void se_stack_dealloc(SeStackAllocator* allocator, void* ptr, size_t size)
{
    // nothing
}
