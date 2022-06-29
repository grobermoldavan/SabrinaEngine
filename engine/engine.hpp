#ifndef _SE_ENGINE_HPP_
#define _SE_ENGINE_HPP_

#include <string.h>
#include "common_includes.hpp"
#include "se_math.hpp"
#include "allocator_bindings.hpp"
#include "hash.hpp"
#include "utils.hpp"
#include "containers.hpp"
#include "data_providers.hpp"
#include "render_abstraction_interface.hpp"
#include "subsystems/se_platform_subsystem.hpp"
#include "subsystems/se_application_allocators_subsystem.hpp"
#include "subsystems/se_string_subsystem.hpp"
#include "subsystems/se_debug_subsystem.hpp"
#include "subsystems/se_window_subsystem.hpp"
#include "subsystems/se_vulkan_render_abstraction_subsystem.hpp"
#include "subsystems/se_ui_subsystem.hpp"

struct UpdateInfo
{
    float dt;
};

using SeSubsystemFunc           = void  (*)(struct SabrinaEngine*);
using SeSubsystemReturnPtrFunc  = void* (*)(struct SabrinaEngine*);
using SeSubsystemUpdateFunc     = void  (*)(struct SabrinaEngine*, const struct UpdateInfo* dt);

using LibraryHandle = uint64_t;

struct SubsystemStorageEntry
{
    LibraryHandle               libraryHandle;
    SeSubsystemReturnPtrFunc    getInterface;
    SeSubsystemUpdateFunc       update;
    const char*                 subsystemName;
    const char*                 interfaceName;
};

static constexpr size_t SE_MAX_SUBSYSTEMS = 256;

struct SubsystemStorage
{
    SubsystemStorageEntry   storage[SE_MAX_SUBSYSTEMS];
    size_t                  size;
};

struct SabrinaEngine
{
    LibraryHandle   (*load_dynamic_library)(const char* name);
    void*           (*find_function_address)(LibraryHandle lib, const char* name);

    SubsystemStorage subsystemStorage;
    bool shouldRun;
};

void se_initialize(SabrinaEngine* engine);

void se_run(SabrinaEngine* engine);

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
    LibraryHandle libHandle = { };
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

void se_add_default_subsystems(SabrinaEngine* engine)
{
    se_add_subsystem<SePlatformSubsystem>(engine);
    se_add_subsystem<SeApplicationAllocatorsSubsystem>(engine);
    se_add_subsystem<SeStringSubsystem>(engine);
    se_add_subsystem<SeDebugSubsystem>(engine);
    se_add_subsystem<SeWindowSubsystem>(engine);
    se_add_subsystem<SeUiSubsystem>(engine);
}

void se_init_global_subsystem_pointers(SabrinaEngine* engine)
{
    SE_PLATFORM_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SePlatformSubsystemInterface>(engine);
    SE_APPLICATION_ALLOCATORS_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeApplicationAllocatorsSubsystemInterface>(engine);
    SE_STRING_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeStringSubsystemInterface>(engine);
    SE_DEBUG_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeDebugSubsystemInterface>(engine);
    SE_WINDOW_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeWindowSubsystemInterface>(engine);
    SE_UI_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeUiSubsystemInterface>(engine);
}

#endif
