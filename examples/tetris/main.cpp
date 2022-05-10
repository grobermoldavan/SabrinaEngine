
#include "engine/engine.hpp"
#include "subsystems/tetris_subsystem.hpp"

int main(int argc, char* argv[])
{
    SabrinaEngine engine = {0};
    se_initialize(&engine); 
    se_add_subsystem<SePlatformSubsystem>(&engine);
    se_add_subsystem<SeStackAllocatorSubsystem>(&engine);
    se_add_subsystem<SePoolAllocatorSubsystem>(&engine);
    se_add_subsystem<SeApplicationAllocatorsSubsystem>(&engine);
    se_add_subsystem<SeLoggingSubsystem>(&engine);
    se_add_subsystem<SeStringSubsystem>(&engine);
    se_add_subsystem<SeWindowSubsystem>(&engine);
    se_add_subsystem<SeVulkanRenderAbstractionSubsystem>(&engine);
    se_add_subsystem<TetrisSubsystem>(&engine);
    se_run(&engine);
    return 0;
}

#include "engine/engine.cpp"
