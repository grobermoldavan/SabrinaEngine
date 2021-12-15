#ifndef _SE_ENGINE_H_
#define _SE_ENGINE_H_

#include "common_includes.h"
#include "debug.h"
#include "se_math.h"
#include "allocator_bindings.h"
#include "containers.h"
#include "platform.h"
#include "render_abstraction_interface.h"
#include "subsystems/se_application_allocators_subsystem.h"
#include "subsystems/se_stack_allocator_subsystem.h"
#include "subsystems/se_pool_allocator_subsystem.h"
#include "subsystems/se_window_subsystem.h"
#include "subsystems/se_vulkan_render_abstraction_subsystem.h"

typedef void  (*SeSubsystemFunc)(struct SabrinaEngine*);
typedef void* (*SeSubsystemReturnPtrFunc)(struct SabrinaEngine*);
typedef void  (*SeSubsystemUpdateFunc)(struct SabrinaEngine*, const struct SeUpdateInfo* dt);

typedef struct SeUpdateInfo
{
    float dt;
} SeUpdateInfo;

typedef struct SeSubsystemStorageEntry
{
    SeHandle libraryHandle;
    SeSubsystemReturnPtrFunc getInterface;
    SeSubsystemUpdateFunc update;
    const char* name;
} SeSubsystemStorageEntry;

typedef struct SeSubsystemStorage
{
    SeSubsystemStorageEntry subsystemsStorage[256];
    size_t subsystemsStorageSize;
} SeSubsystemStorage;

typedef struct SabrinaEngine
{
    void* (*find_subsystem_interface)(struct SabrinaEngine* engine, const char* name);
    SePlatformInterface platformIface;
    SeSubsystemStorage subsystems;
    bool shouldRun;
} SabrinaEngine;

void  se_initialize(SabrinaEngine* engine);
void  se_run(SabrinaEngine* engine);

#endif
