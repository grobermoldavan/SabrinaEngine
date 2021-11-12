
#include "se_application_allocators_subsystem.h"
#include "se_stack_allocator_subsystem.h"
#include "engine/engine.h"

#ifdef _WIN32
#   define SE_APP_ALLOCATOR_IFACE_FUNC __declspec(dllexport)
#else
#   error Unsupported platform
#endif

SE_APP_ALLOCATOR_IFACE_FUNC void  se_init(struct SabrinaEngine*);
SE_APP_ALLOCATOR_IFACE_FUNC void  se_terminate(struct SabrinaEngine*);
SE_APP_ALLOCATOR_IFACE_FUNC void  se_update(struct SabrinaEngine*);
SE_APP_ALLOCATOR_IFACE_FUNC void* se_get_interface(struct SabrinaEngine*);

static SeStackAllocator frameAllocator;
static SeStackAllocator sceneAllocator;
static SeStackAllocator appAllocator;

static SeAllocatorBindings frameAllocatorBindings;
static SeAllocatorBindings sceneAllocatorBindings;
static SeAllocatorBindings appAllocatorBindings;

static SeApplicationAllocatorsSubsystemInterface g_Iface;

SE_APP_ALLOCATOR_IFACE_FUNC void se_init(SabrinaEngine* engine)
{
    SeStackAllocatorSubsystemInterface* stackIface =
        (SeStackAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_STACK_ALLOCATOR_SUBSYSTEM_NAME);
    stackIface->construct_allocator(&engine->platformIface, &frameAllocator, se_gigabytes(64));
    stackIface->construct_allocator(&engine->platformIface, &sceneAllocator, se_gigabytes(64));
    stackIface->construct_allocator(&engine->platformIface, &appAllocator, se_gigabytes(64));

    stackIface->to_allocator_bindings(&frameAllocator, &frameAllocatorBindings);
    stackIface->to_allocator_bindings(&sceneAllocator, &sceneAllocatorBindings);
    stackIface->to_allocator_bindings(&appAllocator, &appAllocatorBindings);

    g_Iface = (SeApplicationAllocatorsSubsystemInterface)
    {
        .frameAllocator = &frameAllocatorBindings,
        .sceneAllocator = &sceneAllocatorBindings,
        .appAllocator = &appAllocatorBindings,
    };
}

SE_APP_ALLOCATOR_IFACE_FUNC void se_terminate(SabrinaEngine* engine)
{
    SeStackAllocatorSubsystemInterface* stackIface =
        (SeStackAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_STACK_ALLOCATOR_SUBSYSTEM_NAME);
    stackIface->destruct_allocator(&frameAllocator);
    stackIface->destruct_allocator(&sceneAllocator);
    stackIface->destruct_allocator(&appAllocator);
}

SE_APP_ALLOCATOR_IFACE_FUNC void se_update(SabrinaEngine* engine)
{
    frameAllocator.reset(&frameAllocator);
}

SE_APP_ALLOCATOR_IFACE_FUNC void* se_get_interface(SabrinaEngine* engine)
{
    return &g_Iface;
}
