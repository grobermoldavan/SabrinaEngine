
#include "engine/engine.hpp"
#include "engine/engine.cpp"

void init()
{

}

void terminate()
{
    
}

void update(const SeUpdateInfo& info)
{
    if (win::is_close_button_pressed()) engine::stop();
}

int main(int argc, char* argv[])
{
    const SeSettings settings
    {
        .applicationName    = "Project template",
        .isFullscreenWindow = false,
        .isResizableWindow  = true,
        .windowWidth        = 640,
        .windowHeight       = 480,
        .maxAssetsCpuUsage  = se_megabytes(512),
        .maxAssetsGpuUsage  = se_megabytes(512),
    };
    engine::run(&settings, init, update, terminate);
    return 0;
}
