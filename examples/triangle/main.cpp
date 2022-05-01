
#include "engine/engine.hpp"
#include "subsystems/triangle_subsystem.hpp"

int main(int argc, char* argv[])
{
    SabrinaEngine engine = { };
    se_initialize(&engine); 
    se_add_subsystem<SePlatformSubsystem>(&engine);
    se_add_subsystem<SeStackAllocatorSubsystem>(&engine);
    se_add_subsystem<SePoolAllocatorSubsystem>(&engine);
    se_add_subsystem<SeApplicationAllocatorsSubsystem>(&engine);
    se_add_subsystem<SeLoggingSubsystem>(&engine);
    se_add_subsystem<SeStringSubsystem>(&engine);
    se_add_subsystem<SeWindowSubsystem>(&engine);
    se_add_subsystem<SeVulkanRenderAbstractionSubsystem>(&engine);
    se_add_subsystem<TriangleSubsystem>(&engine);
    se_run(&engine);
    return 0;
}

#include "engine/engine.cpp"
