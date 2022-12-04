#ifndef _SE_DEBUG_SUBSYSTEM_HPP_
#define _SE_DEBUG_SUBSYSTEM_HPP_

#include "engine/se_common_includes.hpp"

namespace debug
{
    template<typename ... Args> void message(const char* fmt, const Args& ... args);
    template<typename ... Args> void error(const char* fmt, const Args& ... args);

    void assert_impl(bool result, const char* condition, const char* file, size_t line);
    void assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt);

    template<typename ... Args>
    void assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt, const Args& ... args);

    namespace engine
    {
        void init();
        void update();
        void terminate();
    }
}

#ifdef SE_DEBUG
    #define se_assert(cond) ((!!(cond)) || (debug::assert_impl(cond, #cond, __FILE__, __LINE__), 0))
    #define se_assert_msg(cond, fmt, ...) ((!!(cond)) || (debug::assert_impl(cond, #cond, __FILE__, __LINE__, fmt, __VA_ARGS__), 0))
    #define se_message(fmt, ...) debug::message(fmt, __VA_ARGS__)
#else
    #define se_assert(cond) ((void)(cond))
    #define se_assert_msg(cond, fmt, ...) ((void)(cond))
    #define se_message(fmt, ...)
#endif

#endif
