
#include "se_debug_subsystem.hpp"
#include "engine/engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

HANDLE g_outputHandle = INVALID_HANDLE_VALUE;

inline void se_debug_platform_init()
{
    SetConsoleOutputCP(CP_UTF8);
    g_outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
}

inline void se_debug_platform_terminate()
{
    g_outputHandle = INVALID_HANDLE_VALUE;
}

inline void se_debug_platform_output(const char* str)
{
    static const char* nextLine = "\n";
    static const DWORD nextLineLen = (DWORD)strlen(nextLine);
    WriteFile(g_outputHandle, str, (DWORD)strlen(str), NULL, NULL);
    WriteFile(g_outputHandle, nextLine, nextLineLen, NULL, NULL);
}

inline void se_debug_platform_thread_yeild()
{
    SwitchToThread();
}

#else
#   error Unsupported platform
#endif

SeDebugSubsystemInterface g_iface;

ThreadSafeQueue<SeString> g_logQueue;
uint64_t g_isFlushing;

void se_debug_wait_for_flush()
{
    while (platform::get()->atomic_64_bit_load(&g_isFlushing, SE_ACQUIRE))
    {
        se_debug_platform_thread_yeild();
    }
}

bool se_debug_try_flush()
{
    uint64_t expected = 0;
    // Try to lock the writing flag
    if (platform::get()->atomic_64_bit_cas(&g_isFlushing, &expected, 1, SE_ACQUIRE_RELEASE))
    {
        // If flag is locked by this thread, print all messages
        SeString entry;
        while (thread_safe_queue::dequeue(g_logQueue, &entry))
        {
            se_debug_platform_output(string::cstr(entry));
        }
        platform::get()->atomic_64_bit_store(&g_isFlushing, 0, SE_RELEASE);
        return true;
    }
    // return false if someone is already flushing message queue
    return false;
}

void se_debug_submit(SeString msg)
{
    while (!thread_safe_queue::enqueue(g_logQueue, msg))
    {
        if (!se_debug_try_flush())
        {
            se_debug_wait_for_flush();
        }
    }
}

void se_debug_abort()
{
    se_debug_try_flush();
    int* crash = 0;
    *crash = 0;
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .print      = se_debug_submit,
        .abort      = se_debug_abort,
        .isInited   = false,
    };
    g_isFlushing = 0;
    se_init_global_subsystem_pointers(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_debug_platform_init();
    thread_safe_queue::construct(g_logQueue, app_allocators::persistent(), utils::power_of_two<4096>());
    g_iface.isInited = true;
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* updateInfo)
{
    se_debug_try_flush();
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    se_debug_try_flush();
    thread_safe_queue::destroy(g_logQueue);
    se_debug_platform_terminate();
}
