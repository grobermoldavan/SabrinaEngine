#ifndef _SE_VULKAN_BASE_H_
#define _SE_VULKAN_BASE_H_

#ifdef _WIN32
#   define VK_USE_PLATFORM_WIN32_KHR
#else
#   error Unsupported platform
#endif

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <stddef.h>

#include "engine/subsystems/se_platform.hpp"
#include "engine/subsystems/se_window.hpp"
#include "engine/subsystems/se_debug.hpp"
#include "engine/subsystems/se_application_allocators.hpp"
#include "engine/se_common_includes.hpp"
#include "engine/render/se_render.hpp"
#include "engine/se_allocator_bindings.hpp"
#include "engine/se_containers.hpp"
#include "engine/se_utils.hpp"
#include "engine/se_hash.hpp"
#include "engine/se_data_providers.hpp"
#include "engine/se_math.hpp"

#define ssr_assert se_assert
#define SSR_DIRTY_ALLOCATOR
#include "engine/libs/ssr/simple_spirv_reflection.h"
#include "engine/libs/volk/volk.h"

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

struct SeVkDevice;
struct SeVkCommandBuffer;
struct SeVkFramebuffer;
struct SeVkMemoryBuffer;
struct SeVkPipeline;
struct SeVkProgram;
struct SeVkRenderPass;
struct SeVkSampler;
struct SeVkTexture;

#define se_vk_check(cmd) do { VkResult __result = cmd; se_assert(__result == VK_SUCCESS); } while(0)
#define se_vk_expect_handle(renderObjPtr, expectedHandleType, msg) se_assert(se_vk_render_object_handle_type(renderObjPtr) == expectedHandleType && msg" - incorrect render object type (expected "#expectedHandleType")")

template<typename T>
void se_vk_destroy(T* resource)
{
    static_assert(!"Destroy template function is not specified");
}

template<typename To, typename From>
To se_vk_safe_cast(From from)
{
    static_assert(!"Safe cast operation is not specified");
    return (To)from;
}

template<> uint32_t se_vk_safe_cast(size_t from) { se_assert(from <= UINT32_MAX); return (uint32_t)from; }
template<> uint16_t se_vk_safe_cast(size_t from) { se_assert(from <= UINT16_MAX); return (uint16_t)from; }
template<> uint8_t  se_vk_safe_cast(size_t from) { se_assert(from <= UINT8_MAX);  return (uint8_t)from; }
template<> int64_t  se_vk_safe_cast(size_t from) { se_assert(from <= INT64_MAX);  return (int64_t)from; }
template<> int32_t  se_vk_safe_cast(size_t from) { se_assert(from <= INT32_MAX);  return (int32_t)from; }
template<> int16_t  se_vk_safe_cast(size_t from) { se_assert(from <= INT16_MAX);  return (int16_t)from; }
template<> int8_t   se_vk_safe_cast(size_t from) { se_assert(from <= INT8_MAX);   return (int8_t)from; }

#endif
