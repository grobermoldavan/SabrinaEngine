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

const char*                             se_string_cstr(const SeString& str);
char*                                   se_string_cstr(SeString& str);
size_t                                  se_string_length(const SeString& str);
SeString                                se_string_create(const SeString& source, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
SeString                                se_string_create(const char* source, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
template<typename ... Args> SeString    se_string_create_fmt(SeStringLifetime lifetime, const char* fmt, const Args& ... args);
SeString                                se_string_create(size_t length, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
void                                    se_string_destroy(const SeString& str);

template<typename T>                SeString se_string_cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
template<std::unsigned_integral T>  SeString se_string_cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
template<std::signed_integral T>    SeString se_string_cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
template<std::floating_point T>     SeString se_string_cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
template<se_cstring T>              SeString se_string_cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
template<>                          SeString se_string_cast<SeString>(const SeString& value, SeStringLifetime lifetime);
template<>                          SeString se_string_cast<nullptr_t>(const nullptr_t& value, SeStringLifetime lifetime);

void _se_string_init();
void _se_string_terminate();

SeStringBuilder                     se_string_builder_begin(const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
SeStringBuilder                     se_string_builder_begin(size_t capacity, const char* source = nullptr, SeStringLifetime lifetime = SeStringLifetime::TEMPORARY);
void                                se_string_builder_append(SeStringBuilder& builder, const char* source);
void                                se_string_builder_append(SeStringBuilder& builder, char symbol);
void                                se_string_builder_append(SeStringBuilder& builder, const SeString& source);
template<typename ... Args> void    se_string_builder_append_fmt(SeStringBuilder& builder, const char* fmt, const Args& ... args);
template<typename ... Args> void    se_string_builder_append_with_separator(SeStringBuilder& builder, const char* separator, const Args& ... args);
SeString                            se_string_builder_end(SeStringBuilder& builder);

template<se_cstring T> struct SeIsComparable<SeString, T> { static constexpr bool value = true; };
template<se_cstring T> struct SeIsComparable<T, SeString> { static constexpr bool value = true; };
template<se_cstring First, se_cstring Second> struct SeIsComparable<First, Second> { static constexpr bool value = true; };

template<> bool                                     se_compare<SeString, SeString>(const SeString& first, const SeString& second);
template<se_cstring T> bool                         se_compare(const SeString& first, const T& second);
template<se_cstring T> bool                         se_compare(const T& first, const SeString& second);
template<se_cstring First, se_cstring Second> bool  se_compare(const First& first, const Second& second);

template<> void                     se_hash_value_builder_absorb<SeString>(SeHashValueBuilder& builder, const SeString& value);
template<se_cstring T> void         se_hash_value_builder_absorb(SeHashValueBuilder& builder, const T& value);
template<> SeHashValue              se_hash_value_generate<SeString>(const SeString& value);
template<se_cstring T> SeHashValue  se_hash_value_generate(const T& value);

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

template<typename ... Args>
SeStringW se_stringw_create(const SeAllocatorBindings& allocator, const Args& ... args);

const wchar_t*  se_stringw_cstr(const SeStringW& str);
SeString        se_stringw_to_utf8(const SeStringW& str);
SeStringW       se_stringw_from_utf8(const SeAllocatorBindings& allocator, const char* source);
void            se_stringw_destroy(SeStringW& str);

#endif
