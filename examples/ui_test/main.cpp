
#include "engine/engine.hpp"
#include "subsystems/ui_test_subsystem.hpp"

int main(int argc, char* argv[])
{
    SabrinaEngine engine = { };
    se_initialize(&engine); 
    se_add_default_subsystems(&engine);
    se_add_subsystem<SeVulkanRenderAbstractionSubsystem>(&engine);
    se_add_subsystem<UiTestSubsystem>(&engine);
    se_run(&engine);
    return 0;
}

#include "engine/engine.cpp"
