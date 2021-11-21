#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_FRAMEBUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject*  se_vk_framebuffer_create(struct SeFramebufferCreateInfo* createInfo);
void                    se_vk_framebuffer_destroy(struct SeRenderObject* framebuffer);

VkFramebuffer   se_vk_framebuffer_get_handle(struct SeRenderObject* framebuffer);
void            se_vk_framebuffer_prepare(struct SeRenderObject* framebuffer);

#endif
