#ifndef _SE_STRING_SUBSYSTEM_HPP_
#define _SE_STRING_SUBSYSTEM_HPP_

#include "engine/common_includes.hpp"
#include "engine/hash.hpp"
#include "engine/utils.hpp"

enum struct SeStringLifetime
{
    Temporary,
    Persistent,
};

struct SeString
{
    char*    memory;
    size_t   length;
    void*    internaldData;
};

struct SeStringBuilder
{
    char*   memory;
    size_t  length;
    size_t  capacity;
    bool    isTmp;
};

struct SeStringSubsystemInterface
{
    static constexpr const char* NAME = "SeStringSubsystemInterface";

    SeString        (*create)                   (bool isTmp, size_t length, const char* source);
    SeString        (*create_from_source)       (bool isTmp, const char* source);
    void            (*destroy)                  (const SeString& str);

    SeStringBuilder (*builder_begin)            (bool isTmp, size_t capacity, const char* source);
    SeStringBuilder (*builder_begin_from_source)(bool isTmp, const char* source);
    void            (*builder_append)           (SeStringBuilder& builder, const char* source);
    void            (*builder_append_fmt)       (SeStringBuilder& builder, const char* fmt, const char** args, size_t numArgs);
    SeString        (*builder_end)              (SeStringBuilder& builder);

    void            (*int64_to_cstr)            (int64_t value, char* buffer, size_t bufferSize);
    void            (*uint64_to_cstr)           (uint64_t value, char* buffer, size_t bufferSize);
    void            (*double_to_cstr)           (double value, char* buffer, size_t bufferSize);
};

struct SeStringSubsystem
{
    using Interface = SeStringSubsystemInterface;
    static constexpr const char* NAME = "se_string_subsystem";
};

#define SE_STRING_SUBSYSTEM_GLOBAL_NAME g_stringSubsystemIface
const struct SeStringSubsystemInterface* SE_STRING_SUBSYSTEM_GLOBAL_NAME = nullptr;

namespace string
{
    template <class T>
    concept cstring = std::is_convertible_v<T, const char*>;

    inline char* cstr(SeString str)
    {
        return str.memory;
    }

    inline SeString create(const SeString& source, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->create_from_source(lifetime == SeStringLifetime::Temporary, string::cstr(source));
    }

    inline SeString create(const char* source, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->create_from_source(lifetime == SeStringLifetime::Temporary, source);
    }

    inline void destroy(const SeString& str)
    {
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->destroy(str);
    }

    template<typename T>
    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        return string::create("!!! string::cast operation is not specified !!!");
    }

    template<std::unsigned_integral T>
    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        char buffer[32];
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->uint64_to_cstr(value, buffer, sizeof(buffer));
        return string::create(buffer, lifetime);
    }

    template<std::signed_integral T>
    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        char buffer[32];
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->int64_to_cstr(value, buffer, sizeof(buffer));
        return string::create(buffer, lifetime);
    }

    template<std::floating_point T>
    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        char buffer[64];
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->double_to_cstr(value, buffer, sizeof(buffer));
        return string::create(buffer, lifetime);
    }

    template<cstring T>
    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        return string::create(value, lifetime);
    }
}

namespace string_builder
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

    inline SeStringBuilder begin(const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_begin_from_source(lifetime == SeStringLifetime::Temporary, source);
    }

    inline SeStringBuilder begin(size_t capacity, const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_begin(lifetime == SeStringLifetime::Temporary, capacity, source);
    }

    inline void append(SeStringBuilder& builder, const char* source)
    {
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_append(builder, source);
    }

    inline void append(SeStringBuilder& builder, char symbol)
    {
        const char smallStr[2] = { symbol, 0 };
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_append(builder, smallStr);
    }

    inline void append(SeStringBuilder& builder, const SeString& source)
    {
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_append(builder, source.memory);
    }

    template<typename ... Args>
    void append_fmt(SeStringBuilder& builder, const char* fmt, const Args& ... args)
    {
        if constexpr (sizeof...(Args) == 0)
        {
            SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_append(builder, fmt);
        }
        else
        {
            char* argsStr[sizeof...(args)];
            impl::fill_args<0, Args...>(argsStr, args...);
            SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_append_fmt(builder, fmt, (const char**)argsStr, sizeof...(args));
        }
    }

    inline SeString end(SeStringBuilder& builder)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_end(builder);
    }
}

namespace string
{
    template<typename ... Args>
    SeString create_fmt(SeStringLifetime lifetime, const char* fmt, const Args& ... args)
    {
        SeStringBuilder builder = string_builder::begin(nullptr, lifetime);
        string_builder::append_fmt(builder, fmt, args...);
        return string_builder::end(builder);
    }
}

namespace utils
{
    template<string::cstring T> struct IsComparable<SeString, T> { static constexpr bool value = true; };
    template<string::cstring T> struct IsComparable<T, SeString> { static constexpr bool value = true; };

    template<>
    bool compare<SeString, SeString>(const SeString& first, const SeString& second)
    {
        return first.length == second.length && compare_raw(first.memory, second.memory, first.length);
    }

    template<string::cstring T>
    bool compare(const SeString& first, const T& second)
    {
        const char* const cstr = (const char*)second;
        return first.length == strlen(cstr) && compare_raw(first.memory, cstr, first.length);
    }

    template<string::cstring T>
    bool compare(const T& first, const SeString& second)
    {
        const char* const cstr = (const char*)first;
        return strlen(cstr) == second.length && compare_raw(cstr, second.memory, second.length);
    }
}

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeString>(HashValueBuilder& builder, const SeString& value)
        {
            hash_value::builder::absorb_raw(builder, { value.memory, value.length });
        }

        template<string::cstring T>
        void absorb(HashValueBuilder& builder, const T& value)
        {
            const char* const cstr = (const char*)value;
            hash_value::builder::absorb_raw(builder, { (void*)cstr, strlen(cstr) });
        }
    }

    template<>
    HashValue generate<SeString>(const SeString& value)
    {
        return hash_value::generate_raw({ value.memory, value.length });
    }

    template<string::cstring T>
    HashValue generate(const T& value)
    {
        const char* const cstr = (const char*)value;
        return hash_value::generate_raw({ (void*)cstr, strlen(cstr) });
    }
}

#endif
