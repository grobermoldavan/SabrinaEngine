#ifndef _SE_DEBUG_SUBSYSTEM_HPP_
#define _SE_DEBUG_SUBSYSTEM_HPP_

#include "se_string_subsystem.hpp"

struct SeDebugSubsystemInterface
{
    static constexpr const char* NAME = "SeDebugSubsystemInterface";

    void (*print)(SeString msg);
    void (*abort)();
    bool isInited;
};

struct SeDebugSubsystem
{
    using Interface = SeDebugSubsystemInterface;
    static constexpr const char* NAME = "se_debug_subsystem";
};

#define SE_DEBUG_SUBSYSTEM_GLOBAL_NAME g_debugSubsystemInterface
const struct SeDebugSubsystemInterface* SE_DEBUG_SUBSYSTEM_GLOBAL_NAME;

namespace debug
{
    namespace impl
    {
        void assert_simple()
        {
            int* a = 0;
            *a = 0;
        }
    }

    template<typename ... Args>
    void message(const char* fmt, const Args& ... args)
    {
        SeStringBuilder builder = string_builder::begin(nullptr, SeStringLifetime::Temporary);
        string_builder::append_fmt(builder, fmt, args...);
        SeString formattedString = string_builder::end(builder);
        SE_DEBUG_SUBSYSTEM_GLOBAL_NAME->print(formattedString);
    }

    template<typename ... Args>
    void error(const char* fmt, const Args& ... args)
    {
        SeStringBuilder builder = string_builder::begin(nullptr, SeStringLifetime::Temporary);
        string_builder::append_fmt(builder, fmt, args...);
        SeString formattedString = string_builder::end(builder);
        SE_DEBUG_SUBSYSTEM_GLOBAL_NAME->print(formattedString);
    }

    void assert_impl(bool result, const char* condition, const char* file, size_t line)
    {
        if (!SE_DEBUG_SUBSYSTEM_GLOBAL_NAME || !SE_DEBUG_SUBSYSTEM_GLOBAL_NAME->isInited) impl::assert_simple();
        error("Assertion failed. File : {}, line : {}, condition : {}", file, line, condition);
        SE_DEBUG_SUBSYSTEM_GLOBAL_NAME->abort();
    }

    void assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt)
    {
        if (!SE_DEBUG_SUBSYSTEM_GLOBAL_NAME || !SE_DEBUG_SUBSYSTEM_GLOBAL_NAME->isInited) impl::assert_simple();
        error(fmt);
        assert_impl(result, condition, file, line);
    }

    template<typename ... Args>
    void assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt, const Args& ... args)
    {
        if (!SE_DEBUG_SUBSYSTEM_GLOBAL_NAME || !SE_DEBUG_SUBSYSTEM_GLOBAL_NAME->isInited) impl::assert_simple();
        error(fmt, args...);
        assert_impl(result, condition, file, line);
    }
}

#ifdef SE_DEBUG
    #define se_assert(cond) ((!!(cond)) || (debug::assert_impl(cond, #cond, __FILE__, __LINE__), 0))
    #define se_assert_msg(cond, fmt, ...) ((!!(cond)) || (debug::assert_impl(cond, #cond, __FILE__, __LINE__, fmt, __VA_ARGS__), 0))
#else
    #define se_assert(cond) ((void)(cond))
    #define se_assert_msg(cond, fmt, ...) ((void)(cond))
#endif

#endif
