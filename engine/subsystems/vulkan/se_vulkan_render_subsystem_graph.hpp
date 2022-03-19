#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_GRAPH_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_GRAPH_H_

#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "se_vulkan_render_subsystem_memory_buffer.hpp"
#include "engine/containers.hpp"

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

struct SeVkGraph
{
    struct SeVkDevice*                      device;
    SeVkGraphContextType                    context;

    DynamicArray<DynamicArray<SeVkCommandBuffer*>> frameCommandBuffers;

    DynamicArray<SeVkTextureInfo>           textureInfos;           // Requested textures
    DynamicArray<SeVkGraphPass>             passes;                 // Started passes
    DynamicArray<SeGraphicsPipelineInfo>    graphicsPipelineInfos;  // Used pipeline infos (graphics)
    DynamicArray<SeComputePipelineInfo>     computePipelineInfos;   // Used pipeline infos (compute)
    DynamicArray<SeVkMemoryBufferView>      scratchBufferViews;     // Requested buffers

    SeHashTable                             textureUsageCountTable; // Mapps SeVkTextureInfo to count value (this is required to support multiple textures of exactly the same format)
    SeHashTable                             textureTable;           // Mapps { SeVkTextureInfo, usageCount } to actual SeVkTexture pointer
    SeHashTable                             programTable;
    SeHashTable                             renderPassTable;
    SeHashTable                             framebufferTable;
    SeHashTable                             graphicsPipelineTable;
    SeHashTable                             computePipelineTable;
    SeHashTable                             samplerTable;
    SeHashTable                             descriptorPoolsTable;   // Mapps { SeVkPipeline, frameIndex } to SeVkGraphDescriptorPoolArray
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

void        se_vk_graph_begin_pass(SeVkGraph* graph, SeBeginPassInfo* info);
void        se_vk_graph_end_pass(SeVkGraph* graph);

SeRenderRef se_vk_graph_program(SeVkGraph* graph, SeProgramInfo* info);
SeRenderRef se_vk_graph_texture(SeVkGraph* graph, SeTextureInfo* info);
SeRenderRef se_vk_graph_swap_chain_texture(SeVkGraph* graph);
SeRenderRef se_vk_graph_memory_buffer(SeVkGraph* graph, SeMemoryBufferInfo* info);
SeRenderRef se_vk_graph_graphics_pipeline(SeVkGraph* graph, SeGraphicsPipelineInfo* info);
SeRenderRef se_vk_graph_compute_pipeline(SeVkGraph* graph, SeComputePipelineInfo* info);
SeRenderRef se_vk_graph_sampler(SeVkGraph* graph, SeSamplerInfo* info);
void        se_vk_graph_command_bind(SeVkGraph* graph, SeCommandBindInfo* info);
void        se_vk_graph_command_draw(SeVkGraph* graph, SeCommandDrawInfo* info);

#endif
