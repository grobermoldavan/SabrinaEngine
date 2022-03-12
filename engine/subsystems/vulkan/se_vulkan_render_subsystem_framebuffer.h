#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

#define SE_VK_FRAMEBUFFER_MAX_TEXTURES 8

typedef struct SeVkFramebufferInfo
{
    struct SeVkDevice*      device;
    struct SeVkRenderPass*  pass;
    struct SeVkTexture*     textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                numTextures;
} SeVkFramebufferInfo;

typedef struct SeVkFramebuffer
{
    SeVkObject              object;
    struct SeVkDevice*      device;
    struct SeVkRenderPass*  pass;
    struct SeVkTexture*     textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                numTextures;
    VkFramebuffer           handle;
    VkExtent2D              extent;
} SeVkFramebuffer;

void se_vk_framebuffer_construct(SeVkFramebuffer* framebuffer, SeVkFramebufferInfo* info);
void se_vk_framebuffer_destroy(SeVkFramebuffer* framebuffer);

void se_vk_framebuffer_prepare(SeVkFramebuffer* framebuffer);

#endif
