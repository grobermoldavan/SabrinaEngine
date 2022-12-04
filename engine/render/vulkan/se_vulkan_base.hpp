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

using SeVkFlags = uint32_t;
using SeVkGeneralBitmask = uint32_t;
#define SE_VK_GENERAL_BITMASK_WIDTH (sizeof(SeVkGeneralBitmask) * 8)

struct SeVkObject
{
    enum struct Type : uint32_t
    {
        UNITIALIZED,
        DEVICE,
        PROGRAM,
        TEXTURE,
        PASS,
        FRAMEBUFFER,
        GRAPHICS_PIPELINE,
        COMPUTE_PIPELINE,
        RESOURCE_SET,
        MEMORY_BUFFER,
        SAMPLER,
        COMMAND_BUFFER,
    };
    struct Flags
    {
        enum : SeVkFlags
        {
            IN_GRAVEYARD = 0x00000001,
        };
    };
    Type        type;
    SeVkFlags   flags;
    uint64_t    uniqueIndex;
};

struct SeVkDevice;
struct SeVkCommandBuffer;
struct SeVkFramebuffer;
struct SeVkMemoryBuffer;
struct SeVkPipeline;
struct SeVkProgram;
struct SeVkRenderPass;
struct SeVkSampler;
struct SeVkTexture;

#define se_vk_check(cmd) do { VkResult __result = cmd; se_assert_msg(__result == VK_SUCCESS, "Vulkan check failed. Error code is : {}", int(__result)); } while(0)

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

template<typename T> struct SeVkRefToResource{ };
template<> struct SeVkRefToResource<SeProgramRef> { using Res = SeVkProgram; };
template<> struct SeVkRefToResource<SeSamplerRef> { using Res = SeVkSampler; };
template<> struct SeVkRefToResource<SeBufferRef> { using Res = SeVkMemoryBuffer; };
template<> struct SeVkRefToResource<SeTextureRef> { using Res = SeVkTexture; };

// Defined in se_vulkan.cpp
template<typename Ref> typename SeVkRefToResource<Ref>::Res* se_vk_unref(Ref ref);
template<typename Ref> typename SeVkRefToResource<Ref>::Res* se_vk_unref_graveyard(Ref ref);
template<typename Ref> ObjectPoolEntryRef<typename SeVkRefToResource<Ref>::Res> se_vk_to_pool_ref(Ref ref);

struct SeVkConfig
{
    static constexpr const size_t NUM_FRAMES_IN_FLIGHT = 2;
    static constexpr const size_t COMMAND_BUFFERS_ARRAY_INITIAL_CAPACITY = 128;
    static constexpr const size_t SCRATCH_BUFFERS_ARRAY_INITIAL_CAPACITY = 128;
    static constexpr const size_t COMMAND_BUFFER_EXECUTE_AFTER_MAX = 64;
    static constexpr const size_t COMMAND_BUFFER_WAIT_SEMAPHORES_MAX = 64;
    static constexpr const size_t MAX_UNIQUE_COMMAND_QUEUES = 4;
    static constexpr const size_t MAX_SWAP_CHAIN_IMAGES = 16;
    static constexpr const size_t FRAMEBUFFER_MAX_TEXTURES = 8;
    static constexpr const size_t GRAPH_MAX_POOLS_IN_ARRAY = 64;
    static constexpr const size_t RENDER_PIPELINE_MAX_DESCRIPTOR_SETS = 8;
};

#endif
