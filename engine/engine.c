
#include <string.h>

#include "engine.h"

static void* se_find_subsystem_interface(SabrinaEngine* engine, const char* name)
{
    for (size_t i = 0; i < engine->subsystems.subsystemsStorageSize; i++)
    {
        if (strcmp(engine->subsystems.subsystemsStorage[i].name, name) == 0)
        {
            SeSubsystemReturnPtrFunc getInterface = engine->subsystems.subsystemsStorage[i].getInterface;
            return getInterface ? getInterface(engine) : NULL;
        }
    }
    return NULL;
}

void se_initialize(SabrinaEngine* engine)
{
    engine->find_subsystem_interface = se_find_subsystem_interface;
    se_get_platform_interface(&engine->platformIface);
    engine->shouldRun = true;
}

void se_run(SabrinaEngine* engine)
{
    // First load all subsystems
    for (size_t i = 0; i < engine->subsystems.subsystemsStorageSize; i++)
    {
        SeHandle handle = engine->subsystems.subsystemsStorage[i].libraryHandle;
        SeSubsystemFunc load = (SeSubsystemFunc)engine->platformIface.get_dynamic_library_function_address(handle, "se_load");
        if (load) load(engine);
    }
    // Init subsystems after load (here subsytems can safely query interfaces of other subsystems)
    for (size_t i = 0; i < engine->subsystems.subsystemsStorageSize; i++)
    {
        SeHandle handle = engine->subsystems.subsystemsStorage[i].libraryHandle;
        SeSubsystemFunc init = (SeSubsystemFunc)engine->platformIface.get_dynamic_library_function_address(handle, "se_init");
        if (init) init(engine);
    }
    // Update loop
    while (engine->shouldRun)
    {
        for (size_t i = 0; i < engine->subsystems.subsystemsStorageSize; i++)
        {
            SeSubsystemFunc update = engine->subsystems.subsystemsStorage[i].update;
            if (update) update(engine);
        }
    }
    // Terminate subsystems (here subsytems can safely query interfaces of other subsystems)
    for (size_t i = engine->subsystems.subsystemsStorageSize; i > 0; i--)
    {
        SeHandle handle = engine->subsystems.subsystemsStorage[i - 1].libraryHandle;
        SeSubsystemFunc terminate = (SeSubsystemFunc)engine->platformIface.get_dynamic_library_function_address(handle, "se_terminate");
        if (terminate) terminate(engine);
    }
    // Unload subsystems
    for (size_t i = engine->subsystems.subsystemsStorageSize; i > 0; i--)
    {
        SeHandle handle = engine->subsystems.subsystemsStorage[i - 1].libraryHandle;
        SeSubsystemFunc unload = (SeSubsystemFunc)engine->platformIface.get_dynamic_library_function_address(handle, "se_unload");
        if (unload) unload(engine);
    }
    // Unload lib handle
    for (size_t i = engine->subsystems.subsystemsStorageSize; i > 0; i--)
    {
        engine->platformIface.unload_dynamic_library(engine->subsystems.subsystemsStorage[i - 1].libraryHandle);
    }
}

#define SE_DEBUG_IMPL
#include "debug.h"

#define SE_MATH_IMPL
#include "se_math.h"

#define SE_CONTAINERS_IMPL
#include "containers.h"

#define SE_PLATFORM_IMPL
#include "platform.h"
