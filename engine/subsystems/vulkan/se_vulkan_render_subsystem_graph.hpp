#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_GRAPH_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_GRAPH_H_

#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_program.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "se_vulkan_render_subsystem_framebuffer.hpp"
#include "se_vulkan_render_subsystem_pipeline.hpp"
#include "se_vulkan_render_subsystem_memory_buffer.hpp"
#include "se_vulkan_render_subsystem_sampler.hpp"
#include "se_vulkan_render_subsystem_command_buffer.hpp"
#include "engine/containers.hpp"

static constexpr size_t SE_VK_GRAPH_MAX_POOLS_IN_ARRAY = 64;

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
    SeBeginPassInfo info;
    SeVkRenderPassInfo renderPassInfo;
    DynamicArray<SeVkGraphCommand> commands;
};

template<typename T>
struct SeVkGraphTimed
{
    using Type = T;
    T* handle;
    size_t lastFrame;
};

struct SeVkGraphTextureInfoIndexed
{
    SeVkTextureInfo info;
    size_t index;
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
    SeVkGraphDescriptorPool pools[SE_VK_GRAPH_MAX_POOLS_IN_ARRAY];
    size_t numPools;
    size_t lastFrame;
};

struct SeVkGraph
{
    struct SeVkDevice*                                                  device;
    SeVkGraphContextType                                                context;

    DynamicArray<DynamicArray<SeVkCommandBuffer*>>                      frameCommandBuffers;

    DynamicArray<SeVkTextureInfo>                                       textureInfos;           // Requested textures
    DynamicArray<SeVkGraphPass>                                         passes;                 // Started passes
    DynamicArray<SeGraphicsPipelineInfo>                                graphicsPipelineInfos;  // Used pipeline infos (graphics)
    DynamicArray<SeComputePipelineInfo>                                 computePipelineInfos;   // Used pipeline infos (compute)
    DynamicArray<SeVkMemoryBufferView>                                  scratchBufferViews;     // Requested buffers

    HashTable<SeVkTextureInfo, size_t>                                  textureUsageCountTable; // Mapps SeVkTextureInfo to count value (this is required to support multiple textures of exactly the same format)
    HashTable<SeVkGraphTextureInfoIndexed, SeVkGraphTimed<SeVkTexture>> textureTable;           // Mapps { SeVkTextureInfo, usageCount } to actual SeVkTexture pointer
    HashTable<SeVkProgramInfo, SeVkGraphTimed<SeVkProgram>>             programTable;
    HashTable<SeVkRenderPassInfo, SeVkGraphTimed<SeVkRenderPass>>       renderPassTable;
    HashTable<SeVkFramebufferInfo, SeVkGraphTimed<SeVkFramebuffer>>     framebufferTable;
    HashTable<SeVkGraphicsPipelineInfo, SeVkGraphTimed<SeVkPipeline>>   graphicsPipelineTable;
    HashTable<SeVkComputePipelineInfo, SeVkGraphTimed<SeVkPipeline>>    computePipelineTable;
    HashTable<SeVkSamplerInfo, SeVkGraphTimed<SeVkSampler>>             samplerTable;
    HashTable<SeVkGraphPipelineWithFrame, SeVkGraphDescriptorPoolArray> descriptorPoolsTable;   // Mapps { SeVkPipeline, frameIndex } to SeVkGraphDescriptorPoolArray
};

struct SeVkGraphInfo
{
    struct SeVkDevice* device;
    size_t numFrames;
};

void        se_vk_graph_construct(SeVkGraph* graph, SeVkGraphInfo* info);
void        se_vk_graph_destroy(SeVkGraph* graph);

void        se_vk_graph_begin_frame(SeVkGraph* graph);
void        se_vk_graph_end_frame(SeVkGraph* graph);

void        se_vk_graph_begin_pass(SeVkGraph* graph, const SeBeginPassInfo& info);
void        se_vk_graph_end_pass(SeVkGraph* graph);

SeRenderRef se_vk_graph_program(SeVkGraph* graph, const SeProgramInfo& info);
SeRenderRef se_vk_graph_texture(SeVkGraph* graph, const SeTextureInfo& info);
SeRenderRef se_vk_graph_swap_chain_texture(SeVkGraph* graph);
SeRenderRef se_vk_graph_memory_buffer(SeVkGraph* graph, const SeMemoryBufferInfo& info);
SeRenderRef se_vk_graph_graphics_pipeline(SeVkGraph* graph, const SeGraphicsPipelineInfo& info);
SeRenderRef se_vk_graph_compute_pipeline(SeVkGraph* graph, const SeComputePipelineInfo& info);
SeRenderRef se_vk_graph_sampler(SeVkGraph* graph, const SeSamplerInfo& info);
void        se_vk_graph_command_bind(SeVkGraph* graph, const SeCommandBindInfo& info);
void        se_vk_graph_command_draw(SeVkGraph* graph, const SeCommandDrawInfo& info);

namespace hash_value
{
    template<>
    HashValue generate<SeVkGraphPipelineWithFrame>(const SeVkGraphPipelineWithFrame& pipelineWithFrame)
    {
        const SeVkPipeline* pipeline = pipelineWithFrame.pipeline;
        if (pipeline->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
        {
            return hash_value::generate
            (
                *pipeline->dependencies.graphics.vertexProgram,
                *pipeline->dependencies.graphics.fragmentProgram,
                *pipeline->dependencies.graphics.pass,
                pipelineWithFrame.frame
            );
        }
        else if (pipeline->bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
        {
            return hash_value::generate
            (
                *pipeline->dependencies.compute.program,
                pipelineWithFrame.frame
            );
        }
        else { se_assert(!"Unsupported pipeline type"); }
        return {};
    }
}

#endif
