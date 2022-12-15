
#include "engine.hpp"

#ifdef _WIN32

#define SE_PATH_SEP "\\"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

uint64_t se_get_perf_counter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

uint64_t se_get_perf_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

#else // ifdef _WIN32
#   error Unsupported platform
#endif

bool g_shouldRun = false;

namespace engine
{
    void run(const SeSettings* settings, SeInitPfn init, SeUpdatePfn update, SeTerminatePfn terminate)
    {
        allocators::engine::init();
        string::engine::init();
        debug::engine::init();
        fs::engine::init();
        win::engine::init(settings);
        render::engine::init();
        assets::engine::init(settings);
        ui::engine::init();
        if (init) init();

        g_shouldRun = true;
        
        size_t frame = 0;
        const double counterFrequency = double(se_get_perf_frequency());
        uint64_t prevCounter = se_get_perf_counter();
        while (g_shouldRun)
        {
            const uint64_t newCounter = se_get_perf_counter();
            const float dt = float(double(newCounter - prevCounter) / counterFrequency);
            
            allocators::engine::update();
            debug::engine::update();
            win::engine::update();
            render::engine::update();
            if (update) update({ dt, frame });

            prevCounter = newCounter;
            frame += 1;
        }

        if (terminate) terminate();
        ui::engine::terminate();
        assets::engine::terminate();
        render::engine::terminate();
        win::engine::terminate();
        fs::engine::terminate();
        debug::engine::terminate();
        string::engine::terminate();
        allocators::engine::terminate();
    }

    void stop()
    {
        g_shouldRun = false;
    }
}

#ifdef SE_VULKAN
#   include "engine/render/se_vulkan.cpp"
#else
#   error Graphics api is not defined
#endif

#include "engine/subsystems/se_platform.cpp"
#include "engine/subsystems/se_application_allocators.cpp"
#include "engine/subsystems/se_string.cpp"
#include "engine/subsystems/se_debug.cpp"
#include "engine/subsystems/se_file_system.cpp"
#include "engine/subsystems/se_window.cpp"
#include "engine/subsystems/se_assets.cpp"
#include "engine/subsystems/se_ui.cpp"
