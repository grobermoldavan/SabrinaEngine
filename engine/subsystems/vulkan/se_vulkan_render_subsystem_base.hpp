#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_BASE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_BASE_H_

#ifdef _WIN32
#   define VK_USE_PLATFORM_WIN32_KHR
#else
#   error Unsupported platform
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "engine/libs/volk/volk.h"
#include "engine/common_includes.hpp"
#include "engine/render_abstraction_interface.hpp"
#include "engine/utils.hpp"
#include "engine/subsystems/se_debug_subsystem.hpp"

enum SeVkType
{
    SE_VK_TYPE_UNITIALIZED,
    SE_VK_TYPE_DEVICE,
    SE_VK_TYPE_PROGRAM,
    SE_VK_TYPE_TEXTURE,
    SE_VK_TYPE_PASS,
    SE_VK_TYPE_FRAMEBUFFER,
    SE_VK_TYPE_GRAPHICS_PIPELINE,
    SE_VK_TYPE_COMPUTE_PIPELINE,
    SE_VK_TYPE_RESOURCE_SET,
    SE_VK_TYPE_MEMORY_BUFFER,
    SE_VK_TYPE_SAMPLER,
    SE_VK_TYPE_COMMAND_BUFFER,
};

#define se_vk_ref(type, flags, index)   ((((uint64_t)(type)) << 48) | (((uint64_t)(flags)) << 32) | ((uint64_t)(index)))
#define se_vk_ref_type(ref)             ((SeVkType)((ref) >> 48))
#define se_vk_ref_flags(ref)            ((uint16_t)(((ref) >> 32) & 0xFF))
#define se_vk_ref_index(ref)            ((uint32_t)((ref) & 0xFFFFFFFF))

struct SeVkObject
{
    SeVkType type;
    size_t uniqueIndex;
};

using SeVkFlags = uint32_t;
using SeVkGeneralBitmask = uint32_t;
#define SE_VK_GENERAL_BITMASK_WIDTH (sizeof(SeVkGeneralBitmask) * 8)

#define se_vk_check(cmd) do { VkResult __result = cmd; se_assert(__result == VK_SUCCESS); } while(0)
#define se_vk_expect_handle(renderObjPtr, expectedHandleType, msg) se_assert(se_vk_render_object_handle_type(renderObjPtr) == expectedHandleType && msg" - incorrect render object type (expected "#expectedHandleType")")

#define se_vk_safe_cast_size_t_to_uint32_t(val) ((uint32_t)(val))

template<typename T>
void se_vk_destroy(T* resource)
{
    static_assert(!"Destroy template function is not specified");
}

#endif
