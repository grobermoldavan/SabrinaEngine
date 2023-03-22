
#include "se_debug.hpp"
#include "engine/se_engine.hpp"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

HANDLE g_outputHandle = INVALID_HANDLE_VALUE;

inline void _se_dbg_platform_init()
{
    SetConsoleOutputCP(CP_UTF8);
    g_outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
}

inline void _se_dbg_platform_terminate()
{
    g_outputHandle = INVALID_HANDLE_VALUE;
}

inline void _se_dbg_platform_output(const char* str)
{
    // @NOTE : for some unknown reason, console output with "nextLine = "\n"" (no space before \n)
    //         was bugged - sometimes next output could be written in the same line
    //         I don't know why that happened, but adding dummy space character seems to have helped
    static const char* nextLine = " \n";
    static const DWORD nextLineLen = DWORD(strlen(nextLine));
    WriteFile(g_outputHandle, str, DWORD(strlen(str)), NULL, NULL);
    WriteFile(g_outputHandle, nextLine, nextLineLen, NULL, NULL);
}

inline void _se_dbg_platform_thread_yeild()
{
    SwitchToThread();
}

#else
#   error Unsupported platform
#endif

struct SeLogger
{
    SeThreadSafeQueue<SeString>   logQueue;
    uint64_t                    isFlushing;
    bool                        isInited;
} g_logger;

void _se_dbg_wait_for_flush()
{
    while (se_platform_atomic_64_bit_load(&g_logger.isFlushing, SE_ACQUIRE))
    {
        _se_dbg_platform_thread_yeild();
    }
}

bool _se_dbg_try_flush()
{
    uint64_t expected = 0;
    // Try to lock the writing flag
    if (se_platform_atomic_64_bit_cas(&g_logger.isFlushing, &expected, 1, SE_ACQUIRE_RELEASE))
    {
        // If flag is locked by this thread, print all messages
        SeString entry;
        while (se_thread_safe_queue_dequeue(g_logger.logQueue, &entry))
        {
            _se_dbg_platform_output(se_string_cstr(entry));
        }
        se_platform_atomic_64_bit_store(&g_logger.isFlushing, 0, SE_RELEASE);
        return true;
    }
    // return false if someone is already flushing message queue
    return false;
}

void _se_dbg_submit(SeString msg)
{
    while (!se_thread_safe_queue_enqueue(g_logger.logQueue, msg))
    {
        if (!_se_dbg_try_flush())
        {
            _se_dbg_wait_for_flush();
        }
    }
    _se_dbg_try_flush();
}

void _se_dbg_abort()
{
    _se_dbg_try_flush();
    int* crash = 0;
    *crash = 0;
}

void _se_dbg_assert_simple()
{
    int* a = 0;
    *a = 0;
}

template<typename ... Args>
void se_dbg_message(const char* fmt, const Args& ... args)
{
    SeStringBuilder builder = se_string_builder_begin(nullptr, SeStringLifetime::TEMPORARY);
    se_string_builder_append_fmt(builder, fmt, args...);
    SeString formattedString = se_string_builder_end(builder);
    _se_dbg_submit(formattedString);
}

template<typename ... Args>
void se_dbg_error(const char* fmt, const Args& ... args)
{
    SeStringBuilder builder = se_string_builder_begin(nullptr, SeStringLifetime::TEMPORARY);
    se_string_builder_append_fmt(builder, fmt, args...);
    SeString formattedString = se_string_builder_end(builder);
    _se_dbg_submit(formattedString);
}

void _se_dbg_assert_impl(bool result, const char* condition, const char* file, size_t line)
{
    if (!g_logger.isInited) _se_dbg_assert_simple();
    se_dbg_error("Assertion failed. File : {}, line : {}, condition : {}", file, line, condition);
    _se_dbg_abort();
}

void _se_dbg_assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt)
{
    if (!g_logger.isInited) _se_dbg_assert_simple();
    se_dbg_error(fmt);
    _se_dbg_assert_impl(result, condition, file, line);
}

template<typename ... Args>
void _se_dbg_assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt, const Args& ... args)
{
    if (!g_logger.isInited) _se_dbg_assert_simple();
    se_dbg_error(fmt, args...);
    _se_dbg_assert_impl(result, condition, file, line);
}

void _se_dbg_init()
{
    _se_dbg_platform_init();
    se_thread_safe_queue_construct(g_logger.logQueue, se_allocator_persistent(), se_power_of_two<4096>());
    g_logger.isFlushing = 0;
    g_logger.isInited = true;
}

inline void _se_dbg_update()
{
    _se_dbg_try_flush();
}

inline void _se_dbg_terminate()
{
    _se_dbg_try_flush();
    se_thread_safe_queue_destroy(g_logger.logQueue);
    _se_dbg_platform_terminate();
}