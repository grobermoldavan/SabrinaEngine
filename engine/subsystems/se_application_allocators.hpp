#ifndef _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_HPP_
#define _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_HPP_

#include "engine/se_allocator_bindings.hpp"

SeAllocatorBindings se_allocator_frame();
SeAllocatorBindings se_allocator_persistent();

void _se_allocator_init();
void _se_allocator_update();
void _se_allocator_terminate();

#endif
