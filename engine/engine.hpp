#ifndef _SE_ENGINE_H_
#define _SE_ENGINE_H_

#include "common_includes.hpp"
#include "debug.hpp"
#include "se_math.hpp"
#include "allocator_bindings.hpp"
#include "containers.hpp"
#include "platform.hpp"
#include "render_abstraction_interface.hpp"
#include "subsystems/se_application_allocators_subsystem.hpp"
#include "subsystems/se_stack_allocator_subsystem.hpp"
#include "subsystems/se_pool_allocator_subsystem.hpp"
#include "subsystems/se_window_subsystem.hpp"
#include "subsystems/se_vulkan_render_abstraction_subsystem.hpp"

using SeSubsystemFunc           = void  (*)(struct SabrinaEngine*);
using SeSubsystemReturnPtrFunc  = void* (*)(struct SabrinaEngine*);
using SeSubsystemUpdateFunc     = void  (*)(struct SabrinaEngine*, const struct SeUpdateInfo* dt);

struct SeUpdateInfo
{
    float dt;
};

struct SeSubsystemStorageEntry
{
    SeHandle libraryHandle;
    SeSubsystemReturnPtrFunc getInterface;
    SeSubsystemUpdateFunc update;
    const char* name;
};

struct SeSubsystemStorage
{
    SeSubsystemStorageEntry subsystemsStorage[256];
    size_t subsystemsStorageSize;
};

struct SabrinaEngine
{
    void* (*find_subsystem_interface)(SabrinaEngine* engine, const char* name);
    SePlatformInterface platformIface;
    SeSubsystemStorage subsystems;
    bool shouldRun;
};

void se_initialize(SabrinaEngine* engine);
void se_run(SabrinaEngine* engine);

#endif
