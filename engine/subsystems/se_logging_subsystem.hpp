#ifndef _SE_LOGGING_SUBSYSTEM_HPP_
#define _SE_LOGGING_SUBSYSTEM_HPP_

#include "se_string_subsystem.hpp"

#define SE_LOGGING_SUBSYSTEM_GLOBAL_NAME g_loggingSubsystemIface

const struct SeLoggingSubsystemInterface* SE_LOGGING_SUBSYSTEM_GLOBAL_NAME;

struct SeLoggingSubsystemInterface
{
    static constexpr const char* NAME = "SeLoggingSubsystemInterface";

    void (*print)(const char* fmt, const char** args, size_t numArgs);
};

struct SeLoggingSubsystem
{
    using Interface = SeLoggingSubsystemInterface;
    static constexpr const char* NAME = "se_logging_subsystem";
};

namespace logger
{
    namespace impl
    {
        template<size_t it, typename Arg>
        void fill_args(char** argsStr, const Arg& arg)
        {
            argsStr[it] = string::cstr(string::cast(arg, SeStringLifetime::Temporary));
        }

        template<size_t it, typename Arg, typename ... Other>
        void fill_args(char** argsStr, const Arg& arg, const Other& ... other)
        {
            argsStr[it] = string::cstr(string::cast(arg, SeStringLifetime::Temporary));
            fill_args<it + 1, Other...>(argsStr, other...);
        }
    }

    template<typename ... Args>
    void message(const char* fmt, const Args& ... args)
    {
        char* argsStr[sizeof...(args)] = { };
        logger::impl::fill_args<0, Args...>(argsStr, args...);
        SE_LOGGING_SUBSYSTEM_GLOBAL_NAME->print(fmt, (const char**)argsStr, sizeof...(args));
    }

    void message(const char* fmt)
    {
        SE_LOGGING_SUBSYSTEM_GLOBAL_NAME->print(fmt, nullptr, 0);
    }
}

#endif
