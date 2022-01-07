#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RESOURCE_SET_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RESOURCE_SET_H_

#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_render_pipeline.h"

typedef struct SeVkResourceSet
{
    SeVkRenderObject    object;
    SeRenderObject*     device;
    SeRenderObject*     pipeline;
    VkDescriptorSet     handle;
    uint32_t            set;
    SeRenderObject*     boundObjects[SE_VK_GENERAL_BITMASK_WIDTH];
    uint32_t            numBindings;
} SeVkResourceSet;

SeRenderObject* se_vk_resource_set_request(SeResourceSetRequestInfo* requestInfo);
void            se_vk_resource_set_destroy(SeRenderObject* set);

SeRenderObject* se_vk_resource_set_get_pipeline(SeRenderObject* set);
VkDescriptorSet se_vk_resource_set_get_descriptor_set(SeRenderObject* set);
uint32_t        se_vk_resource_set_get_set_index(SeRenderObject* set);
void            se_vk_resource_set_prepare(SeRenderObject* set);

#endif
