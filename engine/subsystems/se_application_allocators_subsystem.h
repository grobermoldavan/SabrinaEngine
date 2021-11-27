#ifndef _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_
#define _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_H_

#define SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME "se_application_allocators_subsystem"

typedef struct SeApplicationAllocatorsSubsystemInterface
{
    struct SeAllocatorBindings* frameAllocator;
    struct SeAllocatorBindings* persistentAllocator;
} SeApplicationAllocatorsSubsystemInterface;

#endif
