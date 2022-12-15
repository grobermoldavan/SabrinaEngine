#ifndef _SE_ALLOCATOR_BINDINGS_HPP_
#define _SE_ALLOCATOR_BINDINGS_HPP_

#include "se_common_includes.hpp"

#define se_default_alignment 8

#define __PP_CAT(a, b) __PP_CAT_I(a, b)
#define __PP_CAT_I(a, b) __PP_CAT_II(~, a ## b)
#define __PP_CAT_II(p, res) res
#define __STR2(s) #s
#define __STR(s) __STR2(s)
#define se_alloc_tag __PP_CAT("File: " , __PP_CAT(__PP_CAT(__FILE__, ". Line: "), __STR(__LINE__)))

struct AllocatorBindings
{
    void* allocator;
    void* (*alloc)(void* allocator, size_t size, size_t alignment, const char* allocTag);
    void  (*dealloc)(void* allocator, void* ptr, size_t size);
};

namespace allocator_bindings
{
    template<typename T>
    T* alloc(const AllocatorBindings& bindings, const char* allocTag)
    {
        return (T*)bindings.alloc(bindings.allocator, sizeof(T), se_default_alignment, allocTag);
    }

    template<typename T>
    void dealloc(const AllocatorBindings& bindings, T* value)
    {
        bindings.dealloc(bindings.allocator, value, sizeof(T));
    }

    void* alloc(const AllocatorBindings& bindings, size_t size, const char* allocTag)
    {
        return bindings.alloc(bindings.allocator, size, se_default_alignment, allocTag);
    }

    void dealloc(const AllocatorBindings& bindings, void* value, size_t size)
    {
        bindings.dealloc(bindings.allocator, value, size);
    }
}

#endif
