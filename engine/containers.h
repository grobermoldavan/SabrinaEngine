#ifndef _SE_CONTAINERS_H_
#define _SE_CONTAINERS_H_

#include <string.h>
#include <stdlib.h>

#include "allocator_bindings.h"
#include "common_includes.h"
#include "platform.h"

/*
   sbuffer

   Based on stb's stretchy buffer.

   Supports:
   - custom allocators (alloc, realloc and dealloc functions)
   - size and capacity information retrieval
   - reserving memory
   - pushing new values to the end of buffer
   - removing values by index
   - removing values by pointer
   - clearnig array

   In code sbuffer look similar to usual pointer:
   int* sbufferInstance;

   To use sbuffer user have to construct it:
   1) se_sbuffer_construct(sbufferInstance, 4, alloc, realloc, dealloc, userDataPtr);
      This constructs sbuffer with custom allocators
   2) se_sbuffer_construct_std(sbufferInstance, 4);
      This constructs sbuffer with std allocation functions

   After user is done with sbuffer user have to destroy it:
   se_sbuffer_destroy(sbufferInstance);

   sbuffer memory layout:
   {
      struct SeAllocatorBindings allocator;
      size_t capacity;
      size_t size;
      T memory[capacity]; <---- user pointer points to the first element of this array
   }
*/

#define se_sbuffer(type) type*

void  __se_sbuffer_construct(void** arr, size_t entrySize, size_t capacity, struct SeAllocatorBindings* bindings, bool zero);
void  __se_sbuffer_destruct(void** arr, size_t entrySize);
void  __se_sbuffer_realloc(void** arr, size_t entrySize, size_t newCapacity);
void* __se_memcpy(void* dst, void* src, size_t size);
void* __se_memset(void* dst, int val, size_t size);

#define __se_info_size                          (sizeof(struct SeAllocatorBindings) + sizeof(size_t) * 2)
#define __se_raw(arr)                           (((char*)arr) - __se_info_size)
#define __se_param_ptr(arr, index)              (((size_t*)(arr)) - index)
#define __se_param(arr, index)                  (*__se_param_ptr(arr, index))
#define __se_expand(arr)                        __se_sbuffer_realloc((void**)&(arr), sizeof((arr)[0]), (arr) ? __se_param(arr, 2) * 2 : 4)
#define __se_realloc_if_full(arr)               (__se_param(arr, 1) == __se_param(arr, 2) ? __se_expand(arr), 0 : 0)
#define __se_remove_idx(arr, idx)               __se_memcpy((void*)&(arr)[idx], (void*)&(arr)[__se_param(arr, 1) - 1], sizeof((arr)[0])), __se_param(arr, 1) -= 1
#define __se_get_elem_idx(arr, valPtr)          ((valPtr) - (arr))

#define se_sbuffer_construct(arr, capacity, allocatorPtr)           __se_sbuffer_construct((void**)&(arr), sizeof((arr)[0]), capacity, allocatorPtr, false)
#define se_sbuffer_construct_zeroed(arr, capacity, allocatorPtr)    __se_sbuffer_construct((void**)&(arr), sizeof((arr)[0]), capacity, allocatorPtr, true)
#define se_sbuffer_destroy(arr)                                     __se_sbuffer_destruct((void**)&(arr), sizeof((arr)[0]))

#define se_sbuffer_set_size(arr, size)              (*__se_param_ptr(arr, 1) = size)
#define se_sbuffer_size(arr)                        ((arr) ? __se_param(arr, 1) : 0)
#define se_sbuffer_capacity(arr)                    ((arr) ? __se_param(arr, 2) : 0)
#define se_sbuffer_reserve(arr, capacity)           __se_sbuffer_realloc((void**)&(arr), sizeof((arr)[0]), capacity)
#define se_sbuffer_push(arr, val)                   (__se_realloc_if_full(arr), (arr)[__se_param(arr, 1)++] = val)
#define se_sbuffer_remove_idx(arr, idx)             (idx < __se_param(arr, 1) ? __se_remove_idx(arr, idx), 0 : 0)
#define se_sbuffer_remove_val(arr, valPtr)          ((valPtr) >= (arr) ? se_sbuffer_remove_idx(arr, __se_get_elem_idx(arr, valPtr)), 0 : 0)
#define se_sbuffer_clear(arr)                       ((arr) ? __se_memset(arr, 0, sizeof((arr)[0]) * __se_param(arr, 1)), __se_param(arr, 1) = 0, 0 : 0)

typedef void* SeAnyPtr;

