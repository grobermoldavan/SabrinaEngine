
#include "se_debug.hpp"
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
    // @NOTE : for some unknown reason, console output with "nextLine = "\n"" (no space before \n)
    //         was bugged - sometimes next output could be written in the same line
    //         I don't know why that happened, but adding dummy space character seems to have helped
    static const char* nextLine = " \n";
    static const DWORD nextLineLen = DWORD(strlen(nextLine));
    WriteFile(g_outputHandle, str, DWORD(strlen(str)), NULL, NULL);
    WriteFile(g_outputHandle, nextLine, nextLineLen, NULL, NULL);
}

inline void se_debug_platform_thread_yeild()
{
    SwitchToThread();
}

#else
#   error Unsupported platform
#endif

struct SeLogger
{
    ThreadSafeQueue<SeString>   logQueue;
    uint64_t                    isFlushing;
    bool                        isInited;
} g_logger;

void se_debug_wait_for_flush()
{
    while (platform::atomic_64_bit_load(&g_logger.isFlushing, SE_ACQUIRE))
    {
        se_debug_platform_thread_yeild();
    }
}

bool se_debug_try_flush()
{
    uint64_t expected = 0;
    // Try to lock the writing flag
    if (platform::atomic_64_bit_cas(&g_logger.isFlushing, &expected, 1, SE_ACQUIRE_RELEASE))
    {
        // If flag is locked by this thread, print all messages
        SeString entry;
        while (thread_safe_queue::dequeue(g_logger.logQueue, &entry))
        {
            se_debug_platform_output(string::cstr(entry));
        }
        platform::atomic_64_bit_store(&g_logger.isFlushing, 0, SE_RELEASE);
        return true;
    }
    // return false if someone is already flushing message queue
    return false;
}

void se_debug_submit(SeString msg)
{
    while (!thread_safe_queue::enqueue(g_logger.logQueue, msg))
    {
        if (!se_debug_try_flush())
        {
            se_debug_wait_for_flush();
        }
    }
    se_debug_try_flush();
}

void se_debug_abort()
{
    se_debug_try_flush();
    int* crash = 0;
    *crash = 0;
}

void se_debug_assert_simple()
{
    int* a = 0;
    *a = 0;
}

namespace debug
{
    template<typename ... Args>
    void message(const char* fmt, const Args& ... args)
    {
        SeStringBuilder builder = string_builder::begin(nullptr, SeStringLifetime::TEMPORARY);
        string_builder::append_fmt(builder, fmt, args...);
        SeString formattedString = string_builder::end(builder);
        se_debug_submit(formattedString);
    }

    template<typename ... Args>
    void error(const char* fmt, const Args& ... args)
    {
        SeStringBuilder builder = string_builder::begin(nullptr, SeStringLifetime::TEMPORARY);
        string_builder::append_fmt(builder, fmt, args...);
        SeString formattedString = string_builder::end(builder);
        se_debug_submit(formattedString);
    }

    void assert_impl(bool result, const char* condition, const char* file, size_t line)
    {
        if (!g_logger.isInited) se_debug_assert_simple();
        error("Assertion failed. File : {}, line : {}, condition : {}", file, line, condition);
        se_debug_abort();
    }

    void assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt)
    {
        if (!g_logger.isInited) se_debug_assert_simple();
        error(fmt);
        assert_impl(result, condition, file, line);
    }

    template<typename ... Args>
    void assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt, const Args& ... args)
    {
        if (!g_logger.isInited) se_debug_assert_simple();
        error(fmt, args...);
        assert_impl(result, condition, file, line);
    }

    void engine::init()
    {
        se_debug_platform_init();
        thread_safe_queue::construct(g_logger.logQueue, allocators::persistent(), utils::power_of_two<4096>());
        g_logger.isFlushing = 0;
        g_logger.isInited = true;
    }

    void engine::update()
    {
        se_debug_try_flush();
    }

    void engine::terminate()
    {
        se_debug_try_flush();
        thread_safe_queue::destroy(g_logger.logQueue);
        se_debug_platform_terminate();
    }
}