#ifndef _SE_STRING_SUBSYSTEM_HPP_
#define _SE_STRING_SUBSYSTEM_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_hash.hpp"
#include "engine/se_utils.hpp"

enum struct SeStringLifetime
{
    TEMPORARY,
    PERSISTENT,
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

namespace string
{
    const char*                             cstr(const SeString& str);
    char*                                   cstr(SeString& str);
    size_t                                  length(const SeString& str);
    SeString                                create(const SeString& source, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    SeString                                create(const char* source, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    template<typename ... Args> SeString    create_fmt(SeStringLifetime lifetime, const char* fmt, const Args& ... args);
    SeString                                create(size_t length, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    void                                    destroy(const SeString& str);

    template<typename T>                SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    template<std::unsigned_integral T>  SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    template<std::signed_integral T>    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    template<std::floating_point T>     SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    template<se_cstring T>              SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    template<>                          SeString cast<SeString>(const SeString& value, SeStringLifetime lifetime);
    template<>                          SeString cast<nullptr_t>(const nullptr_t& value, SeStringLifetime lifetime);

    namespace engine
    {
        void init();
        void terminate();
    }
}

namespace string_builder
{
    SeStringBuilder                     begin(const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    SeStringBuilder                     begin(size_t capacity, const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
    void                                append(SeStringBuilder& builder, const char* source);
    void                                append(SeStringBuilder& builder, char symbol);
    void                                append(SeStringBuilder& builder, const SeString& source);
    template<typename ... Args> void    append_fmt(SeStringBuilder& builder, const char* fmt, const Args& ... args);
    template<typename ... Args> void    append_with_separator(SeStringBuilder& builder, const char* separator, const Args& ... args);
    SeString                            end(SeStringBuilder& builder);
}

template<se_cstring T> struct SeIsComparable<SeString, T> { static constexpr bool value = true; };
template<se_cstring T> struct SeIsComparable<T, SeString> { static constexpr bool value = true; };
template<se_cstring First, se_cstring Second> struct SeIsComparable<First, Second> { static constexpr bool value = true; };

namespace utils
{
    template<> bool                                     compare<SeString, SeString>(const SeString& first, const SeString& second);
    template<se_cstring T> bool                         compare(const SeString& first, const T& second);
    template<se_cstring T> bool                         compare(const T& first, const SeString& second);
    template<se_cstring First, se_cstring Second> bool  compare(const First& first, const Second& second);
}

namespace hash_value
{
    namespace builder
    {
        template<> void             absorb<SeString>(HashValueBuilder& builder, const SeString& value);
        template<se_cstring T> void absorb(HashValueBuilder& builder, const T& value);
    }
    template<> HashValue                generate<SeString>(const SeString& value);
    template<se_cstring T> HashValue    generate(const T& value);
}

// =========================================================================================
//
// This is a special stripped-down version of char string, but with wchar_t type
//
// =========================================================================================

struct SeStringW
{
    wchar_t* buffer;
    size_t length;
};

namespace stringw
{
    template<typename ... Args>
    SeStringW create(const AllocatorBindings& allocator, const Args& ... args);

    const wchar_t*  cstr(const SeStringW& str);
    SeString        to_utf8(const SeStringW& str);
    SeStringW       from_utf8(const AllocatorBindings& allocator, const char* source);
    void            destroy(SeStringW& str);
}

#endif
