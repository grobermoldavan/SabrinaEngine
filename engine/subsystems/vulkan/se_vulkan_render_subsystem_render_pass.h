#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PASS_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject*  se_vk_render_pass_create(struct SeRenderPassCreateInfo* createInfo);
void                    se_vk_render_pass_destroy(struct SeRenderObject* pass);

size_t                  se_vk_render_pass_get_num_subpasses(struct SeRenderObject* pass);
void                    se_vk_render_pass_validate_fragment_program_setup(struct SeRenderObject* pass, struct SeRenderObject* fragmentShader, size_t subpassIndex);
uint32_t                se_vk_render_pass_get_num_color_attachments(struct SeRenderObject* pass, size_t subpassIndex);
VkRenderPass            se_vk_render_pass_get_handle(struct SeRenderObject* pass);
void                    se_vk_render_pass_validate_framebuffer_textures(struct SeRenderObject* pass, struct SeRenderObject** textures, size_t numTextures);
VkRenderPassBeginInfo   se_vk_render_pass_get_begin_info(struct SeRenderObject* pass, struct SeRenderObject* framebuffer, VkRect2D renderArea);
VkImageLayout           se_vk_render_pass_get_initial_attachment_layout(struct SeRenderObject* pass, size_t attachmentIndex);

#endif
