#ifndef _SE_DEBUG_SUBSYSTEM_HPP_
#define _SE_DEBUG_SUBSYSTEM_HPP_

#include "engine/se_common_includes.hpp"

template<typename ... Args> void se_dbg_message(const char* fmt, const Args& ... args);
template<typename ... Args> void se_dbg_error(const char* fmt, const Args& ... args);

void _se_dbg_assert_impl(bool result, const char* condition, const char* file, size_t line);
void _se_dbg_assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt);

template<typename ... Args>
void _se_dbg_assert_impl(bool result, const char* condition, const char* file, size_t line, const char* fmt, const Args& ... args);

void _se_dbg_init();
void _se_dbg_update();
void _se_dbg_terminate();

#ifdef SE_DEBUG
    #define se_assert(cond) ((!!(cond)) || (_se_dbg_assert_impl(cond, #cond, __FILE__, __LINE__), 0))
    #define se_assert_msg(cond, fmt, ...) ((!!(cond)) || (_se_dbg_assert_impl(cond, #cond, __FILE__, __LINE__, fmt, __VA_ARGS__), 0))
    #define se_message(fmt, ...) se_dbg_message(fmt, __VA_ARGS__)
#else
    #define se_assert(cond) ((void)(cond))
    #define se_assert_msg(cond, fmt, ...) ((void)(cond))
    #define se_message(fmt, ...)
#endif

#endif
