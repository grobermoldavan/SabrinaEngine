
#include "se_vulkan_render_subsystem_graph.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_frame_manager.h"
#include "se_vulkan_render_subsystem_program.h"
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_pipeline.h"
#include "se_vulkan_render_subsystem_sampler.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "engine/containers.h"

#define SE_VK_GRAPH_OBJECT_LIFETIME 10
#define SE_VK_GRAPH_MAX_POOLS_IN_ARRAY 64
#define SE_VK_GRAPH_MAX_SETS_IN_DESCRIPTOR_POOL 64

typedef enum SeVkGraphTextureFlags
{
    SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN = 0x00000001,
} SeVkGraphTextureFlags;

typedef enum SeVkGraphMemoryBufferFlags
{
    SE_VK_GRAPH_MEMORY_BUFFER_SCRATCH = 0x00000001,
} SeVkGraphMemoryBufferFlags;

typedef struct SeVkGraphTextureInfoIndexed
{
    SeVkTextureInfo info;
    size_t index;
} SeVkGraphTextureInfoIndexed;

typedef struct SeVkGraphPipelineWithFrame
{
    SeVkPipeline* pipeline;
    size_t frame;
} SeVkGraphPipelineWithFrame;

typedef struct SeVkGraphDescriptorPool
{
    VkDescriptorPool handle;
    bool isLastAllocationSuccessful;
} SeVkGraphDescriptorPool;

typedef struct SeVkGraphDescriptorPoolArray
{
    SeVkGraphDescriptorPool pools[SE_VK_GRAPH_MAX_POOLS_IN_ARRAY];
    size_t numPools;
    size_t lastFrame;
} SeVkGraphDescriptorPoolArray;

#define __SE_VK_GRAPH_TIMED(type) type##Timed
#define __SE_VK_GRAPH_DEFINE_TIMED_TYPE(type) typedef struct __SE_VK_GRAPH_TIMED(type) { type* handle; size_t lastFrame; } __SE_VK_GRAPH_TIMED(type)

__SE_VK_GRAPH_DEFINE_TIMED_TYPE(SeVkTexture);
__SE_VK_GRAPH_DEFINE_TIMED_TYPE(SeVkProgram);
__SE_VK_GRAPH_DEFINE_TIMED_TYPE(SeVkRenderPass);
__SE_VK_GRAPH_DEFINE_TIMED_TYPE(SeVkFramebuffer);
__SE_VK_GRAPH_DEFINE_TIMED_TYPE(SeVkPipeline);
__SE_VK_GRAPH_DEFINE_TIMED_TYPE(SeVkSampler);

static VkStencilOpState se_vk_graph_pipeline_stencil_op_state(const SeStencilOpState* state)
{
    return (VkStencilOpState)
    {
        .failOp         = se_vk_utils_to_vk_stencil_op(state->failOp),
        .passOp         = se_vk_utils_to_vk_stencil_op(state->passOp),
        .depthFailOp    = se_vk_utils_to_vk_stencil_op(state->depthFailOp),
        .compareOp      = se_vk_utils_to_vk_compare_op(state->compareOp),
        .compareMask    = state->compareMask,
        .writeMask      = state->writeMask,
        .reference      = state->reference,
    };
}

static SeHash se_vk_graph_hash_pipeline_with_frame(const SeVkGraphPipelineWithFrame* pipelineWithFrame)
{
    const SeVkPipeline* pipeline = pipelineWithFrame->pipeline;
    SeHashInput inputs[5] = { { pipeline, sizeof(SeVkPipeline) } };
    size_t numInputs = pipeline->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS ? 5 : 3;
    if (pipeline->bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
    {
        inputs[1] = se_vk_program_get_hash_input(pipeline->dependencies.graphics.vertexProgram);
        inputs[2] = se_vk_program_get_hash_input(pipeline->dependencies.graphics.fragmentProgram);
        inputs[3] = se_vk_render_pass_get_hash_input(pipeline->dependencies.graphics.pass);
        inputs[4] = (SeHashInput){ &pipelineWithFrame->frame, sizeof(size_t) };
    }
    else if (pipeline->bindPoint == VK_PIPELINE_BIND_POINT_COMPUTE)
    {
        inputs[1] = se_vk_program_get_hash_input(pipeline->dependencies.compute.program);
        inputs[2] = (SeHashInput){ &pipelineWithFrame->frame, sizeof(size_t) };
    }
    else { se_assert(!"Unsupported pipeline type"); }
    return se_hash_multiple(inputs, numInputs);
}

void se_vk_graph_construct(SeVkGraph* graph, SeVkGraphInfo* info)
{
    SeVkMemoryManager* memoryManager = &info->device->memoryManager;
    *graph = (SeVkGraph)
    {
        .device                     = info->device,
        .context                    = SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES,
        .textureInfos               = NULL,
        .passes                     = NULL,
        .graphicsPipelineInfos      = NULL,
        .computePipelineInfos       = NULL,
        .scratchBufferViews         = NULL,
        .textureUsageCountTable     = {0},
        .textureTable               = {0},
        .programTable               = {0},
        .renderPassTable            = {0},
        .framebufferTable           = {0},
        .graphicsPipelineTable      = {0},
        .computePipelineTable       = {0},
        .samplerTable               = {0},
    };
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkTextureInfo), sizeof(size_t), 32 };
        se_hash_table_construct(&graph->textureUsageCountTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkGraphTextureInfoIndexed), sizeof(__SE_VK_GRAPH_TIMED(SeVkTexture)), 32 };
        se_hash_table_construct(&graph->textureTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkProgramInfo), sizeof(__SE_VK_GRAPH_TIMED(SeVkProgram)), 32 };
        se_hash_table_construct(&graph->programTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkRenderPassInfo), sizeof(__SE_VK_GRAPH_TIMED(SeVkRenderPass)), 32 };
        se_hash_table_construct(&graph->renderPassTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkFramebufferInfo), sizeof(__SE_VK_GRAPH_TIMED(SeVkFramebuffer)), 32 };
        se_hash_table_construct(&graph->framebufferTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkGraphicsPipelineInfo), sizeof(__SE_VK_GRAPH_TIMED(SeVkPipeline)), 32 };
        se_hash_table_construct(&graph->graphicsPipelineTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkComputePipelineInfo), sizeof(__SE_VK_GRAPH_TIMED(SeVkPipeline)), 32 };
        se_hash_table_construct(&graph->computePipelineTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkSamplerInfo), sizeof(__SE_VK_GRAPH_TIMED(SeVkSampler)), 32 };
        se_hash_table_construct(&graph->samplerTable, &ci);
    }
    {
        SeHashTableCreateInfo ci = { memoryManager->cpu_persistentAllocator, sizeof(SeVkGraphPipelineWithFrame), sizeof(SeVkGraphDescriptorPoolArray), 32 };
        se_hash_table_construct(&graph->descriptorPoolsTable, &ci);
    }
}

