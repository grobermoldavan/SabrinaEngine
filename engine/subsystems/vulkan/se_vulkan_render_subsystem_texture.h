#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject*  se_vk_texture_create(struct SeTextureCreateInfo* createInfo);
struct SeRenderObject*  se_vk_texture_create_from_external_resources(struct SeRenderObject* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format);
void                    se_vk_texture_submit_for_deffered_destruction(struct SeRenderObject* texture);
void                    se_vk_texture_destroy(struct SeRenderObject* texture);
uint32_t                se_vk_texture_get_width(struct SeRenderObject* texture);
uint32_t                se_vk_texture_get_height(struct SeRenderObject* texture);

VkFormat        se_vk_texture_get_format(struct SeRenderObject* texture);
VkImageView     se_vk_texture_get_view(struct SeRenderObject* texture);
VkExtent3D      se_vk_texture_get_extent(struct SeRenderObject* texture);
void            se_vk_texture_transition_image_layout(struct SeRenderObject* texture, VkCommandBuffer commandBuffer, VkImageLayout targetLayout);
VkImageLayout   se_vk_texture_get_current_layout(struct SeRenderObject* texture);
VkSampler       se_vk_texture_get_default_sampler(struct SeRenderObject* texture);

#endif
