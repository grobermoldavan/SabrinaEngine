
#include "engine/engine.hpp"
#include "subsystems/triangle_subsystem.hpp"

int main(int argc, char* argv[])
{
    SabrinaEngine engine = { };
    se_initialize(&engine);    
    se_add_subsystem(SE_PLATFORM_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_STACK_ALLOCATOR_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_POOL_ALLOCATOR_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_WINDOW_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_VULKAN_RENDER_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(TRIANGLE_SUBSYSTEM_NAME, &engine);
    se_run(&engine);
    return 0;
}

#include "engine/engine.cpp"
