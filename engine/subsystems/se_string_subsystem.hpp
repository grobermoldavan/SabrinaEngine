#ifndef _SE_STRING_SUBSYSTEM_HPP_
#define _SE_STRING_SUBSYSTEM_HPP_

#include "engine/common_includes.hpp"

#define SE_STRING_SUBSYSTEM_GLOBAL_NAME g_stringSubsystemIface

const struct SeStringSubsystemInterface* SE_STRING_SUBSYSTEM_GLOBAL_NAME = nullptr;

struct SeString
{
    char*                   memory;
    size_t                  length;
    struct SeStringData*    internaldData;
};

struct SeStringBuilder
{
    char*   memory;
    size_t  length;
    size_t  capacity;
};

struct SeStringSubsystemInterface
{
    static constexpr const char* NAME = "SeStringSubsystemInterface";

    SeString        (*create)                   (size_t length, const char* source);
    SeString        (*create_from_source)       (const char* source);
    void            (*destroy)                  (const SeString& str);
    SeString        (*create_tmp)               (size_t length, const char* source);

    SeStringBuilder (*builder_begin)            (size_t capacity, const char* source);
    SeStringBuilder (*builder_begin_from_source)(const char* source);
    void            (*builder_append)           (SeStringBuilder& builder, const char* source);
    SeString        (*builder_end)              (SeStringBuilder& builder);
};

struct SeStringSubsystem
{
    using Interface = SeStringSubsystemInterface;
    static constexpr const char* NAME = "se_string_subsystem";
};

namespace string
{
    inline char* cstr(SeString str) { return str.memory; }

    inline SeString create(const char* source)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->create_from_source(source);
    }

    inline SeString create(size_t capacity, const char* source = nullptr)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->create(capacity, source);
    }

    inline void destroy(const SeString& str)
    {
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->destroy(str);
    }

    inline SeString create_tmp(size_t length, const char* source = nullptr)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->create_tmp(length, source);
    }
}

namespace string_builder
{
    inline SeStringBuilder begin(const char* source = nullptr)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_begin_from_source(source);
    }

    inline SeStringBuilder begin(size_t capacity, const char* source = nullptr)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_begin(capacity, source);
    }

    inline void append(SeStringBuilder& builder, const char* source)
    {
        SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_append(builder, source);
    }

    inline SeString end(SeStringBuilder& builder)
    {
        return SE_STRING_SUBSYSTEM_GLOBAL_NAME->builder_end(builder);
    }
}

#endif
