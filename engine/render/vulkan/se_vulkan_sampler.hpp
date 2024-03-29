#ifndef _SE_VULKAN_SAMPLER_H_
#define _SE_VULKAN_SAMPLER_H_

#include "se_vulkan_base.hpp"

#define se_vk_sampler_filter(seFilter) (seFilter == SE_SAMPLER_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR)
#define se_vk_sampler_mipmap_mode(seMode) (seMode == SE_SAMPLER_MIPMAP_MODE_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR)

struct SeVkSampler
{
    SeVkObject  object;
    SeVkDevice* device;
    VkSampler   handle;
};

struct SeVkSamplerInfo
{
    SeVkDevice*             device;
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
};

void se_vk_sampler_construct(SeVkSampler* sampler, SeVkSamplerInfo* info);
void se_vk_sampler_destroy(SeVkSampler* sampler);

template<>
void se_vk_destroy<SeVkSampler>(SeVkSampler* res)
{
    se_vk_sampler_destroy(res);
}

template<>
void se_hash_value_builder_absorb<SeVkSampler>(SeHashValueBuilder& builder, const SeVkSampler& value)
{
    se_hash_value_builder_absorb_raw(builder, { (void*)&value, sizeof(value) });
}

template<>
SeHashValue se_hash_value_generate<SeVkSampler>(const SeVkSampler& value)
{
    return se_hash_value_generate_raw({ (void*)&value, sizeof(value) });
}

#endif