
#include "se_debug_subsystem.hpp"
#include "engine/engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

HANDLE g_outputHandle = INVALID_HANDLE_VALUE;

void se_debug_platform_init()
{
    SetConsoleOutputCP(CP_UTF8);
    g_outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
}

void se_debug_platform_terminate()
{
    g_outputHandle = INVALID_HANDLE_VALUE;
}

void se_debug_platform_output(const char* str)
{
    static const char* nextLine = "\n";
    static const DWORD nextLineLen = (DWORD)strlen(nextLine);
    WriteFile(g_outputHandle, str, (DWORD)strlen(str), NULL, NULL);
    WriteFile(g_outputHandle, nextLine, nextLineLen, NULL, NULL);
}

void se_debug_platform_thread_yeild()
{
    SwitchToThread();
}

#else
#   error Unsupported platform
#endif

SeDebugSubsystemInterface g_iface;

struct SeLogEntry
{
    static constexpr size_t MEMORY_SIZE = 1024;
    char memory[MEMORY_SIZE];
};

ThreadSafeQueue<SeLogEntry> g_logQueue;
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
        SeLogEntry entry;
        while (thread_safe_queue::dequeue(g_logQueue, &entry))
        {
            se_debug_platform_output(entry.memory);
        }
        platform::get()->atomic_64_bit_store(&g_isFlushing, 0, SE_RELEASE);
        return true;
    }
    // return false if someone is already flushing message queue
    return false;
}

void se_debug_submit(const char* fmt, const char** args, size_t numArgs)
{
    //
    // Prepare log entry
    //

    enum ParserState
    {
        IN_TEXT,
        IN_ARG,
    };

    SeLogEntry entry;
    const size_t fmtLength = strlen(fmt);
    ParserState state = IN_TEXT;
    size_t argIt = 0;
    size_t bufferPivot = 0;
    size_t fmtPivot = 0;

    auto copyToBuffer = [&entry, &bufferPivot](const char* text, size_t length)
    {
        const size_t availableSize = SeLogEntry::MEMORY_SIZE - bufferPivot - 1;
        const size_t copySize = availableSize > length ? length : availableSize;
        memcpy(entry.memory + bufferPivot, text, copySize);
        bufferPivot += copySize;
    };

    for (size_t it = 0; it < fmtLength; it++)
    {
        switch (state)
        {
            case IN_TEXT:
            {
                if (fmt[it] == '{')
                {
                    const size_t textCopySize = it - fmtPivot;
                    const char* arg = args[argIt++];
                    copyToBuffer(fmt + fmtPivot, textCopySize);
                    copyToBuffer(arg, strlen(arg));
                    state = IN_ARG;
                }
            } break;
            case IN_ARG:
            {
                if (fmt[it] == '}')
                {
                    fmtPivot = it + 1;
                    state = IN_TEXT;
                }
            } break;
        }
    }

    copyToBuffer(fmt + fmtPivot, fmtLength - fmtPivot);
    entry.memory[bufferPivot] = 0;

    //
    // Push entry to log queue
    //

    se_debug_wait_for_flush();
    while (!thread_safe_queue::enqueue(g_logQueue, entry))
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
