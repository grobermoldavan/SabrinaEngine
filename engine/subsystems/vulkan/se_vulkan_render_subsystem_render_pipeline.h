#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_

#include "se_vulkan_render_subsystem_base.h"

#define SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET 32

typedef struct SeVkDescriptorSetBindingInfo
{
    VkDescriptorType descriptorType;
} SeVkDescriptorSetBindingInfo;

struct SeRenderObject*          se_vk_render_pipeline_graphics_create(struct SeGraphicsRenderPipelineCreateInfo* createInfo);
void                            se_vk_render_pipeline_destroy(struct SeRenderObject* pipeline);

struct SeRenderObject*          se_vk_render_pipeline_get_render_pass(struct SeRenderObject* pipeline);
VkPipelineBindPoint             se_vk_render_pipeline_get_bind_point(struct SeRenderObject* pipeline);
VkPipeline                      se_vk_render_pipeline_get_handle(struct SeRenderObject* pipeline);
VkDescriptorPool                se_vk_render_pipeline_create_descriptor_pool(struct SeRenderObject* pipeline, size_t set);
size_t                          se_vk_render_pipeline_get_biggest_set_index(struct SeRenderObject* pipeline);
size_t                          se_vk_render_pipeline_get_biggest_binding_index(struct SeRenderObject* pipeline, size_t set);
VkDescriptorSetLayout           se_vk_render_pipeline_get_descriptor_set_layout(struct SeRenderObject* pipeline, size_t set);
SeVkDescriptorSetBindingInfo    se_vk_render_pipeline_get_binding_info(struct SeRenderObject* pipeline, size_t set, size_t binding);
VkPipelineLayout                se_vk_render_pipeline_get_layout(struct SeRenderObject* pipeline);

#endif
