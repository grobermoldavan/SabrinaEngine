#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_SAMPLER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_SAMPLER_H_

#include "se_vulkan_render_subsystem_base.h"

#define se_vk_sampler_filter(seFilter) (seFilter == SE_SAMPLER_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
#define se_vk_sampler_mipmap_mode(seMode) (seMode == SE_SAMPLER_MIPMAP_MODE_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR)

typedef struct SeVkSampler
{
    SeVkObject          object;
    struct SeVkDevice*  device;
    VkSampler           handle;
} SeVkSampler;

typedef struct SeVkSamplerInfo
{
    struct SeVkDevice*      device;
    VkFilter                magFilter;
    VkFilter                minFilter;
    VkSamplerAddressMode    addressModeU;
    VkSamplerAddressMode    addressModeV;
    VkSamplerAddressMode    addressModeW;
    VkSamplerMipmapMode     mipmapMode;
    float                   mipLodBias;
    float                   minLod;
    float                   maxLod;
    VkBool32                anisotropyEnabled;
    float                   maxAnisotropy;
    VkBool32                compareEnabled;
    VkCompareOp             compareOp;
} SeVkSamplerInfo;

void se_vk_sampler_construct(SeVkSampler* sampler, SeVkSamplerInfo* info);
void se_vk_sampler_destroy(SeVkSampler* sampler);

#endif