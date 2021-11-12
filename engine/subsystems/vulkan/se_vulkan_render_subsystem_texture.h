#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_TEXTURE_H_

struct SeVkTexture;

struct SeRenderObject* se_vk_texture_create(struct SeTextureCreateInfo* createInfo);
struct SeRenderObject* se_vk_texture_create_from_external_resources(struct SeRenderObject* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format);
void se_vk_texture_destroy(struct SeRenderObject* texture);

#endif
