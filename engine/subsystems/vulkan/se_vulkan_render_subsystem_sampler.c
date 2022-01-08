
#include "se_vulkan_render_subsystem_sampler.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "engine/se_math.h"

#define se_vk_sampler_filter(seFilter) (seFilter == SE_SAMPLER_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
#define se_vk_sampler_mipmap_mode(seMode) (seMode == SE_SAMPLER_MIPMAP_MODE_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR)

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

SeRenderObject* se_vk_sampler_create(SeSamplerCreateInfo* createInfo)
{
    se_vk_expect_handle(createInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create sampler");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    //
    //
    SeVkSampler* sampler = se_object_pool_take(SeVkSampler, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_SAMPLER));
    *sampler = (SeVkSampler)
    {
        .device = createInfo->device,
        .object = se_vk_render_object(SE_RENDER_HANDLE_TYPE_SAMPLER, se_vk_sampler_submit_for_deffered_destruction),
        .handle = VK_NULL_HANDLE,
    };
    const VkPhysicalDeviceFeatures* features = se_vk_device_get_physical_device_features(sampler->device);
    const VkPhysicalDeviceProperties* properties = se_vk_device_get_physical_device_properties(sampler->device);
    VkSamplerCreateInfo samplerCreateInfo = (VkSamplerCreateInfo)
    {
        .sType                      = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext                      = NULL,
        .flags                      = 0,
        .magFilter                  = se_vk_sampler_filter(createInfo->magFilter),
        .minFilter                  = se_vk_sampler_filter(createInfo->minFilter),
        .mipmapMode                 = se_vk_sampler_mipmap_mode(createInfo->mipmapMode),
        .addressModeU               = se_vk_sampler_address_mode(createInfo->addressModeU),
        .addressModeV               = se_vk_sampler_address_mode(createInfo->addressModeV),
        .addressModeW               = se_vk_sampler_address_mode(createInfo->addressModeW),
        .mipLodBias                 = createInfo->mipLodBias,
        .anisotropyEnable           = features->samplerAnisotropy && (createInfo->anisotropyEnable ? VK_TRUE : VK_FALSE),
        .maxAnisotropy              = se_min(createInfo->maxAnisotropy, properties->limits.maxSamplerAnisotropy),
        .compareEnable              = createInfo->compareEnable ? VK_TRUE : VK_FALSE,
        .compareOp                  = se_vk_utils_to_vk_compare_op(createInfo->compareOp),
        .minLod                     = createInfo->minLod,
        .maxLod                     = createInfo->maxLod,
        .borderColor                = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates    = VK_FALSE,
    };
    se_vk_check(vkCreateSampler(logicalHandle, &samplerCreateInfo, callbacks, &sampler->handle));
    //
    //
    //
    se_vk_add_external_resource_dependency(sampler->device);
    //
    //
    //
    return (SeRenderObject*)sampler;
}

void se_vk_sampler_submit_for_deffered_destruction(SeRenderObject* _sampler)
{
    se_vk_expect_handle(_sampler, SE_RENDER_HANDLE_TYPE_SAMPLER, "Can't destroy sampler");
    SeVkSampler* sampler = (SeVkSampler*)_sampler;
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(sampler->device);
    //
    //
    //
    se_vk_in_flight_manager_submit_deffered_destruction(inFlightManager, (SeVkDefferedDestruction){ _sampler, se_vk_sampler_destroy });
}

void se_vk_sampler_destroy(SeRenderObject* _sampler)
{
    se_vk_expect_handle(_sampler, SE_RENDER_HANDLE_TYPE_SAMPLER, "Can't destroy sampler");
    SeVkSampler* sampler = (SeVkSampler*)_sampler;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(sampler->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(sampler->device);
    //
    //
    //
    vkDestroySampler(logicalHandle, sampler->handle, callbacks);
    se_vk_remove_external_resource_dependency(sampler->device);
    se_object_pool_return(SeVkSampler, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_SAMPLER), sampler);
}

VkSampler se_vk_sampler_get_handle(SeRenderObject* _sampler)
{
    se_vk_expect_handle(_sampler, SE_RENDER_HANDLE_TYPE_SAMPLER, "Can't get sampler handle");
    SeVkSampler* sampler = (SeVkSampler*)_sampler;
    return sampler->handle;
}
