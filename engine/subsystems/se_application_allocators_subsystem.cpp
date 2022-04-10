
#include "se_application_allocators_subsystem.hpp"
#include "se_stack_allocator_subsystem.hpp"
#include "se_pool_allocator_subsystem.hpp"
#include "engine/engine.hpp"

static SeStackAllocator frameAllocator;
static SePoolAllocator persistentAllocator;

static SeAllocatorBindings frameAllocatorBindings;
static SeAllocatorBindings persistentAllocatorBindings;

static SeApplicationAllocatorsSubsystemInterface g_Iface;

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_Iface =
    {
        .frameAllocator = &frameAllocatorBindings,
        .persistentAllocator = &persistentAllocatorBindings,
    };
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    const SeStackAllocatorSubsystemInterface* stackIface = se_get_subsystem_interface<SeStackAllocatorSubsystemInterface>(engine);
    const SePoolAllocatorSubsystemInterface* poolIface = se_get_subsystem_interface<SePoolAllocatorSubsystemInterface>(engine);

    stackIface->construct(&frameAllocator, se_gigabytes(64));
    SePoolAllocatorCreateInfo poolCreateInfo
    {
        .buckets = { { .blockSize = 8 }, { .blockSize = 32 }, { .blockSize = 256 } },
    };
    poolIface->construct(&persistentAllocator, &poolCreateInfo);

    stackIface->to_allocator_bindings(&frameAllocator, &frameAllocatorBindings);
    poolIface->to_allocator_bindings(&persistentAllocator, &persistentAllocatorBindings);
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    const SeStackAllocatorSubsystemInterface* stackIface = se_get_subsystem_interface<SeStackAllocatorSubsystemInterface>(engine);
    const SePoolAllocatorSubsystemInterface* poolIface = se_get_subsystem_interface<SePoolAllocatorSubsystemInterface>(engine);

    poolIface->destroy(&persistentAllocator);
    stackIface->destroy(&frameAllocator);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    frameAllocator.reset(&frameAllocator);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_Iface;
}
