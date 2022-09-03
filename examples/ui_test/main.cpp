
#include "engine/engine.hpp"
#include "subsystems/ui_test_subsystem.hpp"

int main(int argc, char* argv[])
{
    SabrinaEngine engine = { };
    se_initialize(&engine); 
    se_add_default_subsystems(&engine);
    se_add_subsystem<SeVulkanRenderAbstractionSubsystem>(&engine);
    se_add_subsystem<UiTestSubsystem>(&engine);

    const SeEngineSettings settings
    {
        .applicationName    = "Sabrina engine - ui example",
        .isFullscreenWindow = true,
        .isResizableWindow  = true,
        .windowWidth        = 640,
        .windowHeight       = 480,
    };
    se_run(&engine, &settings);
    return 0;
}

#include "engine/engine.cpp"
