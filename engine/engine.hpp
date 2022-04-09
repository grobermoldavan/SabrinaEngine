#ifndef _SE_ENGINE_H_
#define _SE_ENGINE_H_

#include "common_includes.hpp"
#include "debug.hpp"
#include "se_math.hpp"
#include "allocator_bindings.hpp"
#include "containers.hpp"
#include "render_abstraction_interface.hpp"
#include "subsystems/se_platform_subsystem.hpp"
#include "subsystems/se_application_allocators_subsystem.hpp"
#include "subsystems/se_stack_allocator_subsystem.hpp"
#include "subsystems/se_pool_allocator_subsystem.hpp"
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
    const char*                 name;
};

static constexpr size_t SE_MAX_SUBSYSTEMS = 256;

struct SeSubsystemStorage
{
    SeSubsystemStorageEntry storage[SE_MAX_SUBSYSTEMS];
    size_t                  size;
};

struct SabrinaEngine
{
    void* (*find_subsystem_interface)(SabrinaEngine* engine, const char* name);
    SeSubsystemStorage subsystemStorage;
    bool shouldRun;
};

void se_initialize(SabrinaEngine* engine);
void se_add_subsystem(const char* name, SabrinaEngine* engine);
void se_run(SabrinaEngine* engine);

#endif
