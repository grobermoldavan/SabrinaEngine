
#include <string.h>
#include <stdlib.h>

#include "se_containers_susbsytem.h"

typedef void* SeAnyPtr;

SE_DLL_EXPORT void __se_sbuffer_construct(void** arr, size_t entrySize, size_t capacity, SbufferAllocate alloc, SbufferReallocate realloc, SbufferDeallocate dealloc, void* userData)
{
    SeAnyPtr* mem = (SeAnyPtr*)(alloc ? alloc(__se_info_size + entrySize * capacity, userData) : malloc(__se_info_size + entrySize * capacity));
    mem[0] = alloc;
    mem[1] = realloc;
    mem[2] = dealloc;
    mem[3] = userData;
    size_t* params = (size_t*)(&mem[4]);
    params[0] = capacity;
    params[1] = 0; // size
    *arr = ((char*)mem) + __se_info_size;
}

SE_DLL_EXPORT void __se_sbuffer_destruct(void** arr, size_t entrySize)
{
    if (!(*arr)) return;
    SeAnyPtr* mem = (SeAnyPtr*)__se_raw(*arr);
    size_t capacity = se_sbuffer_capacity(*arr);
    if (mem[2])
        ((SbufferDeallocate)mem[2])(mem, __se_info_size + entrySize * capacity, mem[3]);
    else
        free(mem);
}

SE_DLL_EXPORT void __se_sbuffer_realloc(void** arr, size_t entrySize, size_t newCapacity)
{
    size_t capacity = se_sbuffer_capacity(*arr);
    size_t size = se_sbuffer_size(*arr);
    if (capacity >= newCapacity) return;
    SeAnyPtr* mem = (SeAnyPtr*)__se_raw(*arr);
    SeAnyPtr* newMem;
    if (mem[1])
        newMem = (SeAnyPtr*)((SbufferReallocate)mem[1])(mem,__se_info_size + entrySize * capacity,  __se_info_size + entrySize * newCapacity, mem[3]);
    else
        newMem = (SeAnyPtr*)realloc(mem, __se_info_size + entrySize * newCapacity);
    size_t* params = (size_t*)(&newMem[4]);
    params[0] = newCapacity;
    params[1] = size;
    *arr = ((char*)newMem) + __se_info_size;
}

SE_DLL_EXPORT void* __se_memcpy(void* dst, void* src, size_t size)
{
    return memcpy(dst, src, size);
}

SE_DLL_EXPORT void* __se_memset(void* dst, int val, size_t size)
{
    return memset(dst, val, size);
}
