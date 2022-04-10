#ifndef _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_
#define _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_

struct SeApplicationAllocatorsSubsystemInterface
{
    static constexpr const char* NAME = "SeApplicationAllocatorsSubsystemInterface";

    struct SeAllocatorBindings* frameAllocator;
    struct SeAllocatorBindings* persistentAllocator;
};

struct SeApplicationAllocatorsSubsystem
{
    using Interface = SeApplicationAllocatorsSubsystemInterface;
    static constexpr const char* NAME = "se_application_allocators_subsystem";
};

#endif
