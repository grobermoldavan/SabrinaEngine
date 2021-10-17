#ifndef _SE_COMMON_INCLUDES_H_
#define _SE_COMMON_INCLUDES_H_

#include <inttypes.h>
#include <stdbool.h>

#define se_array_size(arr) (sizeof(arr) / sizeof(arr[0]))

#define se_concat(first, second) first##second
#define se_to_str(text) #text

#define se_add_subsystem(subsystemName, enginePtr)                      \
{                                                                       \
    struct SeHandle libHandle = (enginePtr)->platformIface.load_dynamic_library(subsystemName); \
    SeSubsystemFunc update = (SeSubsystemFunc)(enginePtr)->platformIface.get_dynamic_library_function_address(libHandle, "se_update"); \
    SeSubsystemReturnPtrFunc getInterface = (SeSubsystemReturnPtrFunc)(enginePtr)->platformIface.get_dynamic_library_function_address(libHandle, "se_get_interface"); \
    struct SeSubsystemStorageEntry storageEntry =                       \
    {                                                                   \
        .libraryHandle      = libHandle,                                \
        .getInterface       = getInterface,                             \
        .update             = update,                                   \
        .name               = SE_STACK_ALLOCATOR_SUBSYSTEM_NAME,        \
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

#endif
