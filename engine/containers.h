#ifndef _SE_CONTAINERS_H_
#define _SE_CONTAINERS_H_

#include "engine/common_includes.h"

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
      void* alloc;
      void* realloc;
      void* dealloc;
      void* userData;
      size_t capacity;
      size_t size;
      T memory[capacity]; <---- user pointer points to the first element of this array
   }
*/

typedef void*(*SbufferAllocate)(size_t sizeBytes, void* userData);
typedef void*(*SbufferReallocate)(void* ptr, size_t oldSize, size_t newSize, void* userData);
typedef void (*SbufferDeallocate)(void* ptr, size_t size, void* userData);

void  __se_sbuffer_construct(void** arr, size_t entrySize, size_t capacity, SbufferAllocate alloc, SbufferReallocate realloc, SbufferDeallocate dealloc, void* userData);
void  __se_sbuffer_destruct(void** arr, size_t entrySize);
void  __se_sbuffer_realloc(void** arr, size_t entrySize, size_t newCapacity);
void* __se_memcpy(void* dst, void* src, size_t size);
void* __se_memset(void* dst, int val, size_t size);

#define __se_info_size                          (sizeof(void*) * 4 + sizeof(size_t) * 2)
#define __se_raw(arr)                           (((char*)arr) - __se_info_size)
#define __se_param_ptr(arr, index)              (((size_t*)(arr)) - index)
#define __se_param(arr, index)                  (*__se_param_ptr(arr, index))
#define __se_expand(arr)                        __se_sbuffer_realloc(&(arr), sizeof((arr)[0]), (arr) ? __se_param(arr, 2) * 2 : 4)
#define __se_realloc_if_full(arr)               (__se_param(arr, 1) == __se_param(arr, 2) ? __se_expand(arr), 0 : 0)
#define __se_remove_idx(arr, idx)               __se_memcpy(&(arr)[idx], &(arr)[__se_param(arr, 1) - 1], sizeof((arr)[0])), __se_param(arr, 1) -= 1
#define __se_get_elem_idx(arr, valPtr)          ((valPtr) - (arr))

#define se_sbuffer_construct_std(arr, capacity)                                 __se_sbuffer_construct(&(arr), sizeof((arr)[0]), capacity, NULL, NULL, NULL, NULL)
#define se_sbuffer_construct(arr, capacity, alloc, realloc, dealloc, userData)  __se_sbuffer_construct(&(arr), sizeof((arr)[0]), capacity, alloc, realloc, dealloc, userData)
#define se_sbuffer_destroy(arr)                                                 __se_sbuffer_destruct(&(arr), sizeof((arr)[0]))

#define se_sbuffer_size(arr)                    ((arr) ? __se_param(arr, 1) : 0)
#define se_sbuffer_capacity(arr)                ((arr) ? __se_param(arr, 2) : 0)
#define se_sbuffer_reserve(arr, capacity)       __se_sbuffer_realloc(&(arr), sizeof((arr)[0]), capacity)
#define se_sbuffer_push(arr, val)               (__se_realloc_if_full(arr), (arr)[__se_param(arr, 1)++] = val)
#define se_sbuffer_remove_idx(arr, idx)         (idx < __se_param(arr, 1) ? __se_remove_idx(arr, idx), 0 : 0)
#define se_sbuffer_remove_val(arr, valPtr)      ((valPtr) >= (arr) ? se_sbuffer_remove_idx(arr, __se_get_elem_idx(arr, valPtr)), 0 : 0)
#define se_sbuffer_clear(arr)                   ((arr) ? __se_memset(arr, 0, sizeof((arr)[0]) * __se_param(arr, 1)), __se_param(arr, 1) = 0, 0 : 0)

#endif