void __se_sbuffer_construct(void** arr, size_t entrySize, size_t capacity, struct SeAllocatorBindings* allocator, bool zero)
{
    SeAnyPtr* mem = (SeAnyPtr*)allocator->alloc(allocator->allocator, __se_info_size + entrySize * capacity, se_default_alignment, se_alloc_tag);
    __se_memcpy(mem, allocator, sizeof(struct SeAllocatorBindings));
    size_t* params = (size_t*)(((char*)mem) + sizeof(struct SeAllocatorBindings));
    params[0] = capacity;
    params[1] = 0; // size
    *arr = ((char*)mem) + __se_info_size;
    if (zero)
        __se_memset(*arr, 0, entrySize * capacity);
}

void __se_sbuffer_destruct(void** arr, size_t entrySize)
{
    if (!(*arr)) return;
    SeAnyPtr* mem = (SeAnyPtr*)__se_raw(*arr);
    size_t capacity = se_sbuffer_capacity(*arr);
    struct SeAllocatorBindings* allocator = (struct SeAllocatorBindings*)mem;
    allocator->dealloc(allocator->allocator, mem, __se_info_size + entrySize * capacity);
}

void __se_sbuffer_realloc(void** arr, size_t entrySize, size_t newCapacity)
{
    size_t capacity = se_sbuffer_capacity(*arr);
    size_t size = se_sbuffer_size(*arr);
    if (capacity >= newCapacity) return;

    const size_t oldSize = __se_info_size + entrySize * capacity;
    const size_t newSize = __se_info_size + entrySize * newCapacity;

    SeAnyPtr* oldMem = (SeAnyPtr*)__se_raw(*arr);
    struct SeAllocatorBindings* allocator = (struct SeAllocatorBindings*)oldMem;
    SeAnyPtr* newMem = (SeAnyPtr*)allocator->alloc(allocator->allocator, newSize, se_default_alignment, se_alloc_tag);
    __se_memcpy(newMem, oldMem, oldSize);
    allocator->dealloc(allocator->allocator, oldMem, oldSize);

    size_t* params = (size_t*)(((char*)newMem) + sizeof(struct SeAllocatorBindings));
    params[0] = newCapacity;
    params[1] = size;
    *arr = ((char*)newMem) + __se_info_size;
}

void* __se_memcpy(void* dst, void* src, size_t size)
{
    return memcpy(dst, src, size);
}

void* __se_memset(void* dst, int val, size_t size)
{
    return memset(dst, val, size);
}

/*
    Expandable virtual memory.

    Basically a stack that uses OS virtual memory system and commits memory pages on demand.
*/

typedef struct SeExpandableVirtualMemory
{
    SePlatformInterface* platform;
    void* base;
    size_t reserved;
    size_t commited;
    size_t used;
} SeExpandableVirtualMemory;

void se_expandable_virtual_memory_construct(SeExpandableVirtualMemory* memory, SePlatformInterface* platform, size_t size)
{
    *memory = (SeExpandableVirtualMemory)
    {
        .platform   = platform,
        .base       = platform->mem_reserve(size),
        .reserved   = size,
        .commited   = 0,
        .used       = 0,
    };
}

void se_expandable_virtual_memory_destroy(SeExpandableVirtualMemory* memory)
{
    memory->platform->mem_release(memory->base, memory->reserved);
}

void se_expandable_virtual_memory_add(SeExpandableVirtualMemory* memory, size_t addition)
{
    memory->used += addition;
    se_assert(memory->used <= memory->reserved);
    if (memory->used > memory->commited)
    {
        const size_t memPageSize = memory->platform->get_mem_page_size();
        const size_t requredToCommit = memory->used - memory->commited;
        const size_t numPagesToCommit = 1 + ((requredToCommit - 1) / memPageSize);
        const size_t memoryToCommit = numPagesToCommit * memPageSize;
        memory->platform->mem_commit(((uint8_t*)memory->base) + memory->commited, memoryToCommit);
        memory->commited += memoryToCommit;
    }
}

/*
    Object pool.

    Container for an objects of the same size.
*/

typedef struct SeObjectPool
{
    SeExpandableVirtualMemory ledger;
    SeExpandableVirtualMemory objectMemory;
    size_t objectSize;
    size_t currentCapacity;
} SeObjectPool;

typedef struct SeObjectPoolCreateInfo
{
    SePlatformInterface* platform;
    size_t objectSize;
} SeObjectPoolCreateInfo;

