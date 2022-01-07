#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

#define SE_VK_FRAMEBUFFER_MAX_TEXTURES 32

typedef struct SeVkFramebuffer
{
    //
    // Constant info (filled once at creation)
    //
    SeVkRenderObject        object;
    SeFramebufferCreateInfo createInfo;
    //
    // Non-constant info (filled every time at re-creation)
    //
    VkFramebuffer           handle;
    SeRenderObject*         textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    size_t                  numTextures;
    VkExtent2D              extent;
    uint64_t                flags;
} SeVkFramebuffer;

#define __SE_VK_FRAMEBUFFER_RECREATE_ZEROING_SIZE (sizeof(SeVkFramebuffer) - offsetof(SeVkFramebuffer, handle))
#define __SE_VK_FRAMEBUFFER_RECREATE_ZEROING_OFFSET(framebufferPtr) &(framebufferPtr)->handle

SeRenderObject* se_vk_framebuffer_create(SeFramebufferCreateInfo* createInfo);
void            se_vk_framebuffer_submit_for_deffered_destruction(SeRenderObject* framebuffer);
void            se_vk_framebuffer_destroy(SeRenderObject* framebuffer);
void            se_vk_framebuffer_destroy_inplace(SeRenderObject* framebuffer);
void            se_vk_framebuffer_recreate_inplace(SeRenderObject* framebuffer);

VkFramebuffer   se_vk_framebuffer_get_handle(SeRenderObject* framebuffer);
void            se_vk_framebuffer_prepare(SeRenderObject* framebuffer);
VkExtent2D      se_vk_framebuffer_get_extent(SeRenderObject* framebuffer);
bool            se_vk_framebuffer_is_dependent_on_swap_chain(SeRenderObject* framebuffer);

#endif
