#ifndef _SE_VULKAN_GRAPH_H_
#define _SE_VULKAN_GRAPH_H_

#include "se_vulkan_base.hpp"
#include "se_vulkan_program.hpp"
#include "se_vulkan_texture.hpp"
#include "se_vulkan_render_pass.hpp"
#include "se_vulkan_framebuffer.hpp"
#include "se_vulkan_pipeline.hpp"
#include "se_vulkan_memory_buffer.hpp"
#include "se_vulkan_sampler.hpp"
#include "se_vulkan_command_buffer.hpp"

enum SeVkGraphContextType
{
    SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES,
    SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME,
    SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS,
};

enum SeVkGraphCommandType
{
    SE_VK_GRAPH_COMMAND_TYPE_DRAW,
    SE_VK_GRAPH_COMMAND_TYPE_DISPATCH,
    SE_VK_GRAPH_COMMAND_TYPE_BIND,
};

union SeVkGraphCommandInfo
{
    SeCommandDrawInfo draw;
    SeCommandDispatchInfo dispatch;
    SeCommandBindInfo bind;
};

struct SeVkGraphCommand
{
    SeVkGraphCommandType type;
    SeVkGraphCommandInfo info;
};

struct SeVkGraphPass
{
    enum { GRAPHICS, COMPUTE } type;
    union
    {
        SeGraphicsPassInfo graphicsPassInfo;
        SeComputePassInfo computePassInfo;
    };
    SeVkRenderPassInfo renderPassInfo;
    DynamicArray<SeVkGraphCommand> commands;
};

struct SeVkGraphPipelineWithFrame
{
    SeVkPipeline* pipeline;
    size_t frame;
};

struct SeVkGraphDescriptorPool
{
    VkDescriptorPool handle;
    bool isLastAllocationSuccessful;
};

struct SeVkGraphDescriptorPoolArray
{
    SeVkGraphDescriptorPool pools[SeVkConfig::GRAPH_MAX_POOLS_IN_ARRAY];
    size_t numPools;
    size_t lastFrame;
};

template<typename T>
struct SeVkGraphWithFrame
{
    T* object;
    size_t frame;
};

struct SeVkGraph
{
    SeVkDevice*                                             device;
    SeVkGraphContextType                                    context;
    
    DynamicArray<SeVkGraphPass>                             passes;

    HashTable<SeVkRenderPassInfo            , SeVkGraphWithFrame<SeVkRenderPass>>   renderPassInfoToRenderPass;
    HashTable<SeVkFramebufferInfo           , SeVkGraphWithFrame<SeVkFramebuffer>>  framebufferInfoToFramebuffer;
    HashTable<SeVkGraphicsPipelineInfo      , SeVkGraphWithFrame<SeVkPipeline>>     graphicsPipelineInfoToGraphicsPipeline;
    HashTable<SeVkComputePipelineInfo       , SeVkGraphWithFrame<SeVkPipeline>>     computePipelineInfoToComputePipeline;
    HashTable<SeVkGraphPipelineWithFrame    , SeVkGraphDescriptorPoolArray>         pipelineToDescriptorPools;
};

struct SeVkGraphInfo
{
    SeVkDevice* device;
};

void        se_vk_graph_construct(SeVkGraph* graph, const SeVkGraphInfo* info);
void        se_vk_graph_destroy(SeVkGraph* graph);

void        se_vk_graph_begin_frame(SeVkGraph* graph);
void        se_vk_graph_end_frame(SeVkGraph* graph);

SePassDependencies  se_vk_graph_begin_graphics_pass(SeVkGraph* graph, const SeGraphicsPassInfo& info);
SePassDependencies  se_vk_graph_begin_compute_pass(SeVkGraph* graph, const SeComputePassInfo& info);
void                se_vk_graph_end_pass(SeVkGraph* graph);

void                se_vk_graph_command_bind(SeVkGraph* graph, const SeCommandBindInfo& info);
void                se_vk_graph_command_draw(SeVkGraph* graph, const SeCommandDrawInfo& info);
void                se_vk_graph_command_dispatch(SeVkGraph* graph, const SeCommandDispatchInfo& info);

namespace hash_value
{
    namespace builder
    {
        template<>
        void absorb<SeVkGraphPipelineWithFrame>(HashValueBuilder& builder, const SeVkGraphPipelineWithFrame& value)
        {
            hash_value::builder::absorb(builder, *value.pipeline);
            hash_value::builder::absorb(builder, value.frame);
        }
    }

    template<>
    HashValue generate<SeVkGraphPipelineWithFrame>(const SeVkGraphPipelineWithFrame& value)
    {
        HashValueBuilder builder = hash_value::builder::begin();
        hash_value::builder::absorb(builder, value);
        return hash_value::builder::end(builder);
    }
}

namespace string
{
    template<>
    SeString cast<SeVkGraphWithFrame<SeVkFramebuffer>>(const SeVkGraphWithFrame<SeVkFramebuffer>& value, SeStringLifetime lifetime)
    {
        SeStringBuilder builder = string_builder::begin("[object: ", lifetime);
        string_builder::append(builder, string::cast(*value.object));
        string_builder::append(builder, ", frame: ");
        string_builder::append(builder, string::cast(value.frame));
        string_builder::append(builder, "]");
        return string_builder::end(builder);
    }
}

#endif
