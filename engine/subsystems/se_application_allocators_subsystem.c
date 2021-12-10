
#include "se_application_allocators_subsystem.h"
#include "se_stack_allocator_subsystem.h"
#include "se_pool_allocator_subsystem.h"
#include "engine/engine.h"

#ifdef _WIN32
#   define SE_APP_ALLOCATOR_IFACE_FUNC __declspec(dllexport)
#else
#   error Unsupported platform
#endif

static SeStackAllocator frameAllocator;
static SePoolAllocator persistentAllocator;

static SeAllocatorBindings frameAllocatorBindings;
static SeAllocatorBindings persistentAllocatorBindings;

static SeApplicationAllocatorsSubsystemInterface g_Iface;

SE_APP_ALLOCATOR_IFACE_FUNC void se_load(SabrinaEngine* engine)
{
    g_Iface = (SeApplicationAllocatorsSubsystemInterface)
    {
        .frameAllocator = &frameAllocatorBindings,
        .persistentAllocator = &persistentAllocatorBindings,
    };
}

SE_APP_ALLOCATOR_IFACE_FUNC void se_init(SabrinaEngine* engine)
{
    SeStackAllocatorSubsystemInterface* stackIface = (SeStackAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_STACK_ALLOCATOR_SUBSYSTEM_NAME);
    SePoolAllocatorSubsystemInterface* poolIface = (SePoolAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_POOL_ALLOCATOR_SUBSYSTEM_NAME);

    stackIface->construct(&engine->platformIface, &frameAllocator, se_gigabytes(64));
    SePoolAllocatorCreateInfo poolCreateInfo = (SePoolAllocatorCreateInfo)
    {
        .platformIface = &engine->platformIface,
        .buckets = { { .blockSize = 8 }, { .blockSize = 32 }, { .blockSize = 256 } },
    };
    poolIface->construct(&persistentAllocator, &poolCreateInfo);

    stackIface->to_allocator_bindings(&frameAllocator, &frameAllocatorBindings);
    poolIface->to_allocator_bindings(&persistentAllocator, &persistentAllocatorBindings);
}

SE_APP_ALLOCATOR_IFACE_FUNC void se_terminate(SabrinaEngine* engine)
{
    SeStackAllocatorSubsystemInterface* stackIface = (SeStackAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_STACK_ALLOCATOR_SUBSYSTEM_NAME);
    SePoolAllocatorSubsystemInterface* poolIface = (SePoolAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_POOL_ALLOCATOR_SUBSYSTEM_NAME);
    
    poolIface->destroy(&persistentAllocator);
    stackIface->destroy(&frameAllocator);
}

SE_APP_ALLOCATOR_IFACE_FUNC void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    frameAllocator.reset(&frameAllocator);
}

SE_APP_ALLOCATOR_IFACE_FUNC void* se_get_interface(SabrinaEngine* engine)
{
    return &g_Iface;
}
