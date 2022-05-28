#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_RENDER_PIPELINE_H_

#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_program.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "engine/hash.hpp"

#define SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS 8

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
    struct SeVkDevice*      device;
    VkPipelineBindPoint     bindPoint;
    VkPipeline              handle;
    VkPipelineLayout        layout;
    SeVkDescriptorSetLayout descriptorSetLayouts[SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS];
    size_t                  numDescriptorSetLayouts;
    union
    {
        struct
        {
            struct SeVkProgram* vertexProgram;
            struct SeVkProgram* fragmentProgram;
            struct SeVkRenderPass* pass;
        } graphics;
        struct
        {
            struct SeVkProgram* program;
        } compute;
    } dependencies;
};

struct SeVkGraphicsPipelineInfo
{
    struct SeVkDevice*          device;
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
    struct SeVkDevice*          device;
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

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeVkGraphicsPipelineInfo>(HashValueBuilder& builder, const SeVkGraphicsPipelineInfo& value)
        {
            hash_value::builder::absorb(builder, *value.pass);
            hash_value::builder::absorb(builder, value.vertexProgram);
            hash_value::builder::absorb(builder, value.fragmentProgram);
            const size_t absorbOffset = (size_t)&(((SeVkGraphicsPipelineInfo*)0)->subpassIndex);
            const size_t absorbSize = sizeof(SeVkGraphicsPipelineInfo) - absorbOffset;
            void* data = (void*)(((uint8_t*)&value) + absorbOffset);
            hash_value::builder::absorb_raw(builder, { data, absorbSize });
        }

        template<>
        void absorb<SeVkComputePipelineInfo>(HashValueBuilder& builder, const SeVkComputePipelineInfo& value)
        {
            hash_value::builder::absorb(builder, value.program);
        }

        template<>
        void absorb<SeVkPipeline>(HashValueBuilder& builder, const SeVkPipeline& value)
        {
            hash_value::builder::absorb_raw(builder, { (void*)&value.object, sizeof(value.object) });
        }
    }

    template<>
    HashValue generate<SeVkComputePipelineInfo>(const SeVkComputePipelineInfo& value)
    {
        HashValueBuilder builder = hash_value::builder::create();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }

    template<>
    HashValue generate<SeVkGraphicsPipelineInfo>(const SeVkGraphicsPipelineInfo& value)
    {
        HashValueBuilder builder = hash_value::builder::create();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }

    template<>
    HashValue generate<SeVkPipeline>(const SeVkPipeline& value)
    {
        return hash_value::generate_raw({ (void*)&value.object, sizeof(value.object) });
    }
}

#endif
