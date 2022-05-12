
#include "engine.hpp"

#ifdef _WIN32

#define SE_PATH_SEP "\\"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static HMODULE se_hmodule_from_se_handle(SeLibraryHandle handle)
{
    HMODULE result = {0};
    memcpy(&result, &handle, sizeof(result));
    return result;
}

static SeLibraryHandle se_load_dynamic_library(const char* path)
{
    HMODULE handle = LoadLibraryA(path);
    SeLibraryHandle result;
    memcpy(&result, &handle, sizeof(HMODULE));
    return result;
}

static void se_unload_dynamic_library(SeLibraryHandle handle)
{
    FreeLibrary(se_hmodule_from_se_handle(handle));
}

static void* se_get_dynamic_library_function_address(SeLibraryHandle handle, const char* functionName)
{
    return GetProcAddress(se_hmodule_from_se_handle(handle), functionName);
}

static uint64_t se_get_perf_counter()
{
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

static uint64_t se_get_perf_frequency()
{
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

#else // ifdef _WIN32
#   error Unsupported platform
#endif

void se_initialize(SabrinaEngine* engine)
{
    *engine =
    {
        .load_dynamic_library   = se_load_dynamic_library,
        .find_function_address  = se_get_dynamic_library_function_address,
        .subsystemStorage       = { },
        .shouldRun              = true,
    };
}

void se_run(SabrinaEngine* engine)
{
    // First load all subsystems
    for (size_t i = 0; i < engine->subsystemStorage.size; i++)
    {
        const SeLibraryHandle handle = engine->subsystemStorage.storage[i].libraryHandle;
        const SeSubsystemFunc load = (SeSubsystemFunc)se_get_dynamic_library_function_address(handle, "se_load");
        if (load) load(engine);
    }
    // Init subsystems after load (here subsytems can safely query interfaces of other subsystems)
    for (size_t i = 0; i < engine->subsystemStorage.size; i++)
    {
        const SeLibraryHandle handle = engine->subsystemStorage.storage[i].libraryHandle;
        const SeSubsystemFunc init = (SeSubsystemFunc)se_get_dynamic_library_function_address(handle, "se_init");
        if (init) init(engine);
    }
    // Update loop
    const double counterFrequency = (double)se_get_perf_frequency();
    uint64_t prevCounter = se_get_perf_counter();
    while (engine->shouldRun)
    {
        const uint64_t newCounter = se_get_perf_counter();
        const float dt = (float)((double)(newCounter - prevCounter) / counterFrequency);
        const SeUpdateInfo info { dt };
        for (size_t i = 0; i < engine->subsystemStorage.size; i++)
        {
            const SeSubsystemUpdateFunc update = engine->subsystemStorage.storage[i].update;
            if (update) update(engine, &info);
        }
        prevCounter = newCounter;
    }
    // Terminate subsystems (here subsytems can safely query interfaces of other subsystems)
    for (size_t i = engine->subsystemStorage.size; i > 0; i--)
    {
        const SeLibraryHandle handle = engine->subsystemStorage.storage[i - 1].libraryHandle;
        const SeSubsystemFunc terminate = (SeSubsystemFunc)se_get_dynamic_library_function_address(handle, "se_terminate");
        if (terminate) terminate(engine);
    }
    // Unload subsystems
    for (size_t i = engine->subsystemStorage.size; i > 0; i--)
    {
        const SeLibraryHandle handle = engine->subsystemStorage.storage[i - 1].libraryHandle;
        const SeSubsystemFunc unload = (SeSubsystemFunc)se_get_dynamic_library_function_address(handle, "se_unload");
        if (unload) unload(engine);
    }
    // Unload lib handle
    for (size_t i = engine->subsystemStorage.size; i > 0; i--)
    {
        se_unload_dynamic_library(engine->subsystemStorage.storage[i - 1].libraryHandle);
    }
}
