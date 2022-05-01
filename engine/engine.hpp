#ifndef _SE_ENGINE_H_
#define _SE_ENGINE_H_

#include <string.h>
#include "common_includes.hpp"
#include "debug.hpp"
#include "se_math.hpp"
#include "allocator_bindings.hpp"
#include "containers.hpp"
#include "render_abstraction_interface.hpp"
#include "subsystems/se_platform_subsystem.hpp"
#include "subsystems/se_stack_allocator_subsystem.hpp"
#include "subsystems/se_pool_allocator_subsystem.hpp"
#include "subsystems/se_application_allocators_subsystem.hpp"
#include "subsystems/se_logging_subsystem.hpp"
#include "subsystems/se_string_subsystem.hpp"
#include "subsystems/se_window_subsystem.hpp"
#include "subsystems/se_vulkan_render_abstraction_subsystem.hpp"

struct SeUpdateInfo
{
    float dt;
};

using SeSubsystemFunc           = void  (*)(struct SabrinaEngine*);
using SeSubsystemReturnPtrFunc  = void* (*)(struct SabrinaEngine*);
using SeSubsystemUpdateFunc     = void  (*)(struct SabrinaEngine*, const struct SeUpdateInfo* dt);

using SeLibraryHandle = uint64_t;

struct SeSubsystemStorageEntry
{
    SeLibraryHandle             libraryHandle;
    SeSubsystemReturnPtrFunc    getInterface;
    SeSubsystemUpdateFunc       update;
    const char*                 subsystemName;
    const char*                 interfaceName;
};

static constexpr size_t SE_MAX_SUBSYSTEMS = 256;

struct SeSubsystemStorage
{
    SeSubsystemStorageEntry storage[SE_MAX_SUBSYSTEMS];
    size_t                  size;
};

struct SabrinaEngine
{
    SeLibraryHandle (*load_dynamic_library)(const char* name);
    void* (*find_function_address)(SeLibraryHandle lib, const char* name);

    SeSubsystemStorage subsystemStorage;
    bool shouldRun;
};

void se_initialize(SabrinaEngine* engine);

template<typename Subsystem>
void se_add_subsystem(SabrinaEngine* engine)
{
    const size_t BUFFER_SIZE = 512;
    const char* POSSIBLE_LIB_PATHS[] =
    {
        "." SE_PATH_SEP "subsystems" SE_PATH_SEP "default" SE_PATH_SEP,
        "." SE_PATH_SEP "subsystems" SE_PATH_SEP "application" SE_PATH_SEP,
        "",
    };
    const size_t nameLength = strlen(Subsystem::NAME);
    char buffer[BUFFER_SIZE];
    SeLibraryHandle libHandle = { };
    for (const char* path : POSSIBLE_LIB_PATHS)
    {
        const size_t pathLength = strlen(path);
        se_assert((nameLength + pathLength) < BUFFER_SIZE);
        memcpy(buffer, path, pathLength);
        memcpy(buffer + pathLength, Subsystem::NAME, nameLength);
        buffer[pathLength + nameLength] = 0;
        libHandle = engine->load_dynamic_library(buffer);
        if (libHandle) break;
    }
    se_assert(libHandle);
    using Interface = typename Subsystem::Interface;
    engine->subsystemStorage.storage[engine->subsystemStorage.size++] =
    {
        .libraryHandle      = libHandle,
        .getInterface       = (SeSubsystemReturnPtrFunc)engine->find_function_address(libHandle, "se_get_interface"),
        .update             = (SeSubsystemUpdateFunc)engine->find_function_address(libHandle, "se_update"),
        .subsystemName      = Subsystem::NAME,
        .interfaceName      = Interface::NAME
    };
}

template<typename Iterface>
const Iterface* se_get_subsystem_interface(SabrinaEngine* engine)
{
    for (size_t i = 0; i < engine->subsystemStorage.size; i++)
    {
        if (strcmp(engine->subsystemStorage.storage[i].interfaceName, Iterface::NAME) == 0)
        {
            SeSubsystemReturnPtrFunc getInterface = engine->subsystemStorage.storage[i].getInterface;
            return (const Iterface*)(getInterface ? getInterface(engine) : nullptr);
        }
    }
    return nullptr;
}


void se_run(SabrinaEngine* engine);

#endif
