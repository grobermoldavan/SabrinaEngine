#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_

#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_memory.h"

typedef enum SeVkTextureFlags
{
    SE_VK_TEXTURE_FROM_SWAP_CHAIN               = 0x00000001,
    SE_VK_TEXTURE_HAS_SAMPLER                   = 0x00000002,
    SE_VK_TEXTURE_DEPENDS_ON_SWAP_CHAIN_EXTENT  = 0x00000004,
    SE_VK_TEXTURE_DEPENDS_ON_SWAP_CHAIN_FROMAT  = 0x00000008,
} SeVkTextureFlags;

typedef struct SeVkTexture
{
    //
    // Constant info (filled once at creation)
    //
    SeVkRenderObject        object;
    SeTextureCreateInfo     createInfo;
    //
    // Non-constant info (filled every time at re-creation)
    //
    VkExtent3D              extent;
    VkFormat                format;
    VkImageLayout           currentLayout;
    VkImage                 image;
    SeVkMemory              memory;
    VkImageView             view;
    VkImageSubresourceRange fullSubresourceRange;
    uint64_t                flags;
} SeVkTexture;

#define __SE_VK_TEXTURE_RECREATE_ZEROING_SIZE (sizeof(SeVkTexture) - offsetof(SeVkTexture, extent))
#define __SE_VK_TEXTURE_RECREATE_ZEROING_OFFSET(texturePtr) &(texturePtr)->extent

SeRenderObject* se_vk_texture_create(SeTextureCreateInfo* createInfo);
SeRenderObject* se_vk_texture_create_from_swap_chain(SeRenderObject* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format);
void            se_vk_texture_recreate_inplace(SeRenderObject* texture);
void            se_vk_texture_recreate_inplace_from_swap_chain(SeRenderObject* texture, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format);
void            se_vk_texture_submit_for_deffered_destruction(SeRenderObject* texture);
void            se_vk_texture_destroy(SeRenderObject* texture);
void            se_vk_texture_destroy_inplace(SeRenderObject* texture);
uint32_t        se_vk_texture_get_width(SeRenderObject* texture);
uint32_t        se_vk_texture_get_height(SeRenderObject* texture);

VkFormat        se_vk_texture_get_format(SeRenderObject* texture);
VkImageView     se_vk_texture_get_view(SeRenderObject* texture);
VkExtent3D      se_vk_texture_get_extent(SeRenderObject* texture);
void            se_vk_texture_transition_image_layout(SeRenderObject* texture, VkCommandBuffer commandBuffer, VkImageLayout targetLayout);
VkImageLayout   se_vk_texture_get_current_layout(SeRenderObject* texture);
bool            se_vk_texture_is_dependent_on_swap_chain(SeRenderObject* texture);

#endif
