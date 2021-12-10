#ifndef _SE_COMMON_INCLUDES_H_
#define _SE_COMMON_INCLUDES_H_

#include <inttypes.h>
#include <stdbool.h>

#define se_array_size(arr) (sizeof(arr) / sizeof(arr[0]))

#define se_concat(first, second) first##second
#define se_to_str(text) #text

#define se_add_subsystem(subsystemName, enginePtr)                      \
{                                                                       \
    SeHandle libHandle = (enginePtr)->platformIface.dynamic_library_load(subsystemName); \
    SeSubsystemUpdateFunc update = (SeSubsystemUpdateFunc)(enginePtr)->platformIface.dynamic_library_get_function_address(libHandle, "se_update"); \
    SeSubsystemReturnPtrFunc getInterface = (SeSubsystemReturnPtrFunc)(enginePtr)->platformIface.dynamic_library_get_function_address(libHandle, "se_get_interface"); \
    SeSubsystemStorageEntry storageEntry =                              \
    {                                                                   \
        .libraryHandle      = libHandle,                                \
        .getInterface       = getInterface,                             \
        .update             = update,                                   \
        .name               = subsystemName,                            \
    };                                                                  \
    (enginePtr)->subsystems.subsystemsStorage[(enginePtr)->subsystems.subsystemsStorageSize++] = storageEntry;  \
}

#define se_bytes(val) val
#define se_kilobytes(val) val * 1024ull
#define se_megabytes(val) val * 1024ull * 1024ull
#define se_gigabytes(val) val * 1024ull * 1024ull * 1024ull

#ifdef _WIN32
#   define SE_DLL_EXPORT __declspec(dllexport)
#else
#   error Unsupported platform
#endif

void se_qsort(void* memory, size_t left, size_t right, bool (*is_greater)(void* mem, size_t one, size_t other), void (*swap)(void* mem, size_t one, size_t other))
{
    if (left >= right) return;
    swap(memory, left, (left + right) / 2);
    size_t last = left;
    for (size_t it = left + 1; it <= right; it++)
        if (is_greater(memory, left, it))
            swap(memory, ++last, it);
    swap(memory, left, last);
    if (last != 0) se_qsort(memory, left, last - 1, is_greater, swap);
    se_qsort(memory, last + 1, right, is_greater, swap);
}

#define se_qsort_swap_def(type)                                 \
void se_qsort_swap_##type(void* mem, size_t one, size_t other)  \
{                                                               \
    type* arr = (type*)mem;                                     \
    type val = arr[one];                                        \
    arr[one] = arr[other];                                      \
    arr[other] = val;                                           \
}

#define se_qsort_is_greater_def(type)                               \
bool se_qsort_is_greater_##type(void* mem, size_t one, size_t other)\
{                                                                   \
    type* arr = (type*)mem;                                         \
    return arr[one] > arr[other];                                   \
}

#define se_qsort_swap(type) se_qsort_swap_##type
#define se_qsort_is_greater(type) se_qsort_is_greater_##type

se_qsort_swap_def(int)
se_qsort_swap_def(uint8_t)
se_qsort_swap_def(uint16_t)
se_qsort_swap_def(uint32_t)
se_qsort_swap_def(uint64_t)
se_qsort_swap_def(int8_t)
se_qsort_swap_def(int16_t)
se_qsort_swap_def(int32_t)
se_qsort_swap_def(int64_t)
se_qsort_swap_def(size_t)
se_qsort_swap_def(float)
se_qsort_swap_def(double)

se_qsort_is_greater_def(int)
se_qsort_is_greater_def(uint8_t)
se_qsort_is_greater_def(uint16_t)
se_qsort_is_greater_def(uint32_t)
se_qsort_is_greater_def(uint64_t)
se_qsort_is_greater_def(int8_t)
se_qsort_is_greater_def(int16_t)
se_qsort_is_greater_def(int32_t)
se_qsort_is_greater_def(int64_t)
se_qsort_is_greater_def(size_t)
se_qsort_is_greater_def(float)
se_qsort_is_greater_def(double)

#endif
