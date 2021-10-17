
#include "se_application_allocators_subsystem.h"
#include "engine/engine.h"

static struct SeStackAllocator frameAllocator;
static struct SeStackAllocator sceneAllocator;
static struct SeStackAllocator appAllocator;

static struct SeAllocatorBindings frameAllocatorBindings;
static struct SeAllocatorBindings sceneAllocatorBindings;
static struct SeAllocatorBindings appAllocatorBindings;

static struct SeApplicationAllocatorsSubsystemInterface iface;

SE_DLL_EXPORT void se_init(struct SabrinaEngine* engine)
{
    struct SeStackAllocatorSubsystemInterface* stackIface =
        (struct SeStackAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_STACK_ALLOCATOR_SUBSYSTEM_NAME);
    stackIface->construct_allocator(&engine->platformIface, &frameAllocator, se_gigabytes(64));
    stackIface->construct_allocator(&engine->platformIface, &sceneAllocator, se_gigabytes(64));
    stackIface->construct_allocator(&engine->platformIface, &appAllocator, se_gigabytes(64));

    stackIface->to_allocator_bindings(&frameAllocator, &frameAllocatorBindings);
    stackIface->to_allocator_bindings(&sceneAllocator, &sceneAllocatorBindings);
    stackIface->to_allocator_bindings(&appAllocator, &appAllocatorBindings);

    iface = (struct SeApplicationAllocatorsSubsystemInterface)
    {
        .frameAllocator = &frameAllocatorBindings,
        .sceneAllocator = &sceneAllocatorBindings,
        .appAllocator = &appAllocatorBindings,
    };
}

SE_DLL_EXPORT void se_terminate(struct SabrinaEngine* engine)
{
    struct SeStackAllocatorSubsystemInterface* stackIface =
        (struct SeStackAllocatorSubsystemInterface*)engine->find_subsystem_interface(engine, SE_STACK_ALLOCATOR_SUBSYSTEM_NAME);
    stackIface->destruct_allocator(&frameAllocator);
    stackIface->destruct_allocator(&sceneAllocator);
    stackIface->destruct_allocator(&appAllocator);
}

SE_DLL_EXPORT void se_update(struct SabrinaEngine* engine)
{
    frameAllocator.reset(&frameAllocator);
}

SE_DLL_EXPORT void* se_get_interface(struct SabrinaEngine* engine)
{
    return &iface;
}
