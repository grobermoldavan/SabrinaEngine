
#include "engine/engine.hpp"
#include "engine/engine.cpp"

#include "impl/cubes.hpp"
#include "impl/cubes.cpp"

int main(int argc, char* argv[])
{
    const SeSettings settings
    {
        .applicationName    = "Sabrina engine - marching cubes example",
        .isFullscreenWindow = false,
        .isResizableWindow  = true,
        .windowWidth        = 640,
        .windowHeight       = 480,
    };
    engine::run(&settings, init, update, terminate);
    return 0;
}

