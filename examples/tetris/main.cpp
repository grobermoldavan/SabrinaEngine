
#include "engine/engine.hpp"
#include "subsystems/tetris_subsystem.hpp"

int main(int argc, char* argv[])
{
    SabrinaEngine engine = {0};
    se_initialize(&engine); 
    se_add_default_subsystems(&engine);
    se_add_subsystem<SeVulkanRenderAbstractionSubsystem>(&engine);
    se_add_subsystem<TetrisSubsystem>(&engine);
    
    const SeEngineSettings settings
    {
        .applicationName    = "Sabrina engine - tetris example",
        .isFullscreenWindow = false,
        .isResizableWindow  = true,
        .windowWidth        = 640,
        .windowHeight       = 480,
    };
    se_run(&engine, &settings);
    return 0;
}

#include "engine/engine.cpp"
