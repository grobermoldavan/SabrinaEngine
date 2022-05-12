
#include "se_application_allocators_subsystem.hpp"
#include "engine/engine.hpp"

static SeStackAllocator frameAllocator;
static SePoolAllocator persistentAllocator;

static SeApplicationAllocatorsSubsystemInterface g_iface;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_init_global_subsystem_pointers(engine);
    frameAllocator = stack_allocator::create(se_gigabytes(64));
    persistentAllocator = pool_allocator::create({ .buckets = { { .blockSize = 8 }, { .blockSize = 32 }, { .blockSize = 256 } }, });
    g_iface =
    {
        .frameAllocator      = stack_allocator::to_bindings(frameAllocator),
        .persistentAllocator = pool_allocator::to_bindings(persistentAllocator),
    };
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    pool_allocator::destroy(persistentAllocator);
    stack_allocator::destroy(frameAllocator);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    stack_allocator::reset(frameAllocator);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}
