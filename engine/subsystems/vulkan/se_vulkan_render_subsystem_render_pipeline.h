#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject*  se_vk_render_pipeline_graphics_create(struct SeGraphicsRenderPipelineCreateInfo* createInfo);
void                    se_vk_render_pipeline_destroy(struct SeRenderObject* pipeline);

struct SeRenderObject*  se_vk_render_pipeline_get_render_pass(struct SeRenderObject* pipeline);
VkPipelineBindPoint     se_vk_render_pipeline_get_bind_point(struct SeRenderObject* pipeline);
VkPipeline              se_vk_render_pipeline_get_handle(struct SeRenderObject* pipeline);

#endif
