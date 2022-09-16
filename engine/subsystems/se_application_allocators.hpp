#ifndef _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_HPP_
#define _SE_APPLICATION_ALLOCATORS_SUBSYSTEM_HPP_

#include "engine/se_allocator_bindings.hpp"

namespace allocators
{
    AllocatorBindings frame();
    AllocatorBindings persistent();

    namespace engine
    {
        void init();
        void update();
        void terminate();
    }
}

#endif
