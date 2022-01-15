#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_

#include "se_vulkan_render_subsystem_base.h"

#define SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS 8

typedef struct SeVkDescriptorSetBindingInfo
{
    VkDescriptorType descriptorType;
} SeVkDescriptorSetBindingInfo;

typedef struct SeVkDescriptorSetLayout
{
    SeVkDescriptorSetBindingInfo    bindingInfos[SE_VK_GENERAL_BITMASK_WIDTH];
    VkDescriptorPoolSize            poolSizes[SE_VK_GENERAL_BITMASK_WIDTH];
    size_t                          numPoolSizes;
    size_t                          numBindings;
    VkDescriptorPoolCreateInfo      poolCreateInfo;
    VkDescriptorSetLayout           handle;
} SeVkDescriptorSetLayout;

typedef union SeVkRenderPipelineCreateInfos
{
    SeGraphicsRenderPipelineCreateInfo graphics;
    SeComputeRenderPipelineCreateInfo compute;
} SeVkRenderPipelineCreateInfos;

typedef struct SeVkRenderPipeline
{
    //
    // Constant info (filled once at creation)
    //
    SeVkRenderObject                    object;
    SeVkRenderPipelineCreateInfos       createInfo;
    VkPipelineBindPoint                 bindPoint;
    //
    // Non-constant info (filled every time at re-creation)
    //
    VkPipeline                          handle;
    VkPipelineLayout                    layout;
    SeVkDescriptorSetLayout             descriptorSetLayouts[SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS];
    size_t                              numDescriptorSetLayouts;
} SeVkRenderPipeline;

#define __SE_VK_RENDER_PIPELINE_RECREATE_ZEROING_SIZE (sizeof(SeVkRenderPipeline) - (offsetof(SeVkRenderPipeline, handle)))
#define __SE_VK_RENDER_PIPELINE_RECREATE_ZEROING_OFFSET(pipelinePtr) &(pipelinePtr)->handle

SeRenderObject*                 se_vk_render_pipeline_graphics_create(SeGraphicsRenderPipelineCreateInfo* createInfo);
SeRenderObject*                 se_vk_render_pipeline_compute_create(SeComputeRenderPipelineCreateInfo* createInfo);
void                            se_vk_render_pipeline_submit_for_deffered_destruction(SeRenderObject* pipeline);
void                            se_vk_render_pipeline_destroy(SeRenderObject* pipeline);
void                            se_vk_render_pipeline_destroy_inplace(SeRenderObject* pipeline);
void                            se_vk_render_pipeline_recreate_inplace(SeRenderObject* pipeline);

SeRenderObject*                 se_vk_render_pipeline_get_render_pass(SeRenderObject* pipeline);
VkPipelineBindPoint             se_vk_render_pipeline_get_bind_point(SeRenderObject* pipeline);
VkPipeline                      se_vk_render_pipeline_get_handle(SeRenderObject* pipeline);
VkDescriptorPool                se_vk_render_pipeline_create_descriptor_pool(SeRenderObject* pipeline, size_t set);
size_t                          se_vk_render_pipeline_get_biggest_set_index(SeRenderObject* pipeline);
size_t                          se_vk_render_pipeline_get_biggest_binding_index(SeRenderObject* pipeline, size_t set);
VkDescriptorSetLayout           se_vk_render_pipeline_get_descriptor_set_layout(SeRenderObject* pipeline, size_t set);
SeVkDescriptorSetBindingInfo    se_vk_render_pipeline_get_binding_info(SeRenderObject* pipeline, size_t set, size_t binding);
VkPipelineLayout                se_vk_render_pipeline_get_layout(SeRenderObject* pipeline);
bool                            se_vk_render_pipeline_is_dependent_on_swap_chain(SeRenderObject* pipeline);

#endif
