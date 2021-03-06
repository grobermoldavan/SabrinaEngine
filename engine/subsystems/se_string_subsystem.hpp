#ifndef _SE_STRING_SUBSYSTEM_HPP_
#define _SE_STRING_SUBSYSTEM_HPP_

#include "engine/common_includes.hpp"
#include "engine/hash.hpp"
#include "engine/utils.hpp"
#include <concepts>
#include <type_traits>

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

namespace string_builder
{
    inline SeStringBuilder begin(SeStringLifetime lifetime = SeStringLifetime::Temporary, const char* source = nullptr)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_begin_from_source(lifetime == SeStringLifetime::Temporary, source);
    }

    inline SeStringBuilder begin(size_t capacity, SeStringLifetime lifetime = SeStringLifetime::Temporary, const char* source = nullptr)
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

    inline SeString end(SeStringBuilder& builder)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_end(builder);
    }
}

namespace string
{
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

    template <class T>
    concept cstring = std::is_same_v<T, const char*> || std::is_same_v<T, char*>;

    template<cstring T>
    SeString cast(const T& value, SeStringLifetime lifetime = SeStringLifetime::Temporary)
    {
        return string::create(value, lifetime);
    }
}

namespace utils
{
    template<>
    bool compare<SeString>(const SeString& first, const SeString& second)
    {
        return first.length == second.length && compare_raw(first.memory, second.memory, first.length);
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
    }

    template<>
    HashValue generate<SeString>(const SeString& value)
    {
        return hash_value::generate_raw({ value.memory, value.length });
    }
}

#endif