void se_vk_graph_destroy(SeVkGraph* graph)
{
    VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&graph->device->memoryManager);

    if (graph->textureInfos)            se_sbuffer_destroy(graph->textureInfos);
    if (graph->passes)                  se_sbuffer_destroy(graph->passes);
    if (graph->graphicsPipelineInfos)   se_sbuffer_destroy(graph->graphicsPipelineInfos);
    if (graph->computePipelineInfos)    se_sbuffer_destroy(graph->computePipelineInfos);
    if (graph->scratchBufferViews)      se_sbuffer_destroy(graph->scratchBufferViews);
    se_hash_table_destroy(&graph->textureUsageCountTable);
    se_hash_table_destroy(&graph->textureTable);
    se_hash_table_destroy(&graph->programTable);
    se_hash_table_destroy(&graph->renderPassTable);
    se_hash_table_destroy(&graph->framebufferTable);
    se_hash_table_destroy(&graph->graphicsPipelineTable);
    se_hash_table_destroy(&graph->computePipelineTable);
    se_hash_table_destroy(&graph->samplerTable);

    se_hash_table_for_each(SeVkGraphPipelineWithFrame, SeVkGraphDescriptorPoolArray, &graph->descriptorPoolsTable, pipeline, pools,
        for (size_t it = 0; it < pools->numPools; it++) vkDestroyDescriptorPool(logicalHandle, pools->pools[it].handle, callbacks);
    );
    se_hash_table_destroy(&graph->descriptorPoolsTable);
}

void se_vk_graph_begin_frame(SeVkGraph* graph)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES);

    VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    SeVkMemoryManager* memoryManager = &graph->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeVkFrameManager* frameManager = &graph->device->frameManager;
    const size_t currentFrame = frameManager->frameNumber;
    const size_t frameIndex = se_vk_frame_manager_get_active_frame_index(frameManager);

    const size_t INITIAL_CAPACITY = 32;
    se_sbuffer_construct(graph->textureInfos            , INITIAL_CAPACITY, memoryManager->cpu_frameAllocator);
    se_sbuffer_construct(graph->passes                  , INITIAL_CAPACITY, memoryManager->cpu_frameAllocator);
    se_sbuffer_construct(graph->graphicsPipelineInfos   , INITIAL_CAPACITY, memoryManager->cpu_frameAllocator);
    se_sbuffer_construct(graph->computePipelineInfos    , INITIAL_CAPACITY, memoryManager->cpu_frameAllocator);
    se_sbuffer_construct(graph->scratchBufferViews      , INITIAL_CAPACITY, memoryManager->cpu_frameAllocator);

    se_hash_table_reset(&graph->textureUsageCountTable);
    const size_t OBJECT_LIFETIME = 5;
    se_hash_table_remove_if(__SE_VK_GRAPH_TIMED(SeVkTexture), &graph->textureTable, object,
        (currentFrame - object->lastFrame) > OBJECT_LIFETIME,
        {
            se_vk_texture_destroy(object->handle);
            se_object_pool_return(SeVkTexture, &memoryManager->texturePool, object->handle);
        }
    );
    se_hash_table_remove_if(__SE_VK_GRAPH_TIMED(SeVkProgram), &graph->programTable, object,
        (currentFrame - object->lastFrame) > OBJECT_LIFETIME,
        {
            se_vk_program_destroy(object->handle);
            se_object_pool_return(SeVkProgram, &memoryManager->programPool, object->handle);
        }
    );
    se_hash_table_remove_if(__SE_VK_GRAPH_TIMED(SeVkRenderPass), &graph->renderPassTable, object,
        (currentFrame - object->lastFrame) > OBJECT_LIFETIME,
        {
            se_vk_render_pass_destroy(object->handle);
            se_object_pool_return(SeVkRenderPass, &memoryManager->renderPassPool, object->handle);
        }
    );
    se_hash_table_remove_if(__SE_VK_GRAPH_TIMED(SeVkFramebuffer), &graph->framebufferTable, object,
        (currentFrame - object->lastFrame) > OBJECT_LIFETIME,
        {
            se_vk_framebuffer_destroy(object->handle);
            se_object_pool_return(SeVkFramebuffer, &memoryManager->framebufferPool, object->handle);
        }
    );
    se_hash_table_remove_if(__SE_VK_GRAPH_TIMED(SeVkPipeline), &graph->graphicsPipelineTable, object,
        (currentFrame - object->lastFrame) > OBJECT_LIFETIME,
        {
            se_vk_pipeline_destroy(object->handle);
            se_object_pool_return(SeVkPipeline, &memoryManager->pipelinePool, object->handle);
        }
    );
    se_hash_table_remove_if(__SE_VK_GRAPH_TIMED(SeVkPipeline), &graph->computePipelineTable, object,
        (currentFrame - object->lastFrame) > OBJECT_LIFETIME,
        {
            se_vk_pipeline_destroy(object->handle);
            se_object_pool_return(SeVkPipeline, &memoryManager->pipelinePool, object->handle);
        }
    );
    se_hash_table_remove_if(__SE_VK_GRAPH_TIMED(SeVkSampler), &graph->samplerTable, object,
        (currentFrame - object->lastFrame) > OBJECT_LIFETIME,
        {
            se_vk_sampler_destroy(object->handle);
            se_object_pool_return(SeVkSampler, &memoryManager->samplerPool, object->handle);
        }
    );
    se_hash_table_remove_if(SeVkGraphDescriptorPoolArray, &graph->descriptorPoolsTable, pools,
        (currentFrame - pools->lastFrame) > OBJECT_LIFETIME,
        for (size_t it = 0; it < pools->numPools; it++) vkDestroyDescriptorPool(logicalHandle, pools->pools[it].handle, callbacks);
    );

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME;
}

