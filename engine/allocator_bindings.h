#ifndef _SE_ALLOCATOR_BINDINGS_H_
#define _SE_ALLOCATOR_BINDINGS_H_

#include <inttypes.h>

struct SeAllocatorBindings
{
    void* allocator;
    void* (*alloc)(void* allocator, size_t size, size_t alignment);
    void (*dealloc)(void* allocator, void* ptr, size_t size);
};

#endif
