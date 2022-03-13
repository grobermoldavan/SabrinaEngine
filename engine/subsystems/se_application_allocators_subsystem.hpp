#ifndef _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_
#define _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_

#define SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME "se_application_allocators_subsystem"

struct SeApplicationAllocatorsSubsystemInterface
{
    struct SeAllocatorBindings* frameAllocator;
    struct SeAllocatorBindings* persistentAllocator;
};

#endif
