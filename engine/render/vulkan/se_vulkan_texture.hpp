#ifndef _SE_VULKAN_TEXTURE_H_
#define _SE_VULKAN_TEXTURE_H_

#include "se_vulkan_base.hpp"
#include "se_vulkan_memory.hpp"

enum SeVkTextureFlags
{
    SE_VK_TEXTURE_FROM_SWAP_CHAIN = 0x00000001,
};

struct SeVkTextureInfo
{
    SeVkDevice*             device;
    VkFormat                format;
    VkExtent3D              extent;
    VkImageUsageFlags       usage;
    VkSampleCountFlagBits   sampling;
    SeDataProvider            data;
};

struct SeVkTexture
{
    SeVkObject              object;
    SeVkDevice*             device;
    VkExtent3D              extent;
    VkFormat                format;
    VkImageLayout           currentLayout;
    VkImage                 image;
    SeVkMemory              memory;
    VkImageView             view;
    VkImageSubresourceRange fullSubresourceRange;
    uint64_t                flags;
};

void se_vk_texture_construct(SeVkTexture* texture, SeVkTextureInfo* info);
void se_vk_texture_construct_from_swap_chain(SeVkTexture* texture, SeVkDevice* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format);
void se_vk_texture_destroy(SeVkTexture* texture);

template<>
void se_vk_destroy<SeVkTexture>(SeVkTexture* res)
{
    se_vk_texture_destroy(res);
}

template<>
bool se_compare<SeVkTextureInfo, SeVkTextureInfo>(const SeVkTextureInfo& first, const SeVkTextureInfo& second)
{
    return se_compare_raw(&first, &second, offsetof(SeVkTextureInfo, data)) && se_compare(first.data, second.data);
}

template<>
void se_hash_value_builder_absorb<SeVkTextureInfo>(SeHashValueBuilder& builder, const SeVkTextureInfo& info)
{
    se_hash_value_builder_absorb_raw(builder, { (void*)&info, offsetof(SeVkTextureInfo, data) });
    se_hash_value_builder_absorb(builder, info.data);
}

template<>
void se_hash_value_builder_absorb<SeVkTexture>(SeHashValueBuilder& builder, const SeVkTexture& tex)
{
    se_hash_value_builder_absorb(builder, tex.object);
    se_hash_value_builder_absorb(builder, tex.extent);
    se_hash_value_builder_absorb(builder, tex.format);
    se_hash_value_builder_absorb(builder, tex.image);
    se_hash_value_builder_absorb(builder, tex.object);
}

template<>
SeHashValue se_hash_value_generate<SeVkTextureInfo>(const SeVkTextureInfo& info)
{
    return se_hash_value_generate_raw({ (void*)&info, sizeof(SeVkTextureInfo) });
}

template<>
SeHashValue se_hash_value_generate<SeVkTexture>(const SeVkTexture& tex)
{
    SeHashValueBuilder builder = se_hash_value_builder_begin();
    se_hash_value_builder_absorb(builder, tex);
    return se_hash_value_builder_end(builder);
}

#endif