void se_object_pool_construct(SeObjectPool* pool, SeObjectPoolCreateInfo* createInfo)
{
    __se_memset(pool, 0, sizeof(SeObjectPool));
    se_expandable_virtual_memory_construct(&pool->ledger, createInfo->platform, se_gigabytes(16));
    se_expandable_virtual_memory_construct(&pool->objectMemory, createInfo->platform, se_gigabytes(16));
    pool->objectSize = createInfo->objectSize;
    pool->currentCapacity = 0;

}

void se_object_pool_destroy(SeObjectPool* pool)
{
    se_expandable_virtual_memory_destroy(&pool->ledger);
    se_expandable_virtual_memory_destroy(&pool->objectMemory);
}

void se_object_pool_reset(SeObjectPool* pool)
{
    memset(pool->ledger.base, 0, pool->ledger.used);
}

#ifdef SE_DEBUG
#define se_object_pool_take(type, poolPtr) ((type*)__se_object_pool_take(poolPtr, sizeof(type)))
void* __se_object_pool_take(SeObjectPool* pool, size_t objectSize)
{
    se_assert(objectSize == pool->objectSize);
#else
#define se_object_pool_take(type, poolPtr) ((type*)__se_object_pool_take(poolPtr))
void* __se_object_pool_take(SeObjectPool* pool)
{
#endif
    for (size_t it = 0; it < pool->ledger.used; it++)
    {
        uint8_t* byte = ((uint8_t*)pool->ledger.base) + it;
        if (*byte == 255) continue;
        for (uint8_t byteIt = 0; byteIt < 8; byteIt++)
        {
            if ((*byte & (1 << byteIt)) == 0)
            {
                const size_t ledgerOffset = it * 8 + byteIt;
                const size_t memoryOffset = ledgerOffset * pool->objectSize;
                *byte |= (1 << byteIt);
                return ((uint8_t*)pool->objectMemory.base) + memoryOffset;
            }
        }
        se_assert_msg(false, "Invalid code path");
    }
    const size_t ledgerUsed = pool->ledger.used;
    const size_t ledgerOffset = ledgerUsed * 8;
    const size_t memoryOffset = ledgerOffset * pool->objectSize;
    se_expandable_virtual_memory_add(&pool->ledger, 1);
    se_expandable_virtual_memory_add(&pool->objectMemory, pool->objectSize * 8);
    uint8_t* byte = ((uint8_t*)pool->ledger.base) + ledgerUsed;
    *byte |= 1;
    return ((uint8_t*)pool->objectMemory.base) + memoryOffset;
}

#ifdef SE_DEBUG
#define se_object_pool_return(type, poolPtr, objPtr) __se_object_pool_return(poolPtr, objPtr, sizeof(type))
void __se_object_pool_return(SeObjectPool* pool, void* object, size_t objectSize)
{
    se_assert(objectSize == pool->objectSize);
#else
#define se_object_pool_return(type, poolPtr, objPtr) __se_object_pool_return(poolPtr, objPtr)
void __se_object_pool_return(SeObjectPool* pool, void* object)
{
#endif
    const intptr_t base = (intptr_t)pool->objectMemory.base;
    const intptr_t end = base + pool->objectMemory.used;
    const intptr_t candidate = (intptr_t)object;
    se_assert_msg(candidate >= base && candidate < end, "Can't return object to the pool : pointer is not in the pool range");
    const size_t memoryOffset = candidate - base;
    se_assert_msg((memoryOffset % pool->objectSize) == 0, "Can't return object to the pool : wrong pointer provided");
    const size_t ledgerOffset = memoryOffset / pool->objectSize;
    uint8_t* byte = ((uint8_t*)pool->ledger.base) + (ledgerOffset / 8);
    se_assert_msg(*byte & (1 << (ledgerOffset % 8)), "Can't return object to the pool : object is already returned");
    *byte &= ~(1 << (ledgerOffset % 8));
}

#define se_object_pool_is_taken(poolPtr, index) (*(((uint8_t*)poolPtr->ledger.base) + (index / 8)) & (1 << (index % 8)))

#define se_object_pool_access_by_index(poolPtr, index) (((uint8_t*)poolPtr->objectMemory.base) + (index * poolPtr->objectSize))

#define se_object_pool_for(poolPtr, objName, code)                          \
    for (size_t __it = 0; __it < poolPtr->ledger.used * 8; __it++)          \
        if (se_object_pool_is_taken(poolPtr, __it))                         \
        {                                                                   \
            void* objName = se_object_pool_access_by_index(poolPtr, __it); \
            code;                                                           \
        }

#endif
