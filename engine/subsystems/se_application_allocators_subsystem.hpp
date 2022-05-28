#ifndef _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_
#define _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_

#include "engine/allocator_bindings.hpp"

struct SeApplicationAllocatorsSubsystemInterface
{
    static constexpr const char* NAME = "SeApplicationAllocatorsSubsystemInterface";

    AllocatorBindings frameAllocator;
    AllocatorBindings persistentAllocator;
};

struct SeApplicationAllocatorsSubsystem
{
    using Interface = SeApplicationAllocatorsSubsystemInterface;
    static constexpr const char* NAME = "se_application_allocators_subsystem";
};

#define SE_APPLICATION_ALLOCATORS_SUBSYSTEM_GLOBAL_NAME g_applicationAllocatorsIface
const SeApplicationAllocatorsSubsystemInterface* SE_APPLICATION_ALLOCATORS_SUBSYSTEM_GLOBAL_NAME;

namespace app_allocators
{
    inline AllocatorBindings frame()
    {
        return SE_APPLICATION_ALLOCATORS_SUBSYSTEM_GLOBAL_NAME->frameAllocator;
    }

    inline AllocatorBindings persistent()
    {
        return SE_APPLICATION_ALLOCATORS_SUBSYSTEM_GLOBAL_NAME->persistentAllocator;
    }
}

#endif
