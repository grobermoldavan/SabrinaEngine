
#include "engine/engine.h"
#include "subsystems/marching_cubes_subsystem.h"

int main(int argc, char* argv[])
{
    SabrinaEngine engine = {0};
    se_initialize(&engine);
    se_add_subsystem(SE_STACK_ALLOCATOR_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_POOL_ALLOCATOR_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_APPLICATION_ALLOCATORS_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_WINDOW_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(SE_VULKAN_RENDER_SUBSYSTEM_NAME, &engine);
    se_add_subsystem(MARCHING_CUBES_SUBSYSTEM_NAME, &engine);
    se_run(&engine);
    return 0;
}

#include "engine/engine.c"
