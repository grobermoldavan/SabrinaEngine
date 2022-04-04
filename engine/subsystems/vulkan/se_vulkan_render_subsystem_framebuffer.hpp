#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_

#include "se_vulkan_render_subsystem_base.hpp"

#define SE_VK_FRAMEBUFFER_MAX_TEXTURES 8

struct SeVkFramebufferInfo
{
    struct SeVkDevice*      device;
    struct SeVkRenderPass*  pass;
    struct SeVkTexture*     textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                numTextures;
};

struct SeVkFramebuffer
{
    SeVkObject              object;
    struct SeVkDevice*      device;
    struct SeVkRenderPass*  pass;
    struct SeVkTexture*     textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    uint32_t                numTextures;
    VkFramebuffer           handle;
    VkExtent2D              extent;
};

void se_vk_framebuffer_construct(SeVkFramebuffer* framebuffer, SeVkFramebufferInfo* info);
void se_vk_framebuffer_destroy(SeVkFramebuffer* framebuffer);

template<>
void se_vk_destroy<SeVkFramebuffer>(SeVkFramebuffer* res)
{
    se_vk_framebuffer_destroy(res);
}

#endif
