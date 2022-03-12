
#include "se_vulkan_render_subsystem_sampler.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "engine/se_math.h"

static size_t g_samplerIndex = 0;

static VkSamplerAddressMode se_vk_sampler_address_mode(SeSamplerAddressMode mode)
{
    switch (mode)
    {
        case SE_SAMPLER_ADDRESS_MODE_REPEAT:            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SE_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT:   return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:     return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SE_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER:   return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: se_assert(false);
    }
    return 0;
}

void se_vk_sampler_construct(SeVkSampler* sampler, SeVkSamplerInfo* info)
{
    SeVkDevice* device = info->device;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    *sampler = (SeVkSampler)
    {
        .device = device,
        .object = (SeVkObject){ SE_VK_TYPE_SAMPLER, g_samplerIndex++ },
        .handle = VK_NULL_HANDLE,
    };
    const VkPhysicalDeviceFeatures* features = se_vk_device_get_physical_device_features(device);
    const VkPhysicalDeviceProperties* properties = se_vk_device_get_physical_device_properties(device);
    VkSamplerCreateInfo samplerCreateInfo = (VkSamplerCreateInfo)
    {
        .sType                      = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                      = NULL,
        .flags                      = 0,
        .magFilter                  = info->magFilter,
        .minFilter                  = info->minFilter,
        .mipmapMode                 = info->mipmapMode,
        .addressModeU               = info->addressModeU,
        .addressModeV               = info->addressModeV,
        .addressModeW               = info->addressModeW,
        .mipLodBias                 = info->mipLodBias,
        .anisotropyEnable           = features->samplerAnisotropy && info->anisotropyEnabled,
        .maxAnisotropy              = se_min(info->maxAnisotropy, properties->limits.maxSamplerAnisotropy),
        .compareEnable              = info->compareEnabled,
        .compareOp                  = info->compareOp,
        .minLod                     = info->minLod,
        .maxLod                     = info->maxLod,
        .borderColor                = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates    = VK_FALSE,
    };
    se_vk_check(vkCreateSampler(logicalHandle, &samplerCreateInfo, callbacks, &sampler->handle));
}

void se_vk_sampler_destroy(SeVkSampler* sampler)
{
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&sampler->device->memoryManager);
    vkDestroySampler(se_vk_device_get_logical_handle(sampler->device), sampler->handle, callbacks);
}
