#ifndef _SE_ENGINE_H_
#define _SE_ENGINE_H_

#include "common_includes.h"
#include "debug.h"
#include "se_math.h"
#include "allocator_bindings.h"
#include "containers.h"
#include "platform.h"
#include "subsystems/se_application_allocators_subsystem.h"
#include "subsystems/se_stack_allocator_subsystem.h"
#include "subsystems/se_window_subsystem.h"

typedef void  (*SeSubsystemFunc)(struct SabrinaEngine*);
typedef void* (*SeSubsystemReturnPtrFunc)(struct SabrinaEngine*);

struct SeSubsystemStorageEntry
{
    struct SeHandle libraryHandle;
    SeSubsystemReturnPtrFunc getInterface;
    SeSubsystemFunc update;
    const char* name;
};

struct SeSubsystemStorage
{
    struct SeSubsystemStorageEntry subsystemsStorage[256];
    size_t subsystemsStorageSize;
};

struct SabrinaEngine
{
    void* (*find_subsystem_interface)(struct SabrinaEngine* engine, const char* name);

    struct SePlatformInterface platformIface;
    struct SeSubsystemStorage subsystems;
    bool shouldRun;
};

void  se_initialize(struct SabrinaEngine* engine);
void  se_run(struct SabrinaEngine* engine);

#endif
