#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RESOURCE_SET_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RESOURCE_SET_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject* se_vk_resource_set_create(struct SeResourceSetCreateInfo* createInfo);
void se_vk_resource_set_destroy(struct SeRenderObject* set);

struct SeRenderObject* se_vk_resource_set_get_pipeline(struct SeRenderObject* set);
VkDescriptorSet se_vk_resource_set_get_descriptor_set(struct SeRenderObject* set);
uint32_t se_vk_resource_set_get_set_index(struct SeRenderObject* set);
void se_vk_resource_set_prepare(struct SeRenderObject* set);

#endif
