
#include "se_engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

uint64_t _se_get_perf_counter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

uint64_t _se_get_perf_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

#else // ifdef _WIN32
#   error Unsupported platform
#endif

bool g_shouldRun = false;

void se_engine_run(const SeSettings& settings, SeInitPfn init, SeUpdatePfn update, SeTerminatePfn terminate)
{
    _se_allocator_init();
    _se_string_init();
    _se_dbg_init();
    _se_fs_init(settings);
    _se_win_init(settings);
    _se_render_init(settings);
    _se_asset_init(settings);
    _se_ui_init();
    if (init) init();

    g_shouldRun = true;
    
    size_t frame = 0;
    const double counterFrequency = double(_se_get_perf_frequency());
    uint64_t prevCounter = _se_get_perf_counter();
    while (g_shouldRun)
    {
        const uint64_t newCounter = _se_get_perf_counter();
        const float dt = float(double(newCounter - prevCounter) / counterFrequency);
        
        _se_allocator_update();
        _se_dbg_update();
        _se_win_update();
        _se_render_update();
        if (update) update({ dt, frame });

        prevCounter = newCounter;
        frame += 1;
    }

    if (terminate) terminate();
    _se_ui_terminate();
    _se_asset_terminate();
    _se_render_terminate();
    _se_win_terminate();
    _se_fs_terminate();
    _se_dbg_terminate();
    _se_string_terminate();
    _se_allocator_terminate();
}

void se_engine_stop()
{
    g_shouldRun = false;
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

#include "engine/se_data_providers.cpp"
