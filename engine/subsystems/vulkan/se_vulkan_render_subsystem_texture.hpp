#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_

#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_memory.hpp"

enum SeVkTextureFlags
{
    SE_VK_TEXTURE_FROM_SWAP_CHAIN = 0x00000001,
};

struct SeVkTextureInfo
{
    struct SeVkDevice*      device;
    VkFormat                format;
    VkExtent3D              extent;
    VkImageUsageFlags       usage;
    VkSampleCountFlagBits   sampling;
};

struct SeVkTexture
{
    SeVkObject              object;
    struct SeVkDevice*      device;
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
void se_vk_texture_construct_from_swap_chain(SeVkTexture* texture, struct SeVkDevice* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format);
void se_vk_texture_destroy(SeVkTexture* texture);

#define se_vk_texture_get_hash_input(texturePtr) (SeHashInput{ texturePtr, sizeof(SeVkObject) })

#endif