void se_vk_graph_end_frame(SeVkGraph* graph)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    SeVkMemoryManager* memoryManager = &graph->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeVkFrameManager* frameManager = &graph->device->frameManager;
    SeVkFrame* frame = se_vk_frame_manager_get_active_frame(frameManager);
    const size_t frameIndex = se_vk_frame_manager_get_active_frame_index(frameManager);

    //
    // Acquire next image and wait for last frame that used this image
    //

    uint32_t swapChainTextureIndex;
    se_vk_check(vkAcquireNextImageKHR(
        logicalHandle,
        se_vk_device_get_swap_chain_handle(graph->device),
        UINT64_MAX,
        frame->imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &swapChainTextureIndex
    ));
    if (frameManager->imageToFrame[swapChainTextureIndex] != SE_VK_FRAME_MANAGER_INVALID_FRAME)
    {
        SeVkFrame* prevImageFrame = &frameManager->frames[frameManager->imageToFrame[swapChainTextureIndex]];
        se_assert(prevImageFrame->lastBuffer);
        VkFence fence = prevImageFrame->lastBuffer->fence;
        vkWaitForFences(logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    frameManager->imageToFrame[swapChainTextureIndex] = frameIndex;

    //
    // Textures
    //

    const size_t numTextureInfos = se_sbuffer_size(graph->textureInfos);
    se_sbuffer(SeVkTexture*) frameTextures = NULL;
    se_sbuffer_construct(frameTextures, numTextureInfos, memoryManager->cpu_frameAllocator);
    for (size_t it = 0; it < numTextureInfos; it++)
    {
        SeVkTextureInfo* info = &graph->textureInfos[it];
        const SeHash textureInfoHash = se_hash(info, sizeof(SeVkTextureInfo));
        size_t* usageCount = se_hash_table_get(SeVkTextureInfo, size_t, &graph->textureUsageCountTable, textureInfoHash, info);
        if (!usageCount)
        {
            size_t newUsageCount = 0;
            usageCount = se_hash_table_set(SeVkTextureInfo, size_t, &graph->textureUsageCountTable, textureInfoHash, info, &newUsageCount);
        }

        const SeVkGraphTextureInfoIndexed indexedInfo = (SeVkGraphTextureInfoIndexed){ *info, *usageCount };
        const SeHash indexedInfoHash = se_hash(&indexedInfo, sizeof(SeVkGraphTextureInfoIndexed));
        __SE_VK_GRAPH_TIMED(SeVkTexture)* texture = se_hash_table_get(SeVkGraphTextureInfoIndexed, __SE_VK_GRAPH_TIMED(SeVkTexture), &graph->textureTable, indexedInfoHash, &indexedInfo);
        if (!texture)
        {
            __SE_VK_GRAPH_TIMED(SeVkTexture) newTexture = (__SE_VK_GRAPH_TIMED(SeVkTexture))
            {
                .handle     = se_object_pool_take(SeVkTexture, &memoryManager->texturePool),
                .lastFrame  = 0,
            };
            se_vk_texture_construct(newTexture.handle, info);
            texture = se_hash_table_set(SeVkGraphTextureInfoIndexed, __SE_VK_GRAPH_TIMED(SeVkTexture), &graph->textureTable, indexedInfoHash, &indexedInfo, &newTexture);
        }
        texture->lastFrame = frameManager->frameNumber;
        *usageCount += 1;
        se_sbuffer_push(frameTextures, texture->handle);
    }

    //
    // Render passes
    //

    const size_t numPasses = se_sbuffer_size(graph->passes);
    se_sbuffer(SeVkRenderPass*) frameRenderPasses = NULL;
    se_sbuffer_construct(frameRenderPasses, numPasses, memoryManager->cpu_frameAllocator);
    for (size_t it = 0; it < numPasses; it++)
    {
        const SeVkType pipelineType = se_vk_ref_type(graph->passes[it].info.pipeline);
        se_assert((pipelineType == SE_VK_TYPE_GRAPHICS_PIPELINE) || (pipelineType == SE_VK_TYPE_COMPUTE_PIPELINE));
        if (pipelineType == SE_VK_TYPE_COMPUTE_PIPELINE)
        {
            se_sbuffer_push(frameRenderPasses, NULL);
        }
        else
        {
            SeVkRenderPassInfo* info = &graph->passes[it].renderPassInfo;
            const SeHash hash = se_hash(info, sizeof(SeVkRenderPassInfo));
            __SE_VK_GRAPH_TIMED(SeVkRenderPass)* pass = se_hash_table_get(SeVkRenderPassInfo, __SE_VK_GRAPH_TIMED(SeVkRenderPass), &graph->renderPassTable, hash, info);
            if (!pass)
            {
                __SE_VK_GRAPH_TIMED(SeVkRenderPass) newPass = (__SE_VK_GRAPH_TIMED(SeVkRenderPass))
                {
                    .handle     = se_object_pool_take(SeVkRenderPass, &memoryManager->renderPassPool),
                    .lastFrame  = 0,
                };
                se_vk_render_pass_construct(newPass.handle, info);
                pass = se_hash_table_set(SeVkRenderPassInfo, __SE_VK_GRAPH_TIMED(SeVkRenderPass), &graph->renderPassTable, hash, info, &newPass);
            }
            pass->lastFrame = frameManager->frameNumber;
            se_sbuffer_push(frameRenderPasses, pass->handle);
        }
    }

    //
    // Framebuffers
    //

    se_sbuffer(SeVkFramebuffer*) frameFramebuffers = NULL;
    se_sbuffer_construct(frameFramebuffers, numPasses, memoryManager->cpu_frameAllocator);
    for (size_t it = 0; it < numPasses; it++)
    {
        if (se_vk_ref_type(graph->passes[it].info.pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE)
        {
            se_sbuffer_push(frameFramebuffers, NULL);
        }
        else
        {
            const SeVkGraphPass* pass = &graph->passes[it];
            SeVkFramebufferInfo info = (SeVkFramebufferInfo)
            {
                .device         = graph->device,
                .pass           = frameRenderPasses[it],
                .textures       = {0},
                .numTextures    = pass->renderPassInfo.numColorAttachments + (pass->renderPassInfo.hasDepthStencilAttachment ? 1 : 0),
            };
            SeHashInput hashInputs[SE_VK_FRAMEBUFFER_MAX_TEXTURES + 2];
            hashInputs[0] = (SeHashInput){ &info, sizeof(SeVkFramebufferInfo) };
            hashInputs[1] = se_vk_render_pass_get_hash_input(frameRenderPasses[it]);
            for (size_t textureIt = 0; textureIt < pass->renderPassInfo.numColorAttachments; textureIt++)
            {
                const SeRenderRef textureRef = pass->info.renderTargets[textureIt].texture;
                if (se_vk_ref_flags(textureRef) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN)
                {
                    info.textures[textureIt] = se_vk_device_get_swap_chain_texture(graph->device, swapChainTextureIndex);
                }
                else
                {
                    info.textures[textureIt] = frameTextures[se_vk_ref_index(textureRef)];
                }
                hashInputs[2 + textureIt] = se_vk_texture_get_hash_input(info.textures[textureIt]);
            }
            if (pass->renderPassInfo.hasDepthStencilAttachment)
            {
                info.textures[info.numTextures - 1] = frameTextures[se_vk_ref_index(pass->info.depthStencilTarget.texture)];
                hashInputs[2 + info.numTextures - 1] = se_vk_texture_get_hash_input(info.textures[info.numTextures - 1]);
            }
            const SeHash hash = se_hash_multiple(hashInputs, 2 + info.numTextures);
            __SE_VK_GRAPH_TIMED(SeVkFramebuffer)* framebuffer = se_hash_table_get(SeVkFramebufferInfo, __SE_VK_GRAPH_TIMED(SeVkFramebuffer), &graph->framebufferTable, hash, &info);
            if (!framebuffer)
            {
                __SE_VK_GRAPH_TIMED(SeVkFramebuffer) newFramebuffer = (__SE_VK_GRAPH_TIMED(SeVkFramebuffer))
                {
                    .handle     = se_object_pool_take(SeVkFramebuffer, &memoryManager->framebufferPool),
                    .lastFrame  = 0,
                };
                se_vk_framebuffer_construct(newFramebuffer.handle, &info);
                framebuffer = se_hash_table_set(SeVkFramebufferInfo, __SE_VK_GRAPH_TIMED(SeVkFramebuffer), &graph->framebufferTable, hash, &info, &newFramebuffer);
            }
            framebuffer->lastFrame = frameManager->frameNumber;
            se_sbuffer_push(frameFramebuffers, framebuffer->handle);
        }
    }

    //
    // Pipelines
    //

    se_sbuffer(SeVkPipeline*) framePipelines = NULL;
    se_sbuffer_construct(framePipelines, numPasses, memoryManager->cpu_frameAllocator);
    for (size_t it = 0; it < numPasses; it++)
    {
        SeVkPipeline* pipeline = NULL;
        if (se_vk_ref_type(graph->passes[it].info.pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE)
        {
            se_assert(!"todo");
        }
        else
        {
            se_assert(frameRenderPasses[it]);
            const SeGraphicsPipelineInfo* seInfo = &graph->graphicsPipelineInfos[se_vk_ref_index(graph->passes[it].info.pipeline)];
            SeVkProgramWithConstants vertexProgram =
            {
                .program                    = se_object_pool_access_by_index(SeVkProgram, &memoryManager->programPool, se_vk_ref_index(seInfo->vertexProgram.program)),
                .constants                  = {0},
                .numSpecializationConstants = seInfo->vertexProgram.numSpecializationConstants,
            };
            memcpy(vertexProgram.constants, seInfo->vertexProgram.specializationConstants, sizeof(SeSpecializationConstant) * seInfo->vertexProgram.numSpecializationConstants);
            SeVkProgramWithConstants fragmentProgram =
            {
                .program                    = se_object_pool_access_by_index(SeVkProgram, &memoryManager->programPool, se_vk_ref_index(seInfo->fragmentProgram.program)),
                .constants                  = {0},
                .numSpecializationConstants = seInfo->fragmentProgram.numSpecializationConstants,
            };
            memcpy(fragmentProgram.constants, seInfo->fragmentProgram.specializationConstants, sizeof(SeSpecializationConstant) * seInfo->fragmentProgram.numSpecializationConstants);
            SeVkGraphicsPipelineInfo vkInfo = (SeVkGraphicsPipelineInfo)
            {
                .device                 = graph->device,
                .pass                   = frameRenderPasses[it],
                .subpassIndex           = 0,
                .vertexProgram          = vertexProgram,
                .fragmentProgram        = fragmentProgram,
                .isStencilTestEnabled   = seInfo->frontStencilOpState.isEnabled || seInfo->backStencilOpState.isEnabled,
                .frontStencilOpState    = seInfo->frontStencilOpState.isEnabled ? se_vk_graph_pipeline_stencil_op_state(&seInfo->frontStencilOpState) : (VkStencilOpState){0},
                .backStencilOpState     = seInfo->backStencilOpState.isEnabled ? se_vk_graph_pipeline_stencil_op_state(&seInfo->backStencilOpState) : (VkStencilOpState){0},
                .isDepthTestEnabled     = seInfo->depthState.isTestEnabled,
                .isDepthWriteEnabled    = seInfo->depthState.isWriteEnabled,
                .polygonMode            = se_vk_utils_to_vk_polygon_mode(seInfo->polygonMode),
                .cullMode               = se_vk_utils_to_vk_cull_mode(seInfo->cullMode),
                .frontFace              = se_vk_utils_to_vk_front_face(seInfo->frontFace),
                .sampling               = se_vk_utils_to_vk_sample_count(seInfo->samplingType),
            };
            SeHashInput hashInputs[2] =
            {
                { &vkInfo, sizeof(SeVkGraphicsPipelineInfo) },
                se_vk_render_pass_get_hash_input(frameRenderPasses[it]),
            };
            const SeHash hash = se_hash_multiple(hashInputs, 2);
            __SE_VK_GRAPH_TIMED(SeVkPipeline)* pipelineTimed = se_hash_table_get(SeVkGraphicsPipelineInfo, __SE_VK_GRAPH_TIMED(SeVkPipeline), &graph->graphicsPipelineTable, hash, &vkInfo);
            if (!pipelineTimed)
            {
                __SE_VK_GRAPH_TIMED(SeVkPipeline) newPipeline = (__SE_VK_GRAPH_TIMED(SeVkPipeline))
                {
                    .handle     = se_object_pool_take(SeVkPipeline, &memoryManager->pipelinePool),
                    .lastFrame  = 0,
                };
                se_vk_pipeline_graphics_construct(newPipeline.handle, &vkInfo);
                pipelineTimed = se_hash_table_set(SeVkGraphicsPipelineInfo, __SE_VK_GRAPH_TIMED(SeVkPipeline), &graph->graphicsPipelineTable, hash, &vkInfo, &newPipeline);
            }
            pipelineTimed->lastFrame = frameManager->frameNumber;
            pipeline = pipelineTimed->handle;
        }
        se_assert(pipeline);
        se_sbuffer_push(framePipelines, pipeline);
    }

    //
    // Prepare for command buffer recording
    // 1. Wait for previous commands from this frame
    // 2. Reset command buffer pool
    // 3. Reset descriptor pools
    //

    SeVkCommandBuffer* lastCommandBufferFromThisFrame = frame->lastBuffer;
    if (lastCommandBufferFromThisFrame)
    {
        se_vk_check(vkWaitForFences(
            logicalHandle,
            1,
            &lastCommandBufferFromThisFrame->fence,
            VK_TRUE,
            UINT64_MAX
        ));
    }

    se_assert(frameIndex < se_sbuffer_size(memoryManager->commandBufferPools));
    SeObjectPool* cmdPool = &memoryManager->commandBufferPools[frameIndex];
    se_object_pool_for(SeVkCommandBuffer, cmdPool, cmd, se_vk_command_buffer_destroy(cmd));
    se_object_pool_reset(cmdPool);

    se_hash_table_for_each(SeVkGraphPipelineWithFrame, SeVkGraphDescriptorPoolArray, &graph->descriptorPoolsTable, info, pools,
        if (info->frame == frameIndex)
        {
            for (size_t it = 0; it < pools->numPools; it++)
            {
                pools->pools[it].isLastAllocationSuccessful = true;
                vkResetDescriptorPool(logicalHandle, pools->pools[it].handle, 0);
            }
        }
    );

    //
    // Record command buffers
    //

    SePassDependencies queuePresentDependencies = 0;    
    se_assert(numPasses <= SE_MAX_PASS_DEPENDENCIES);
    for (size_t it = 0; it < numPasses; it++)
    {
        queuePresentDependencies |= 1ull << it;
        SeVkGraphPass* pass = &graph->passes[it];
        SeVkPipeline* pipeline = framePipelines[it];
        SeVkGraphDescriptorPoolArray* descriptorPools = NULL;
        {
            SeVkGraphPipelineWithFrame pipelineWithFrame = (SeVkGraphPipelineWithFrame)
            {
                .pipeline = pipeline,
                .frame = frameIndex,
            };
            SeHash pipelineHash = se_vk_graph_hash_pipeline_with_frame(&pipelineWithFrame);
            descriptorPools = se_hash_table_get(SeVkGraphPipelineWithFrame, SeVkGraphDescriptorPoolArray, &graph->descriptorPoolsTable, pipelineHash, &pipelineWithFrame);
            if (!descriptorPools)
            {
                SeVkGraphDescriptorPoolArray newPools = {0};
                descriptorPools = se_hash_table_set(SeVkGraphPipelineWithFrame, SeVkGraphDescriptorPoolArray, &graph->descriptorPoolsTable, pipelineHash, &pipelineWithFrame, &newPools);
            }
        }
        descriptorPools->lastFrame = frameManager->frameNumber;
        //
        // Create command buffer
        //
        SeVkCommandBuffer* commandBuffer = se_object_pool_take(SeVkCommandBuffer, cmdPool);
        SeVkCommandBufferInfo cmdInfo = (SeVkCommandBufferInfo)
        {
            .device = graph->device,
            .usage  = se_vk_ref_type(pass->info.pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE
                        ? SE_VK_COMMAND_BUFFER_USAGE_COMPUTE
                        : (SE_VK_COMMAND_BUFFER_USAGE_GRAPHICS | SE_VK_COMMAND_BUFFER_USAGE_TRANSFER),
        };
        se_vk_command_buffer_construct(commandBuffer, &cmdInfo);
        //
        // Prepare render targets and begin pass if needed
        //
        if (se_vk_ref_type(pass->info.pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE)
        {
            SeVkFramebuffer* framebuffer = frameFramebuffers[it];
            SeVkRenderPass* pass = frameRenderPasses[it];
            VkImageMemoryBarrier imageBarriers[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
            uint32_t numImageBarriers = 0;
            VkPipelineStageFlags srcPipelineStageFlags = 0;
            VkPipelineStageFlags dstPipelineStageFlags = 0;
            for (size_t texIt = 0; texIt < framebuffer->numTextures; texIt++)
            {
                SeVkTexture* texture = framebuffer->textures[texIt];
                const VkImageLayout initialLayout = pass->attachmentLayoutInfos[texIt].initialLayout;
                if (texture->currentLayout != initialLayout)
                {
                    imageBarriers[numImageBarriers++] = (VkImageMemoryBarrier)
                    {
                        .sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                        .pNext                  = NULL,
                        .srcAccessMask          = se_vk_utils_image_layout_to_access_flags(texture->currentLayout),
                        .dstAccessMask          = se_vk_utils_image_layout_to_access_flags(initialLayout),
                        .oldLayout              = texture->currentLayout,
                        .newLayout              = initialLayout,
                        .srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
                        .dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
                        .image                  = texture->image,
                        .subresourceRange       = texture->fullSubresourceRange,
                    };
                    srcPipelineStageFlags |= se_vk_utils_image_layout_to_pipeline_stage_flags(texture->currentLayout);
                    dstPipelineStageFlags |= se_vk_utils_image_layout_to_pipeline_stage_flags(initialLayout);
                    texture->currentLayout = initialLayout;
                }
            }
            if (numImageBarriers)
            {
                vkCmdPipelineBarrier
                (
                    commandBuffer->handle,
                    srcPipelineStageFlags,
                    dstPipelineStageFlags,
                    0,
                    0,
                    NULL,
                    0,
                    NULL,
                    numImageBarriers,
                    imageBarriers
                );
            }
            VkViewport viewport = (VkViewport)
            {
                .x          = 0.0f,
                .y          = 0.0f,
                .width      = (float)framebuffer->extent.width,
                .height     = (float)framebuffer->extent.height,
                .minDepth   = 0.0f,
                .maxDepth   = 1.0f,
            };
            VkRect2D scissor = (VkRect2D)
            {
                .offset = (VkOffset2D) { 0, 0 },
                .extent = framebuffer->extent,
            };
            vkCmdSetViewport(commandBuffer->handle, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer->handle, 0, 1, &scissor);
            VkRenderPassBeginInfo beginInfo = (VkRenderPassBeginInfo)
            {
                .sType              = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext              = NULL,
                .renderPass         = pass->handle,
                .framebuffer        = framebuffer->handle,
                .renderArea         = (VkRect2D) { (VkOffset2D){ 0, 0 }, framebuffer->extent },
                .clearValueCount    = se_vk_render_pass_num_attachments(pass),
                .pClearValues       = pass->clearValues,
            };
            vkCmdBeginRenderPass(commandBuffer->handle, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }
        vkCmdBindPipeline(commandBuffer->handle, pipeline->bindPoint, pipeline->handle);
        //
        // Record commands
        //
        const size_t numCommands = se_sbuffer_size(pass->commands);
        for (size_t cmdIt = 0; cmdIt < numCommands; cmdIt++)
        {
            SeVkGraphCommand* command = &pass->commands[cmdIt];
            switch (command->type)
            {
                case SE_VK_GRAPH_COMMAND_TYPE_DRAW:
                {
                    SeCommandDrawInfo* draw = &command->info.draw;
                    vkCmdDraw(commandBuffer->handle, draw->numVertices, draw->numInstances, 0, 0);
                } break;
                case SE_VK_GRAPH_COMMAND_TYPE_DISPATCH:
                {
                    SeCommandDispatchInfo* dispatch = &command->info.dispatch;
                    vkCmdDispatch(commandBuffer->handle, dispatch->groupCountX, dispatch->groupCountY, dispatch->groupCountZ);
                } break;
                case SE_VK_GRAPH_COMMAND_TYPE_BIND:
                {
                    SeCommandBindInfo* bindCommandInfo = &command->info.bind;
                    se_assert(bindCommandInfo->numBindings);
                    se_assert(pipeline->numDescriptorSetLayouts > bindCommandInfo->set);
                    //
                    // Allocate descriptor set
                    //
                    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
                    {
                        SeVkGraphDescriptorPool* pool = NULL;
                        for (size_t poolIt = 0; poolIt < descriptorPools->numPools; poolIt++)
                        {
                            if (descriptorPools->pools[poolIt].isLastAllocationSuccessful)
                            {
                                pool = &descriptorPools->pools[poolIt];
                                break;
                            }
                        }
                        if (pool)
                        {
                            VkDescriptorSetAllocateInfo allocateInfo = (VkDescriptorSetAllocateInfo)
                            {
                                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                .pNext              = NULL,
                                .descriptorPool     = pool->handle,
                                .descriptorSetCount = 1,
                                .pSetLayouts        = &pipeline->descriptorSetLayouts[bindCommandInfo->set].handle,
                            };
                            // @NOTE : no se_vk_check here, because it is fine if this vkAllocateDescriptorSets call fails
                            vkAllocateDescriptorSets(logicalHandle, &allocateInfo, &descriptorSet);
                        }
                        if (descriptorSet == VK_NULL_HANDLE)
                        {
                            if (pool) pool->isLastAllocationSuccessful = false;
                            se_assert(descriptorPools->numPools < SE_VK_GRAPH_MAX_POOLS_IN_ARRAY);
                            SeVkGraphDescriptorPool newPool = (SeVkGraphDescriptorPool)
                            {
                                .handle = VK_NULL_HANDLE,
                                .isLastAllocationSuccessful = true,
                            };
                            se_vk_check(vkCreateDescriptorPool(logicalHandle, &pipeline->descriptorSetLayouts[bindCommandInfo->set].poolCreateInfo, callbacks, &newPool.handle));
                            descriptorPools->pools[descriptorPools->numPools++] = newPool;
                            VkDescriptorSetAllocateInfo allocateInfo = (VkDescriptorSetAllocateInfo)
                            {
                                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                .pNext              = NULL,
                                .descriptorPool     = newPool.handle,
                                .descriptorSetCount = 1,
                                .pSetLayouts        = &pipeline->descriptorSetLayouts[bindCommandInfo->set].handle,
                            };
                            se_vk_check(vkAllocateDescriptorSets(logicalHandle, &allocateInfo, &descriptorSet));
                            se_assert(descriptorSet != VK_NULL_HANDLE);
                        }
                    }
                    //
                    // Write and bind descriptor set
                    //
                    VkDescriptorImageInfo imageInfos[SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS];
                    VkDescriptorBufferInfo bufferInfos[SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS];
                    VkWriteDescriptorSet writes[SE_VK_RENDER_PIPELINE_MAX_DESCRIPTOR_SETS];
                    for (uint32_t bindingIt = 0; bindingIt < bindCommandInfo->numBindings; bindingIt++)
                    {
                        SeBinding* binding = &bindCommandInfo->bindings[bindingIt];
                        VkWriteDescriptorSet write = (VkWriteDescriptorSet)
                        {
                            .sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext              = NULL,
                            .dstSet             = descriptorSet,
                            .dstBinding         = binding->binding,
                            .dstArrayElement    = 0,
                            .descriptorCount    = 1,
                            .descriptorType     = pipeline->descriptorSetLayouts[bindCommandInfo->set].bindingInfos[binding->binding].descriptorType,
                            .pImageInfo         = NULL,
                            .pBufferInfo        = NULL,
                            .pTexelBufferView   = NULL,
                        };
                        if (se_vk_ref_type(binding->object) == SE_VK_TYPE_MEMORY_BUFFER)
                        {
                            if (se_vk_ref_flags(binding->object) & SE_VK_GRAPH_MEMORY_BUFFER_SCRATCH)
                            {
                                SeVkMemoryBufferView view = graph->scratchBufferViews[se_vk_ref_index(binding->object)];
                                VkDescriptorBufferInfo* bufferInfo = &bufferInfos[bindingIt];
                                *bufferInfo = (VkDescriptorBufferInfo)
                                {
                                    .buffer = view.buffer->handle,
                                    .offset = view.offset,
                                    .range  = view.size,
                                };
                                write.pBufferInfo = bufferInfo;
                            }
                            else { se_assert(!"todo : support persistent memory buffers"); }
                        }
                        else if (se_vk_ref_type(binding->object) == SE_VK_TYPE_TEXTURE)
                        {
                            se_assert(se_vk_ref_type(binding->sampler) == SE_VK_TYPE_SAMPLER);
                            SeVkTexture* texture = frameTextures[se_vk_ref_index(binding->object)];
                            SeVkSampler* sampler = se_object_pool_access_by_index(SeVkSampler, &memoryManager->samplerPool, se_vk_ref_index(binding->sampler));
                            VkDescriptorImageInfo* imageInfo = &imageInfos[bindingIt];
                            *imageInfo = (VkDescriptorImageInfo)
                            {
                                .sampler        = sampler->handle,
                                .imageView      = texture->view,
                                .imageLayout    = texture->currentLayout,
                            };
                            write.pImageInfo = imageInfo;
                        }
                        writes[bindingIt] = write;
                    }
                    vkUpdateDescriptorSets(logicalHandle, bindCommandInfo->numBindings, writes, 0, NULL);
                    vkCmdBindDescriptorSets(commandBuffer->handle, pipeline->bindPoint, pipeline->layout, bindCommandInfo->set, 1, &descriptorSet, 0, NULL);
                } break;
                default: { se_assert(!"Unknown SeVkGraphCommand"); }
            };
        }
        //
        // End render pass and submit command buffer
        //
        if (se_vk_ref_type(pass->info.pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE)
        {
            vkCmdEndRenderPass(commandBuffer->handle);
        }
        vkEndCommandBuffer(commandBuffer->handle);
        VkSemaphore waitSemaphores[SE_MAX_PASS_DEPENDENCIES];
        VkPipelineStageFlags waitStages[SE_MAX_PASS_DEPENDENCIES];
        uint32_t numWaitSemaphores = 0;
        if (pass->info.dependencies)
        {
            for (size_t depIt = 0; depIt < SE_MAX_PASS_DEPENDENCIES; depIt++)
            {
                queuePresentDependencies &= ~(1ull << depIt);
                if (pass->info.dependencies & (1ull << depIt))
                {
                    se_assert(depIt < it);
                    SeVkCommandBuffer* depCmd = se_object_pool_access_by_index(SeVkCommandBuffer, cmdPool, depIt);
                    waitSemaphores[numWaitSemaphores] = depCmd->semaphore;
                    waitStages[numWaitSemaphores] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // @TODO : optimize
                    numWaitSemaphores += 1;
                }
            }
        }
        VkSubmitInfo submit = (VkSubmitInfo)
        {
            .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                  = NULL,
            .waitSemaphoreCount     = numWaitSemaphores,
            .pWaitSemaphores        = waitSemaphores,
            .pWaitDstStageMask      = waitStages,
            .commandBufferCount     = 1,
            .pCommandBuffers        = &commandBuffer->handle,
            .signalSemaphoreCount   = 1,
            .pSignalSemaphores      = &commandBuffer->semaphore,
        };
        vkQueueSubmit(commandBuffer->queue, 1, &submit, commandBuffer->fence);
    }

    //
    // Present swap chain image
    //

    {
        //
        // Transition swap chain image layout
        //
        SeVkTexture* swapChainTexture = se_vk_device_get_swap_chain_texture(graph->device, swapChainTextureIndex);
        SeVkCommandBuffer* transitionCmd = se_object_pool_take(SeVkCommandBuffer, cmdPool);
        SeVkCommandBufferInfo cmdInfo = (SeVkCommandBufferInfo)
        {
            .device = graph->device,
            .usage  = SE_VK_COMMAND_BUFFER_USAGE_TRANSFER,
        };
        se_vk_command_buffer_construct(transitionCmd, &cmdInfo);
        VkImageMemoryBarrier swapChainImageBarrier = (VkImageMemoryBarrier)
        {
            .sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext                  = NULL,
            .srcAccessMask          = se_vk_utils_image_layout_to_access_flags(swapChainTexture->currentLayout),
            .dstAccessMask          = se_vk_utils_image_layout_to_access_flags(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
            .oldLayout              = swapChainTexture->currentLayout,
            .newLayout              = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
            .image                  = swapChainTexture->image,
            .subresourceRange       = swapChainTexture->fullSubresourceRange,
        };
        vkCmdPipelineBarrier
        (
            transitionCmd->handle,
            se_vk_utils_image_layout_to_pipeline_stage_flags(swapChainTexture->currentLayout),
            se_vk_utils_image_layout_to_pipeline_stage_flags(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR),
            0,
            0,
            NULL,
            0,
            NULL,
            1,
            &swapChainImageBarrier
        );
        swapChainTexture->currentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkEndCommandBuffer(transitionCmd->handle);
        VkSemaphore presentWaitSemaphores[SE_MAX_PASS_DEPENDENCIES + 1];
        VkPipelineStageFlags presentWaitStages[SE_MAX_PASS_DEPENDENCIES + 1];
        uint32_t numPresentWaitSemaphores = 0;
        for (size_t depIt = 0; depIt < SE_MAX_PASS_DEPENDENCIES; depIt++)
        {
            if (queuePresentDependencies & (1ull << depIt))
            {
                presentWaitSemaphores[numPresentWaitSemaphores] = se_object_pool_access_by_index(SeVkCommandBuffer, cmdPool, depIt)->semaphore;
                presentWaitStages[numPresentWaitSemaphores] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                numPresentWaitSemaphores += 1;
            }
        }
        se_assert(numPresentWaitSemaphores);
        presentWaitSemaphores[numPresentWaitSemaphores] = frame->imageAvailableSemaphore;
        presentWaitStages[numPresentWaitSemaphores] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        numPresentWaitSemaphores += 1;
        VkSubmitInfo submit = (VkSubmitInfo)
        {
            .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                  = NULL,
            .waitSemaphoreCount     = numPresentWaitSemaphores,
            .pWaitSemaphores        = presentWaitSemaphores,
            .pWaitDstStageMask      = presentWaitStages,
            .commandBufferCount     = 1,
            .pCommandBuffers        = &transitionCmd->handle,
            .signalSemaphoreCount   = 1,
            .pSignalSemaphores      = &transitionCmd->semaphore,
        };
        vkQueueSubmit(transitionCmd->queue, 1, &submit, transitionCmd->fence);
        //
        // Present
        //
        VkSwapchainKHR swapChains[] = { se_vk_device_get_swap_chain_handle(graph->device) };
        uint32_t imageIndices[] = { swapChainTextureIndex };
        VkPresentInfoKHR presentInfo = (VkPresentInfoKHR)
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = NULL,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &transitionCmd->semaphore,
            .swapchainCount     = se_array_size(swapChains),
            .pSwapchains        = swapChains,
            .pImageIndices      = imageIndices,
            .pResults           = NULL,
        };
        se_vk_check(vkQueuePresentKHR(se_vk_device_get_command_queue(graph->device, SE_VK_CMD_QUEUE_PRESENT), &presentInfo));
        frame->lastBuffer = transitionCmd;
    }

    se_sbuffer_destroy(framePipelines);
    se_sbuffer_destroy(frameFramebuffers);
    se_sbuffer_destroy(frameRenderPasses);
    se_sbuffer_destroy(frameTextures);

    se_sbuffer_destroy(graph->textureInfos);
    se_sbuffer_destroy(graph->passes);
    se_sbuffer_destroy(graph->graphicsPipelineInfos);
    se_sbuffer_destroy(graph->computePipelineInfos);
    se_sbuffer_destroy(graph->scratchBufferViews);

    graph->textureInfos = NULL;
    graph->passes = NULL;
    graph->graphicsPipelineInfos = NULL;
    graph->computePipelineInfos = NULL;
    graph->scratchBufferViews = NULL;

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES;
}


void se_vk_graph_begin_pass(SeVkGraph* graph, SeBeginPassInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    SeVkDevice* device = graph->device;
    SeVkMemoryManager* memoryManager = &device->memoryManager;

    SeVkGraphPass pass = (SeVkGraphPass)
    {
        .info           = *info,
        .renderPassInfo = {0},
        .commands       = NULL,
    };
    se_sbuffer_construct(pass.commands, 16, memoryManager->cpu_frameAllocator);

    se_assert(se_vk_ref_type(info->pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE || se_vk_ref_type(info->pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE);
    if (se_vk_ref_type(info->pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE)
    {
        //
        // Render target texture usages
        //
        se_assert(info->numRenderTargets || info->hasDepthStencil);
        for (size_t it = 0; it < info->numRenderTargets; it++)
        {
            const SeRenderRef rt = info->renderTargets[it].texture;
            se_assert(se_vk_ref_type(rt) == SE_VK_TYPE_TEXTURE);
            if (se_vk_ref_flags(rt) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN)
            {
                continue;
            }
            SeVkTextureInfo* textureInfo = &graph->textureInfos[se_vk_ref_index(rt)];
            se_assert(!se_vk_utils_is_depth_stencil_format(textureInfo->format));
            textureInfo->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (info->hasDepthStencil)
        {
            const SeRenderRef rt = info->depthStencilTarget.texture;
            SeVkTextureInfo* textureInfo = &graph->textureInfos[se_vk_ref_index(rt)];
            se_assert(!(se_vk_ref_flags(rt) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN));
            se_assert(se_vk_utils_is_depth_stencil_format(textureInfo->format));
            textureInfo->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        //
        // Render pass info
        //
        SeVkTextureInfo* depthStencilTextureInfo = info->hasDepthStencil ? &graph->textureInfos[se_vk_ref_index(info->depthStencilTarget.texture)] : NULL;
        VkAttachmentLoadOp depthStencilLoadOp = info->hasDepthStencil ? (VkAttachmentLoadOp)info->depthStencilTarget.loadOp : 0;
        const uint32_t numColorAttachments = se_vk_safe_cast_size_t_to_uint32_t(info->numRenderTargets);
        SeVkRenderPassInfo renderPassInfo = (SeVkRenderPassInfo)
        {
            .device                     = device,
            .subpasses                  = {0},
            .numSubpasses               = 1,
            .colorAttachments           = {0},
            .numColorAttachments        = numColorAttachments,
            .depthStencilAttachment     = depthStencilTextureInfo 
                ? (SeVkRenderPassAttachment)
                {
                    .format     = se_vk_device_get_depth_stencil_format(device),
                    .loadOp     = depthStencilLoadOp,
                    .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = depthStencilTextureInfo->sampling,  // @TODO : support multisampling (and resolve and stuff)
                    .clearValue = (VkClearValue){ .depthStencil = (VkClearDepthStencilValue){ .depth = 0, .stencil = 0 } },
                }
                : (SeVkRenderPassAttachment){0},
            .hasDepthStencilAttachment  = depthStencilTextureInfo != NULL,
        };
        renderPassInfo.subpasses[0] = (SeVkRenderPassSubpass)
        {
            .colorRefs   = 0,
            .inputRefs   = 0,
            .resolveRefs = {0},
            .depthRead   = depthStencilTextureInfo != NULL,
            .depthWrite  = depthStencilTextureInfo != NULL,
        };
        for (size_t it = 0; it < info->numRenderTargets; it++)
        {
            const SeRenderRef rt = info->renderTargets[it].texture;
            if (se_vk_ref_flags(rt) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN)
            {
                renderPassInfo.subpasses[0].colorRefs |= 1 << it;
                renderPassInfo.colorAttachments[it] = (SeVkRenderPassAttachment)
                {
                    .format     = se_vk_device_get_swap_chain_format(graph->device),
                    .loadOp     = (VkAttachmentLoadOp)info->renderTargets[it].loadOp,
                    .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = VK_SAMPLE_COUNT_1_BIT,
                    .clearValue = (VkClearValue){ .color = (VkClearColorValue){ 0 } },
                };
            }
            else
            {
                const SeVkTextureInfo* textureInfo = &graph->textureInfos[se_vk_ref_index(rt)];
                renderPassInfo.subpasses[0].colorRefs |= 1 << it;
                renderPassInfo.colorAttachments[it] = (SeVkRenderPassAttachment)
                {
                    .format     = textureInfo->format,
                    .loadOp     = (VkAttachmentLoadOp)info->renderTargets[it].loadOp,
                    .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = textureInfo->sampling,            // @TODO : support multisampling (and resolve and stuff)
                    .clearValue = (VkClearValue){ .color = (VkClearColorValue){ 0 } },
                };
            }
        }
        pass.renderPassInfo = renderPassInfo;
    }
    se_sbuffer_push(graph->passes, pass);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS;
}

void se_vk_graph_end_pass(SeVkGraph* graph)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME;
}

SeRenderRef se_vk_graph_program(SeVkGraph* graph, SeProgramInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME || graph->context == SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES);

    SeVkDevice* device = graph->device;
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    SeVkFrameManager* frameManager = &device->frameManager;

    SeVkProgramInfo vkInfo = (SeVkProgramInfo)
    {
        .device         = device,
        .bytecode       = info->bytecode,
        .bytecodeSize   = info->bytecodeSize,
    };
    SeVkProgram* program = NULL;
    if (graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME)
    {
        SeHash hash = se_hash(vkInfo.bytecode, vkInfo.bytecodeSize);
        __SE_VK_GRAPH_TIMED(SeVkProgram)* timedProgram = se_hash_table_get(SeVkProgramInfo, __SE_VK_GRAPH_TIMED(SeVkProgram), &graph->programTable, hash, &vkInfo);
        if (!timedProgram)
        {
            __SE_VK_GRAPH_TIMED(SeVkProgram) newTimedProgram = (__SE_VK_GRAPH_TIMED(SeVkProgram))
            {
                .handle     = se_object_pool_take(SeVkProgram, &memoryManager->programPool),
                .lastFrame  = 0,
            };
            se_vk_program_construct(newTimedProgram.handle, &vkInfo);
            timedProgram = se_hash_table_set(SeVkProgramInfo, __SE_VK_GRAPH_TIMED(SeVkProgram), &graph->programTable, hash, &vkInfo, &newTimedProgram);
        }
        timedProgram->lastFrame = frameManager->frameNumber;
        program = timedProgram->handle;
    }
    else
    {
        program = se_object_pool_take(SeVkProgram, &memoryManager->programPool);
        se_vk_program_construct(program, &vkInfo);
    }
    se_assert(program);

    return se_vk_ref(SE_VK_TYPE_PROGRAM, 0, se_object_pool_index_of(&memoryManager->programPool, program));
}

SeRenderRef se_vk_graph_texture(SeVkGraph* graph, SeTextureInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    // @NOTE : we can't create texture right here, because texture usage will be filled later
    SeVkDevice* device = graph->device;
    SeVkTextureInfo vkInfo = (SeVkTextureInfo)
    {
        .device     = device,
        .format     = info->format == SE_TEXTURE_FORMAT_DEPTH_STENCIL ? se_vk_device_get_depth_stencil_format(device) : se_vk_utils_to_vk_format(info->format),
        .extent     = (VkExtent3D){ se_vk_safe_cast_size_t_to_uint32_t(info->width), se_vk_safe_cast_size_t_to_uint32_t(info->height), 1 },
        .usage      = 0,
        .sampling   = VK_SAMPLE_COUNT_1_BIT, // @TODO : support multisampling
    };
    se_sbuffer_push(graph->textureInfos, vkInfo);

    return se_vk_ref(SE_VK_TYPE_TEXTURE, 0, se_sbuffer_size(graph->textureInfos) - 1);
}

SeRenderRef se_vk_graph_swap_chain_texture(SeVkGraph* graph)
{
    return se_vk_ref(SE_VK_TYPE_TEXTURE, SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN, 0);
}

SeRenderRef se_vk_graph_memory_buffer(SeVkGraph* graph, SeMemoryBufferInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    // @TODO : create persistent buffer if se_vk_graph_memory_buffer call was done outside of begin-end pass region
    SeVkFrameManager* frameManager = &graph->device->frameManager;
    SeVkMemoryBufferView view = se_vk_frame_manager_alloc_scratch_buffer(frameManager, info->size);
    se_sbuffer_push(graph->scratchBufferViews, view);
    se_assert_msg(view.mappedMemory, "todo : support copies for non host-visible memory buffers");
    if (view.mappedMemory && info->data)
    {
        memcpy(view.mappedMemory, info->data, info->size);
    }

    return se_vk_ref(SE_VK_TYPE_MEMORY_BUFFER, SE_VK_GRAPH_MEMORY_BUFFER_SCRATCH, se_sbuffer_size(graph->scratchBufferViews) - 1);
}

SeRenderRef se_vk_graph_graphics_pipeline(SeVkGraph* graph, SeGraphicsPipelineInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);
    se_sbuffer_push(graph->graphicsPipelineInfos, *info);
    return se_vk_ref(SE_VK_TYPE_GRAPHICS_PIPELINE, 0, se_sbuffer_size(graph->graphicsPipelineInfos) - 1);
}

SeRenderRef se_vk_graph_compute_pipeline(SeVkGraph* graph, SeComputePipelineInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);
    se_sbuffer_push(graph->computePipelineInfos, *info);
    return se_vk_ref(SE_VK_TYPE_COMPUTE_PIPELINE, 0, se_sbuffer_size(graph->computePipelineInfos) - 1);
}

SeRenderRef se_vk_graph_sampler(SeVkGraph* graph, SeSamplerInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    SeVkMemoryManager* memoryManager = &graph->device->memoryManager;
    SeVkFrameManager* frameManager = &graph->device->frameManager;

    SeVkSamplerInfo vkInfo = (SeVkSamplerInfo)
    {
        .device             = graph->device,
        .magFilter          = (VkFilter)info->magFilter,
        .minFilter          = (VkFilter)info->minFilter,
        .addressModeU       = (VkSamplerAddressMode)info->addressModeU,
        .addressModeV       = (VkSamplerAddressMode)info->addressModeV,
        .addressModeW       = (VkSamplerAddressMode)info->addressModeW,
        .mipmapMode         = (VkSamplerMipmapMode)info->mipmapMode,
        .mipLodBias         = info->mipLodBias,
        .minLod             = info->minLod,
        .maxLod             = info->maxLod,
        .anisotropyEnabled  = info->anisotropyEnable,
        .maxAnisotropy      = info->maxAnisotropy,
        .compareEnabled     = info->compareEnabled,
        .compareOp          = (VkCompareOp)info->compareOp,
    };
    SeHash hash = se_hash(&vkInfo, sizeof(SeVkSamplerInfo));
    __SE_VK_GRAPH_TIMED(SeVkSampler)* timedSampler = se_hash_table_get(SeVkSamplerInfo, __SE_VK_GRAPH_TIMED(SeVkSampler), &graph->samplerTable, hash, &vkInfo);
    if (!timedSampler)
    {
        __SE_VK_GRAPH_TIMED(SeVkSampler) newTimedSampler = (__SE_VK_GRAPH_TIMED(SeVkSampler))
        {
            .handle     = se_object_pool_take(SeVkSampler, &memoryManager->samplerPool),
            .lastFrame  = 0,
        };
        se_vk_sampler_construct(newTimedSampler.handle, &vkInfo);
        timedSampler = se_hash_table_set(SeVkSamplerInfo, __SE_VK_GRAPH_TIMED(SeVkSampler), &graph->samplerTable, hash, &vkInfo, &newTimedSampler);
    }
    timedSampler->lastFrame = frameManager->frameNumber;
    SeVkSampler* sampler = timedSampler->handle;

    return se_vk_ref(SE_VK_TYPE_SAMPLER, 0, se_object_pool_index_of(&memoryManager->samplerPool, sampler));
}

void se_vk_graph_command_bind(SeVkGraph* graph, SeCommandBindInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkGraphPass* pass = &graph->passes[se_sbuffer_size(graph->passes) - 1];
    se_assert(info->numBindings);
    SeVkGraphCommand command = (SeVkGraphCommand)
    {
        .type = SE_VK_GRAPH_COMMAND_TYPE_BIND,
        .info = (SeVkGraphCommandInfo) { .bind = *info },
    };
    se_sbuffer_push(pass->commands, command);

    for (size_t it = 0; it < info->numBindings; it++)
    {
        const SeRenderRef object = info->bindings[it].object;
        const SeVkType type = se_vk_ref_type(object);
        se_assert(type == SE_VK_TYPE_TEXTURE || type == SE_VK_TYPE_MEMORY_BUFFER);
        if (type == SE_VK_TYPE_TEXTURE)
        {
            SeVkTextureInfo* textureInfo = &graph->textureInfos[se_vk_ref_index(object)];
            textureInfo->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
    }
}

void se_vk_graph_command_draw(SeVkGraph* graph, SeCommandDrawInfo* info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkGraphPass* pass = &graph->passes[se_sbuffer_size(graph->passes) - 1];
    SeVkGraphCommand command = (SeVkGraphCommand)
    {
        .type = SE_VK_GRAPH_COMMAND_TYPE_DRAW,
        .info = (SeVkGraphCommandInfo) { .draw = *info },
    };
    se_sbuffer_push(pass->commands, command);
}
