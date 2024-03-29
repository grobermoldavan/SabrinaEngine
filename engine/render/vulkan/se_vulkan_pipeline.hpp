#ifndef _SE_VULKAN_RENDER_PIPELINE_H_
#define _SE_VULKAN_RENDER_PIPELINE_H_

#include "se_vulkan_base.hpp"
#include "se_vulkan_program.hpp"
#include "se_vulkan_render_pass.hpp"

struct SeVkDescriptorSetBindingInfo
{
    VkDescriptorType descriptorType;
};

struct SeVkDescriptorSetLayout
{
    SeVkDescriptorSetBindingInfo    bindingInfos[SE_VK_GENERAL_BITMASK_WIDTH];
    VkDescriptorPoolSize            poolSizes[SE_VK_GENERAL_BITMASK_WIDTH];
    size_t                          numPoolSizes;
    size_t                          numBindings;
    VkDescriptorPoolCreateInfo      poolCreateInfo;
    VkDescriptorSetLayout           handle;
};

struct SeVkPipeline
{
    SeVkObject              object;
    SeVkDevice*             device;
    VkPipelineBindPoint     bindPoint;
    VkPipeline              handle;
    VkPipelineLayout        layout;
    SeVkDescriptorSetLayout descriptorSetLayouts[SeVkConfig::RENDER_PIPELINE_MAX_DESCRIPTOR_SETS];
    size_t                  numDescriptorSetLayouts;
    union
    {
        struct
        {
            SeVkProgram* vertexProgram;
            SeVkProgram* fragmentProgram;
            SeVkRenderPass* pass;
        } graphics;
        struct
        {
            SeVkProgram* program;
        } compute;
    } dependencies;
};

struct SeVkGraphicsPipelineInfo
{
    SeVkDevice*                 device;
    SeVkRenderPass*             pass;
    SeVkProgramWithConstants    vertexProgram;
    SeVkProgramWithConstants    fragmentProgram;
    uint32_t                    subpassIndex;
    VkBool32                    isStencilTestEnabled;
    VkStencilOpState            frontStencilOpState;
    VkStencilOpState            backStencilOpState;
    VkBool32                    isDepthTestEnabled;
    VkBool32                    isDepthWriteEnabled;
    VkPolygonMode               polygonMode;
    VkCullModeFlags             cullMode;
    VkFrontFace                 frontFace;
    VkSampleCountFlagBits       sampling;
};

struct SeVkComputePipelineInfo
{
    SeVkDevice*                 device;
    SeVkProgramWithConstants    program;
};

void se_vk_pipeline_graphics_construct(SeVkPipeline* pipeline, SeVkGraphicsPipelineInfo* info);
void se_vk_pipeline_compute_construct(SeVkPipeline* pipeline, SeVkComputePipelineInfo* info);
void se_vk_pipeline_destroy(SeVkPipeline* pipeline);

VkDescriptorPool                se_vk_pipeline_create_descriptor_pool(SeVkPipeline* pipeline, size_t set);
size_t                          se_vk_pipeline_get_biggest_set_index(const SeVkPipeline* pipeline);
size_t                          se_vk_pipeline_get_biggest_binding_index(const SeVkPipeline* pipeline, size_t set);
VkDescriptorSetLayout           se_vk_pipeline_get_descriptor_set_layout(SeVkPipeline* pipeline, size_t set);
SeVkDescriptorSetBindingInfo    se_vk_pipeline_get_binding_info(const SeVkPipeline* pipeline, size_t set, size_t binding);

template<>
void se_vk_destroy<SeVkPipeline>(SeVkPipeline* res)
{
    se_vk_pipeline_destroy(res);
}

template<>
void se_hash_value_builder_absorb<SeVkGraphicsPipelineInfo>(SeHashValueBuilder& builder, const SeVkGraphicsPipelineInfo& value)
{
    se_hash_value_builder_absorb(builder, *value.pass);
    se_hash_value_builder_absorb(builder, value.vertexProgram);
    se_hash_value_builder_absorb(builder, value.fragmentProgram);
    const size_t absorbOffset = (size_t)&(((SeVkGraphicsPipelineInfo*)0)->subpassIndex);
    const size_t absorbSize = sizeof(SeVkGraphicsPipelineInfo) - absorbOffset;
    void* data = (void*)(((uint8_t*)&value) + absorbOffset);
    se_hash_value_builder_absorb_raw(builder, { data, absorbSize });
}

template<>
void se_hash_value_builder_absorb<SeVkComputePipelineInfo>(SeHashValueBuilder& builder, const SeVkComputePipelineInfo& value)
{
    se_hash_value_builder_absorb(builder, value.program);
}

template<>
void se_hash_value_builder_absorb<SeVkPipeline>(SeHashValueBuilder& builder, const SeVkPipeline& value)
{
    se_hash_value_builder_absorb_raw(builder, { (void*)&value.object, sizeof(value.object) });
}

template<>
SeHashValue se_hash_value_generate<SeVkComputePipelineInfo>(const SeVkComputePipelineInfo& value)
{
    SeHashValueBuilder builder = se_hash_value_builder_begin();
    se_hash_value_builder_absorb(builder, value);
    return se_hash_value_builder_end(builder);
}

template<>
SeHashValue se_hash_value_generate<SeVkGraphicsPipelineInfo>(const SeVkGraphicsPipelineInfo& value)
{
    SeHashValueBuilder builder = se_hash_value_builder_begin();
    se_hash_value_builder_absorb(builder, value);
    return se_hash_value_builder_end(builder);
}

template<>
SeHashValue se_hash_value_generate<SeVkPipeline>(const SeVkPipeline& value)
{
    return se_hash_value_generate_raw({ (void*)&value.object, sizeof(value.object) });
}

#endif
