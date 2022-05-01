
#include "se_logging_subsystem.hpp"
#include "se_platform_subsystem.hpp"
#include "se_application_allocators_subsystem.hpp"
#include "engine/containers.hpp"
#include "engine/debug.hpp"
#include "engine/engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

void se_log_thread_yeild()
{
    SwitchToThread();
}

#else
#   error Unsupported platform
#endif

static SeLoggingSubsystemInterface g_iface;
static const SePlatformSubsystemInterface* g_platformIface;
static const SeApplicationAllocatorsSubsystemInterface* g_allocatorsIface;

struct SeLogEntry
{
    static constexpr size_t MEMORY_SIZE = 1024;
    char memory[MEMORY_SIZE];
};

static ThreadSafeQueue<SeLogEntry> g_logQueue;
static uint64_t g_isFlushing;

static void se_log_wait_for_flush()
{
    while (g_platformIface->atomic_64_bit_load(&g_isFlushing, SE_ACQUIRE))
    {
        se_log_thread_yeild();
    }
}

static bool se_log_try_flush()
{
    uint64_t expected = 0;
    // Try to lock the writing flag
    if (g_platformIface->atomic_64_bit_cas(&g_isFlushing, &expected, 1, SE_ACQUIRE_RELEASE))
    {
        // If flag is locked by this thread, print all messages
        SeLogEntry entry;
        while (thread_safe_queue::dequeue(g_logQueue, &entry))
        {
            // @TODO : support multiple log outputs and colors
            // @TODO : support multiple log outputs and colors
            // @TODO : support multiple log outputs and colors
            // @TODO : support multiple log outputs and colors
            printf("%s\n", entry.memory);
        }
        g_platformIface->atomic_64_bit_store(&g_isFlushing, 0, SE_RELEASE);
        return true;
    }
    // return false if someone is already flushing message queue
    return false;
}

static void se_log_submit(const char* fmt, const char** args, size_t numArgs)
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

    entry.memory[bufferPivot] = 0;

    //
    // Push entry to log queue
    //

    se_log_wait_for_flush();
    while (!thread_safe_queue::enqueue(g_logQueue, entry))
    {
        if (!se_log_try_flush())
        {
            se_log_wait_for_flush();
        }
    }
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_platformIface = se_get_subsystem_interface<SePlatformSubsystemInterface>(engine);
    g_allocatorsIface = se_get_subsystem_interface<SeApplicationAllocatorsSubsystemInterface>(engine);
    g_iface =
    {
        .print = se_log_submit,
    };
    g_isFlushing = 0;
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    thread_safe_queue::construct(g_logQueue, *g_allocatorsIface->persistentAllocator, g_platformIface, utils::power_of_two<4096>());
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* updateInfo)
{
    se_log_try_flush();
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    se_log_try_flush();
    thread_safe_queue::destroy(g_logQueue);
}
