#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_

struct SeRenderObject* se_vk_render_pass_create(struct SeRenderPassCreateInfo* createInfo);
void se_vk_render_pass_destroy(struct SeRenderObject* pass);

#endif
