#ifndef _SE_STRING_SUBSYSTEM_HPP_
#define _SE_STRING_SUBSYSTEM_HPP_

#include "engine/se_common_includes.hpp"
#include "engine/se_hash.hpp"
#include "engine/se_utils.hpp"

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

template <typename T> concept se_cstring = std::is_convertible_v<T, const char*>;

namespace string
{
    char*                                   cstr(SeString str);
    SeString                                create(const SeString& source, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    SeString                                create(const char* source, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    template<typename ... Args> SeString    create_fmt(SeStringLifetime lifetime, const char* fmt, const Args& ... args);
    void                                    destroy(const SeString& str);

    template<typename T>                SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    template<std::unsigned_integral T>  SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    template<std::signed_integral T>    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    template<std::floating_point T>     SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    template<se_cstring T>              SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary);

    namespace engine
    {
        void init();
        void terminate();
    }
}

namespace string_builder
{
    SeStringBuilder                     begin(const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    SeStringBuilder                     begin(size_t capacity, const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::Temporary);
    void                                append(SeStringBuilder& builder, const char* source);
    void                                append(SeStringBuilder& builder, char symbol);
    void                                append(SeStringBuilder& builder, const SeString& source);
    template<typename ... Args> void    append_fmt(SeStringBuilder& builder, const char* fmt, const Args& ... args);
    SeString                            end(SeStringBuilder& builder);
}

template<se_cstring T> struct SeIsComparable<SeString, T> { static constexpr bool value = true; };
template<se_cstring T> struct SeIsComparable<T, SeString> { static constexpr bool value = true; };

namespace utils
{
    template<> bool                     compare<SeString, SeString>(const SeString& first, const SeString& second);
    template<se_cstring T> bool    compare(const SeString& first, const T& second);
    template<se_cstring T> bool    compare(const T& first, const SeString& second);
}

namespace hash_value
{
    namespace builder
    {
        template<> void                     absorb<SeString>(HashValueBuilder& builder, const SeString& value);
        template<se_cstring T> void    absorb(HashValueBuilder& builder, const T& value);
    }
    template<> HashValue                    generate<SeString>(const SeString& value);
    template<se_cstring T> HashValue   generate(const T& value);
}

#endif
