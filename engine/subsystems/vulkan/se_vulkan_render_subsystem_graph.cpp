
#include "se_vulkan_render_subsystem_graph.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_frame_manager.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "engine/subsystems/se_application_allocators_subsystem.hpp"
#include "engine/containers.hpp"

static constexpr size_t SE_VK_GRAPH_MAX_SETS_IN_DESCRIPTOR_POOL = 64;
static constexpr size_t SE_VK_GRAPH_OBJECT_LIFETIME = 5;
static constexpr size_t CONTAINERS_INITIAL_CAPACITY = 32;

enum SeVkGraphTextureFlags
{
    SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN = 0x00000001,
};

enum SeVkGraphMemoryBufferFlags
{
    SE_VK_GRAPH_MEMORY_BUFFER_SCRATCH = 0x00000001,
};

static VkStencilOpState se_vk_graph_pipeline_stencil_op_state(const SeStencilOpState* state)
{
    return
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

template <typename Key, typename Value>
void se_vk_graph_free_old_resources(HashTable<Key, Value*>& valueTable, HashTable<Key, size_t>& frameTable, size_t currentFrame, SeVkMemoryManager* memoryManager)
{
    SeAllocatorBindings frameAllocator = app_allocators::frame();

    DynamicArray<Key> toRemove = dynamic_array::create<Key>(frameAllocator);
    ObjectPool<Value>& pool = se_vk_memory_manager_get_pool<Value>(memoryManager);
    for (auto kv : frameTable)
    {
        se_assert_msg(hash_table::get(frameTable, iter::key(kv)), "Probably hash table key contains pointer to value that was changed after hash_table::set");
        if ((currentFrame - iter::value(kv)) > SE_VK_GRAPH_OBJECT_LIFETIME)
        {
            dynamic_array::push(toRemove, iter::key(kv));
        }
    }
    for (auto it : toRemove)
    {
        Value** value = hash_table::get(valueTable, iter::value(it));
        se_assert(value);
        se_vk_destroy(*value);
        object_pool::release<Value>(pool, *value);
        hash_table::remove(valueTable, iter::value(it));
        hash_table::remove(frameTable, iter::value(it));
    }
    dynamic_array::destroy(toRemove);
}

void se_vk_graph_construct(SeVkGraph* graph, SeVkGraphInfo* info)
{
    SeAllocatorBindings persistentAllocator = app_allocators::persistent();
    SeAllocatorBindings frameAllocator = app_allocators::frame();

    *graph =
    {
        .device                     = info->device,
        .context                    = SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES,
        .frameCommandBuffers        = dynamic_array::create<DynamicArray<SeVkCommandBuffer*>>(persistentAllocator, info->numFrames),
        .textureInfos               = { }, // @NOTE : constructed at begin frame
        .passes                     = { }, // @NOTE : constructed at begin frame
        .graphicsPipelineInfos      = { }, // @NOTE : constructed at begin frame
        .computePipelineInfos       = { }, // @NOTE : constructed at begin frame
        .scratchBufferViews         = { }, // @NOTE : constructed at begin frame
        .textureInfoToCount                     = { },
        .textureInfoIndexedToTexture            = { },
        .textureInfoIndexedToFrame              = { },
        .programInfoToProgram                   = { },
        .programInfoToFrame                     = { },
        .renderPassInfoToRenderPass             = { },
        .renderPassInfoToFrame                  = { },
        .framebufferInfoToFramebuffer           = { },
        .framebufferInfoToFrame                 = { },
        .graphicsPipelineInfoToGraphicsPipeline = { },
        .graphicsPipelineInfoToFrame            = { },
        .computePipelineInfoToComputePipeline   = { },
        .computePipelineInfoToFrame             = { },
        .samplerInfoToSampler                   = { },
        .samplerInfoToFrame                     = { },
        .pipelineToDescriptorPools              = { },
    };
    for (size_t it = 0; it < info->numFrames; it++)
        dynamic_array::push(graph->frameCommandBuffers, dynamic_array::create<SeVkCommandBuffer*>(persistentAllocator, CONTAINERS_INITIAL_CAPACITY));

    hash_table::construct(graph->textureInfoToCount, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->textureInfoIndexedToTexture, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->textureInfoIndexedToFrame, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->programInfoToProgram, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->programInfoToFrame, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->renderPassInfoToRenderPass, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->renderPassInfoToFrame, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->framebufferInfoToFramebuffer, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->framebufferInfoToFrame, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->graphicsPipelineInfoToGraphicsPipeline, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->graphicsPipelineInfoToFrame, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->computePipelineInfoToComputePipeline, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->computePipelineInfoToFrame, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->samplerInfoToSampler, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->samplerInfoToFrame, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->pipelineToDescriptorPools, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
}

void se_vk_graph_destroy(SeVkGraph* graph)
{
    VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&graph->device->memoryManager);

    for (auto it : graph->frameCommandBuffers)
        dynamic_array::destroy(iter::value(it));
    dynamic_array::destroy(graph->frameCommandBuffers);

    dynamic_array::destroy(graph->textureInfos);
    dynamic_array::destroy(graph->passes);
    dynamic_array::destroy(graph->graphicsPipelineInfos);
    dynamic_array::destroy(graph->computePipelineInfos);
    dynamic_array::destroy(graph->scratchBufferViews);

    hash_table::destroy(graph->textureInfoToCount);
    hash_table::destroy(graph->textureInfoIndexedToTexture);
    hash_table::destroy(graph->textureInfoIndexedToFrame);
    hash_table::destroy(graph->programInfoToProgram);
    hash_table::destroy(graph->programInfoToFrame);
    hash_table::destroy(graph->renderPassInfoToRenderPass);
    hash_table::destroy(graph->renderPassInfoToFrame);
    hash_table::destroy(graph->framebufferInfoToFramebuffer);
    hash_table::destroy(graph->framebufferInfoToFrame);
    hash_table::destroy(graph->graphicsPipelineInfoToGraphicsPipeline);
    hash_table::destroy(graph->graphicsPipelineInfoToFrame);
    hash_table::destroy(graph->computePipelineInfoToComputePipeline);
    hash_table::destroy(graph->computePipelineInfoToFrame);
    hash_table::destroy(graph->samplerInfoToSampler);
    hash_table::destroy(graph->samplerInfoToFrame);

    for (auto kv : graph->pipelineToDescriptorPools)
    {
        auto& pools = iter::value(kv); 
        for (size_t it = 0; it < pools.numPools; it++)
            vkDestroyDescriptorPool(logicalHandle, pools.pools[it].handle, callbacks);
    };
    hash_table::destroy(graph->pipelineToDescriptorPools);
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

    hash_table::reset(graph->textureInfoToCount);
    for (auto kv : graph->pipelineToDescriptorPools)
    {
        auto& pools = iter::value(kv);
        if ((currentFrame - pools.lastFrame) <= SE_VK_GRAPH_OBJECT_LIFETIME) continue;
        for (size_t it = 0; it < pools.numPools; it++)
            vkDestroyDescriptorPool(logicalHandle, pools.pools[it].handle, callbacks);
        iter::remove(kv);
    }
    se_vk_graph_free_old_resources(graph->graphicsPipelineInfoToGraphicsPipeline, graph->graphicsPipelineInfoToFrame, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->computePipelineInfoToComputePipeline, graph->computePipelineInfoToFrame, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->framebufferInfoToFramebuffer, graph->framebufferInfoToFrame, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->textureInfoIndexedToTexture, graph->textureInfoIndexedToFrame, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->programInfoToProgram, graph->programInfoToFrame, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->renderPassInfoToRenderPass, graph->renderPassInfoToFrame, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->samplerInfoToSampler, graph->samplerInfoToFrame, currentFrame, memoryManager);

    dynamic_array::construct(graph->textureInfos, app_allocators::frame(), CONTAINERS_INITIAL_CAPACITY);
    dynamic_array::construct(graph->passes, app_allocators::frame(), CONTAINERS_INITIAL_CAPACITY);
    dynamic_array::construct(graph->graphicsPipelineInfos, app_allocators::frame(), CONTAINERS_INITIAL_CAPACITY);
    dynamic_array::construct(graph->computePipelineInfos, app_allocators::frame(), CONTAINERS_INITIAL_CAPACITY);
    dynamic_array::construct(graph->scratchBufferViews, app_allocators::frame(), CONTAINERS_INITIAL_CAPACITY);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME;
}

void se_vk_graph_end_frame(SeVkGraph* graph)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    SeVkMemoryManager* memoryManager = &graph->device->memoryManager;
    SeAllocatorBindings frameAllocator = app_allocators::frame();

    ObjectPool<SeVkTexture>&        texturePool         = se_vk_memory_manager_get_pool<SeVkTexture>(memoryManager);
    ObjectPool<SeVkRenderPass>&     renderPassPool      = se_vk_memory_manager_get_pool<SeVkRenderPass>(memoryManager);
    ObjectPool<SeVkFramebuffer>&    framebufferPool     = se_vk_memory_manager_get_pool<SeVkFramebuffer>(memoryManager);
    ObjectPool<SeVkProgram>&        programPool         = se_vk_memory_manager_get_pool<SeVkProgram>(memoryManager);
    ObjectPool<SeVkPipeline>&       pipelinePool        = se_vk_memory_manager_get_pool<SeVkPipeline>(memoryManager);
    ObjectPool<SeVkCommandBuffer>&  commandBufferPool   = se_vk_memory_manager_get_pool<SeVkCommandBuffer>(memoryManager);
    
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeVkFrameManager* frameManager = &graph->device->frameManager;
    SeVkFrame* frame = se_vk_frame_manager_get_active_frame(frameManager);
    const size_t currentFrame = frameManager->frameNumber;
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

    const size_t numTextureInfos = dynamic_array::size(graph->textureInfos);
    DynamicArray<SeVkTexture*> frameTextures = dynamic_array::create<SeVkTexture*>(frameAllocator, numTextureInfos);
    for (size_t it = 0; it < numTextureInfos; it++)
    {
        SeVkTextureInfo& info = graph->textureInfos[it];
        size_t* usageCount = hash_table::get(graph->textureInfoToCount, info);
        if (!usageCount)
        {
            usageCount = hash_table::set(graph->textureInfoToCount, info, size_t(0));
        }

        const SeVkGraphTextureInfoIndexed indexedInfo{ info, *usageCount };
        SeVkTexture** texture = hash_table::get(graph->textureInfoIndexedToTexture, indexedInfo);
        if (!texture)
        {
            texture = hash_table::set(graph->textureInfoIndexedToTexture, indexedInfo, object_pool::take(texturePool));
            se_vk_texture_construct(*texture, &info);
        }
        hash_table::set(graph->textureInfoIndexedToFrame, indexedInfo, currentFrame);
        
        *usageCount += 1;
        dynamic_array::push(frameTextures, *texture);
    }

    //
    // Render passes
    //

    const size_t numPasses = dynamic_array::size(graph->passes);
    DynamicArray<SeVkRenderPass*> frameRenderPasses = dynamic_array::create<SeVkRenderPass*>(frameAllocator, numPasses);
    for (size_t it = 0; it < numPasses; it++)
    {
        const SeVkType pipelineType = se_vk_ref_type(graph->passes[it].info.pipeline);
        se_assert((pipelineType == SE_VK_TYPE_GRAPHICS_PIPELINE) || (pipelineType == SE_VK_TYPE_COMPUTE_PIPELINE));
        if (pipelineType == SE_VK_TYPE_COMPUTE_PIPELINE)
        {
            dynamic_array::push(frameRenderPasses, nullptr);
        }
        else
        {
            SeVkRenderPassInfo& info = graph->passes[it].renderPassInfo;
            SeVkRenderPass** pass = hash_table::get(graph->renderPassInfoToRenderPass, info);
            if (!pass)
            {
                pass = hash_table::set(graph->renderPassInfoToRenderPass, info, object_pool::take(renderPassPool));
                se_vk_render_pass_construct(*pass, &info);
            }
            hash_table::set(graph->renderPassInfoToFrame, info, currentFrame);

            dynamic_array::push(frameRenderPasses, *pass);
        }
    }

    //
    // Framebuffers
    //

    DynamicArray<SeVkFramebuffer*> frameFramebuffers = dynamic_array::create<SeVkFramebuffer*>(frameAllocator, numPasses);
    for (size_t it = 0; it < numPasses; it++)
    {
        const SeVkGraphPass* pass = &graph->passes[it];
        if (se_vk_ref_type(pass->info.pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE)
        {
            dynamic_array::push(frameFramebuffers, nullptr);
        }
        else
        {
            SeVkFramebufferInfo info
            {
                .device         = graph->device,
                .pass           = object_pool::to_ref(renderPassPool, frameRenderPasses[it]),
                .textures       = { },
                .numTextures    = pass->renderPassInfo.numColorAttachments + (pass->renderPassInfo.hasDepthStencilAttachment ? 1 : 0),
            };
            for (size_t textureIt = 0; textureIt < pass->renderPassInfo.numColorAttachments; textureIt++)
            {
                const SeRenderRef textureRef = pass->info.renderTargets[textureIt].texture;
                if (se_vk_ref_flags(textureRef) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN)
                    info.textures[textureIt] = se_vk_device_get_swap_chain_texture(graph->device, swapChainTextureIndex);
                else
                    info.textures[textureIt] = object_pool::to_ref(texturePool, frameTextures[se_vk_ref_index(textureRef)]);
            }
            if (pass->renderPassInfo.hasDepthStencilAttachment)
                info.textures[info.numTextures - 1] = object_pool::to_ref(texturePool, frameTextures[se_vk_ref_index(pass->info.depthStencilTarget.texture)]);

            SeVkFramebuffer** framebuffer = hash_table::get(graph->framebufferInfoToFramebuffer, info);
            if (!framebuffer)
            {
                framebuffer = hash_table::set(graph->framebufferInfoToFramebuffer, info, object_pool::take(framebufferPool));
                se_vk_framebuffer_construct(*framebuffer, &info);
            }
            hash_table::set(graph->framebufferInfoToFrame, info, currentFrame);
            
            dynamic_array::push(frameFramebuffers, *framebuffer);
        }
    }

    //
    // Pipelines
    //

    DynamicArray<SeVkPipeline*> framePipelines = dynamic_array::create<SeVkPipeline*>(frameAllocator, numPasses);
    for (size_t it = 0; it < numPasses; it++)
    {
        SeVkPipeline* pipeline = nullptr;
        if (se_vk_ref_type(graph->passes[it].info.pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE)
        {
            se_assert(!"todo");
        }
        else
        {
            se_assert(frameRenderPasses[it]);
            const SeGraphicsPipelineInfo* seInfo = &graph->graphicsPipelineInfos[se_vk_ref_index(graph->passes[it].info.pipeline)];
            SeVkProgramWithConstants vertexProgram
            {
                .program                    = object_pool::access(programPool, se_vk_ref_index(seInfo->vertexProgram.program)),
                .constants                  = { },
                .numSpecializationConstants = seInfo->vertexProgram.numSpecializationConstants,
            };
            memcpy(vertexProgram.constants, seInfo->vertexProgram.specializationConstants, sizeof(SeSpecializationConstant) * seInfo->vertexProgram.numSpecializationConstants);
            SeVkProgramWithConstants fragmentProgram
            {
                .program                    = object_pool::access(programPool, se_vk_ref_index(seInfo->fragmentProgram.program)),
                .constants                  = { },
                .numSpecializationConstants = seInfo->fragmentProgram.numSpecializationConstants,
            };
            memcpy(fragmentProgram.constants, seInfo->fragmentProgram.specializationConstants, sizeof(SeSpecializationConstant) * seInfo->fragmentProgram.numSpecializationConstants);
            SeVkGraphicsPipelineInfo vkInfo
            {
                .device                 = graph->device,
                .pass                   = frameRenderPasses[it],
                .vertexProgram          = vertexProgram,
                .fragmentProgram        = fragmentProgram,
                .subpassIndex           = 0,
                .isStencilTestEnabled   = seInfo->frontStencilOpState.isEnabled || seInfo->backStencilOpState.isEnabled,
                .frontStencilOpState    = seInfo->frontStencilOpState.isEnabled ? se_vk_graph_pipeline_stencil_op_state(&seInfo->frontStencilOpState) : VkStencilOpState{0},
                .backStencilOpState     = seInfo->backStencilOpState.isEnabled ? se_vk_graph_pipeline_stencil_op_state(&seInfo->backStencilOpState) : VkStencilOpState{0},
                .isDepthTestEnabled     = seInfo->depthState.isTestEnabled,
                .isDepthWriteEnabled    = seInfo->depthState.isWriteEnabled,
                .polygonMode            = se_vk_utils_to_vk_polygon_mode(seInfo->polygonMode),
                .cullMode               = se_vk_utils_to_vk_cull_mode(seInfo->cullMode),
                .frontFace              = se_vk_utils_to_vk_front_face(seInfo->frontFace),
                .sampling               = se_vk_utils_to_vk_sample_count(seInfo->samplingType),
            };
            SeVkPipeline** pipelineTimed = hash_table::get(graph->graphicsPipelineInfoToGraphicsPipeline, vkInfo);
            if (!pipelineTimed)
            {
                pipelineTimed = hash_table::set(graph->graphicsPipelineInfoToGraphicsPipeline, vkInfo, object_pool::take(pipelinePool));
                se_vk_pipeline_graphics_construct(*pipelineTimed, &vkInfo);
            }
            hash_table::set(graph->graphicsPipelineInfoToFrame, vkInfo, currentFrame);

            pipeline = *pipelineTimed;
        }
        se_assert(pipeline);
        dynamic_array::push(framePipelines, pipeline);
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

    DynamicArray<SeVkCommandBuffer*>& cmdArray = graph->frameCommandBuffers[frameIndex];
    for (auto it : cmdArray)
    {
        SeVkCommandBuffer* value = iter::value(it);
        se_vk_command_buffer_destroy(value);
        object_pool::release(commandBufferPool, value);
    }
    dynamic_array::reset(cmdArray);

    auto getNewCmd = [&commandBufferPool, &cmdArray]() -> SeVkCommandBuffer*
    {
        SeVkCommandBuffer* cmd = object_pool::take(commandBufferPool);
        dynamic_array::push(cmdArray, cmd);
        return cmd;
    };

    for (auto kv : graph->pipelineToDescriptorPools)
    {
        auto& info = iter::key(kv);
        auto& pools = iter::value(kv);
        if (info.frame == frameIndex)
        {
            for (size_t it = 0; it < pools.numPools; it++)
            {
                pools.pools[it].isLastAllocationSuccessful = true;
                vkResetDescriptorPool(logicalHandle, pools.pools[it].handle, 0);
            }
        }
    }

    //
    // Record command buffers
    //

    SePassDependencies queuePresentDependencies = 0;    
    se_assert(numPasses <= SE_MAX_PASS_DEPENDENCIES);
    for (size_t it = 0; it < numPasses; it++)
    {
        queuePresentDependencies |= 1ull << it;
        SeVkGraphPass* graphPass = &graph->passes[it];
        SeVkPipeline* pipeline = framePipelines[it];
        SeVkFramebuffer* framebuffer = frameFramebuffers[it];
        SeVkRenderPass* renderPass = frameRenderPasses[it];
        //
        // Get or allocate descriptor pool
        //
        SeVkGraphDescriptorPoolArray* descriptorPools = nullptr;
        {
            SeVkGraphPipelineWithFrame pipelineWithFrame
            {
                .pipeline = pipeline,
                .frame = frameIndex,
            };
            descriptorPools = hash_table::get(graph->pipelineToDescriptorPools, pipelineWithFrame);
            if (!descriptorPools)
            {
                descriptorPools = hash_table::set(graph->pipelineToDescriptorPools, pipelineWithFrame, { });
            }
        }
        descriptorPools->lastFrame = frameManager->frameNumber;
        //
        // Create command buffer
        //
        SeVkCommandBuffer* commandBuffer = getNewCmd();
        SeVkCommandBufferInfo cmdInfo
        {
            .device = graph->device,
            .usage  = se_vk_ref_type(graphPass->info.pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE
                        ? SE_VK_COMMAND_BUFFER_USAGE_COMPUTE
                        : (SE_VK_COMMAND_BUFFER_USAGE_GRAPHICS | SE_VK_COMMAND_BUFFER_USAGE_TRANSFER),
        };
        se_vk_command_buffer_construct(commandBuffer, &cmdInfo);
        //
        // Get list of all texture layout transitions necessary for this pass
        //
        HashTable<SeVkTexture*, VkImageLayout> transitions;
        hash_table::construct(transitions, app_allocators::frame(), SE_VK_FRAMEBUFFER_MAX_TEXTURES + SE_MAX_BINDINGS);
        if (se_vk_ref_type(graphPass->info.pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE)
        {
            for (size_t texIt = 0; texIt < framebuffer->numTextures; texIt++)
            {
                SeVkTexture* texture = *framebuffer->textures[texIt];
                hash_table::set(transitions, texture, renderPass->attachmentLayoutInfos[texIt].initialLayout);
            }
        }
        for (auto cmdIt : graphPass->commands)
        {
            const SeVkGraphCommand& command = iter::value(cmdIt);
            if (command.type == SE_VK_GRAPH_COMMAND_TYPE_BIND)
            {
                const size_t numBindings = command.info.bind.numBindings;
                for (size_t bindingIt = 0; bindingIt < numBindings; bindingIt++)
                {
                    const SeBinding& binding = command.info.bind.bindings[bindingIt];
                    if (se_vk_ref_type(binding.object) == SE_VK_TYPE_TEXTURE)
                    {
                        SeVkTexture* texture = frameTextures[se_vk_ref_index(binding.object)];
                        const bool isDepth = se_vk_utils_is_depth_stencil_format(texture->format);
                        VkImageLayout* layout = hash_table::get(transitions, texture);
                        if (!layout)
                        {
                            // @TODO : use more precise layout pick logic (different layouts for depth- or stencil-only formats) :
                            // https://vulkan.lunarg.com/doc/view/1.2.198.1/windows/1.2-extensions/vkspec.html#descriptorsets-combinedimagesample
                            const VkImageLayout newLayout = se_vk_utils_is_depth_stencil_format(texture->format)
                                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            hash_table::set(transitions, texture, newLayout);
                        }
                        else
                        {
                            *layout = VK_IMAGE_LAYOUT_GENERAL; // Is this correct ?
                        }
                    }
                }
            }
        }
        //
        // Transition image layouts
        //
        VkImageMemoryBarrier imageBarriers[SE_VK_FRAMEBUFFER_MAX_TEXTURES + SE_MAX_BINDINGS];
        uint32_t numImageBarriers = 0;
        VkPipelineStageFlags srcPipelineStageFlags = 0;
        VkPipelineStageFlags dstPipelineStageFlags = 0;
        for (auto transitionIt : transitions)
        {
            SeVkTexture* texture = iter::key(transitionIt);
            VkImageLayout layout = iter::value(transitionIt);
            if (texture->currentLayout != layout)
            {
                imageBarriers[numImageBarriers++] =
                {
                    .sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext                  = nullptr,
                    .srcAccessMask          = se_vk_utils_image_layout_to_access_flags(texture->currentLayout),
                    .dstAccessMask          = se_vk_utils_image_layout_to_access_flags(layout),
                    .oldLayout              = texture->currentLayout,
                    .newLayout              = layout,
                    .srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
                    .image                  = texture->image,
                    .subresourceRange       = texture->fullSubresourceRange,
                };
                srcPipelineStageFlags |= se_vk_utils_image_layout_to_pipeline_stage_flags(texture->currentLayout);
                dstPipelineStageFlags |= se_vk_utils_image_layout_to_pipeline_stage_flags(layout);
                texture->currentLayout = layout;
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
                nullptr,
                0,
                nullptr,
                numImageBarriers,
                imageBarriers
            );
        }
        //
        // Begin pass
        //
        if (se_vk_ref_type(graphPass->info.pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE)
        {
            VkViewport viewport
            {
                .x          = 0.0f,
                .y          = 0.0f,
                .width      = (float)framebuffer->extent.width,
                .height     = (float)framebuffer->extent.height,
                .minDepth   = 0.0f,
                .maxDepth   = 1.0f,
            };
            VkRect2D scissor
            {
                .offset = { 0, 0 },
                .extent = framebuffer->extent,
            };
            vkCmdSetViewport(commandBuffer->handle, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer->handle, 0, 1, &scissor);
            VkRenderPassBeginInfo beginInfo
            {
                .sType              = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .pNext              = nullptr,
                .renderPass         = renderPass->handle,
                .framebuffer        = framebuffer->handle,
                .renderArea         = { { 0, 0 }, framebuffer->extent },
                .clearValueCount    = se_vk_render_pass_num_attachments(renderPass),
                .pClearValues       = renderPass->clearValues,
            };
            vkCmdBeginRenderPass(commandBuffer->handle, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
        }
        vkCmdBindPipeline(commandBuffer->handle, pipeline->bindPoint, pipeline->handle);
        //
        // Record commands
        //
        for (auto cmdIt : graphPass->commands)
        {
            SeVkGraphCommand& command = iter::value(cmdIt);
            switch (command.type)
            {
                case SE_VK_GRAPH_COMMAND_TYPE_DRAW:
                {
                    SeCommandDrawInfo* draw = &command.info.draw;
                    vkCmdDraw(commandBuffer->handle, draw->numVertices, draw->numInstances, 0, 0);
                } break;
                case SE_VK_GRAPH_COMMAND_TYPE_DISPATCH:
                {
                    SeCommandDispatchInfo* dispatch = &command.info.dispatch;
                    vkCmdDispatch(commandBuffer->handle, dispatch->groupCountX, dispatch->groupCountY, dispatch->groupCountZ);
                } break;
                case SE_VK_GRAPH_COMMAND_TYPE_BIND:
                {
                    SeCommandBindInfo* bindCommandInfo = &command.info.bind;
                    se_assert(bindCommandInfo->numBindings);
                    se_assert(pipeline->numDescriptorSetLayouts > bindCommandInfo->set);
                    //
                    // Allocate descriptor set
                    //
                    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
                    {
                        SeVkGraphDescriptorPool* pool = nullptr;
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
                            VkDescriptorSetAllocateInfo allocateInfo
                            {
                                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                .pNext              = nullptr,
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
                            SeVkGraphDescriptorPool newPool
                            {
                                .handle = VK_NULL_HANDLE,
                                .isLastAllocationSuccessful = true,
                            };
                            se_vk_check(vkCreateDescriptorPool(logicalHandle, &pipeline->descriptorSetLayouts[bindCommandInfo->set].poolCreateInfo, callbacks, &newPool.handle));
                            descriptorPools->pools[descriptorPools->numPools++] = newPool;
                            VkDescriptorSetAllocateInfo allocateInfo
                            {
                                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                .pNext              = nullptr,
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
                        VkWriteDescriptorSet write
                        {
                            .sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext              = nullptr,
                            .dstSet             = descriptorSet,
                            .dstBinding         = binding->binding,
                            .dstArrayElement    = 0,
                            .descriptorCount    = 1,
                            .descriptorType     = pipeline->descriptorSetLayouts[bindCommandInfo->set].bindingInfos[binding->binding].descriptorType,
                            .pImageInfo         = nullptr,
                            .pBufferInfo        = nullptr,
                            .pTexelBufferView   = nullptr,
                        };
                        if (se_vk_ref_type(binding->object) == SE_VK_TYPE_MEMORY_BUFFER)
                        {
                            if (se_vk_ref_flags(binding->object) & SE_VK_GRAPH_MEMORY_BUFFER_SCRATCH)
                            {
                                SeVkMemoryBufferView view = graph->scratchBufferViews[se_vk_ref_index(binding->object)];
                                VkDescriptorBufferInfo* bufferInfo = &bufferInfos[bindingIt];
                                *bufferInfo =
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
                            SeVkSampler* sampler = object_pool::access(se_vk_memory_manager_get_pool<SeVkSampler>(memoryManager), se_vk_ref_index(binding->sampler));
                            VkDescriptorImageInfo* imageInfo = &imageInfos[bindingIt];
                            *imageInfo =
                            {
                                .sampler        = sampler->handle,
                                .imageView      = texture->view,
                                .imageLayout    = texture->currentLayout,
                            };
                            write.pImageInfo = imageInfo;
                        }
                        writes[bindingIt] = write;
                    }
                    vkUpdateDescriptorSets(logicalHandle, bindCommandInfo->numBindings, writes, 0, nullptr);
                    vkCmdBindDescriptorSets(commandBuffer->handle, pipeline->bindPoint, pipeline->layout, bindCommandInfo->set, 1, &descriptorSet, 0, nullptr);
                } break;
                default: { se_assert(!"Unknown SeVkGraphCommand"); }
            };
        }
        //
        // End render pass and submit command buffer
        //
        if (se_vk_ref_type(graphPass->info.pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE)
        {
            vkCmdEndRenderPass(commandBuffer->handle);
        }
        vkEndCommandBuffer(commandBuffer->handle);
        VkSemaphore waitSemaphores[SE_MAX_PASS_DEPENDENCIES];
        VkPipelineStageFlags waitStages[SE_MAX_PASS_DEPENDENCIES];
        uint32_t numWaitSemaphores = 0;
        if (graphPass->info.dependencies)
        {
            for (size_t depIt = 0; depIt < SE_MAX_PASS_DEPENDENCIES; depIt++)
            {
                if (graphPass->info.dependencies & (1ull << depIt))
                {
                    se_assert(depIt < it);
                    SeVkCommandBuffer* depCmd = cmdArray[depIt];
                    waitSemaphores[numWaitSemaphores] = depCmd->semaphore;
                    waitStages[numWaitSemaphores] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // @TODO : optimize
                    numWaitSemaphores += 1;
                    queuePresentDependencies &= ~(1ull << depIt);
                }
            }
        }
        VkSubmitInfo submit
        {
            .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                  = nullptr,
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
        SeVkTexture* swapChainTexture = *se_vk_device_get_swap_chain_texture(graph->device, swapChainTextureIndex);
        SeVkCommandBuffer* transitionCmd = getNewCmd();
        SeVkCommandBufferInfo cmdInfo
        {
            .device = graph->device,
            .usage  = SE_VK_COMMAND_BUFFER_USAGE_TRANSFER,
        };
        se_vk_command_buffer_construct(transitionCmd, &cmdInfo);
        VkImageMemoryBarrier swapChainImageBarrier
        {
            .sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext                  = nullptr,
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
            nullptr,
            0,
            nullptr,
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
                presentWaitSemaphores[numPresentWaitSemaphores] = cmdArray[depIt]->semaphore;
                presentWaitStages[numPresentWaitSemaphores] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                numPresentWaitSemaphores += 1;
            }
        }
        se_assert(numPresentWaitSemaphores);
        presentWaitSemaphores[numPresentWaitSemaphores] = frame->imageAvailableSemaphore;
        presentWaitStages[numPresentWaitSemaphores] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        numPresentWaitSemaphores += 1;
        VkSubmitInfo submit
        {
            .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext                  = nullptr,
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
        VkPresentInfoKHR presentInfo
        {
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &transitionCmd->semaphore,
            .swapchainCount     = se_array_size(swapChains),
            .pSwapchains        = swapChains,
            .pImageIndices      = imageIndices,
            .pResults           = nullptr,
        };
        se_vk_check(vkQueuePresentKHR(se_vk_device_get_command_queue(graph->device, SE_VK_CMD_QUEUE_PRESENT), &presentInfo));
        frame->lastBuffer = transitionCmd;
    }

    dynamic_array::destroy(framePipelines);
    dynamic_array::destroy(frameFramebuffers);
    dynamic_array::destroy(frameRenderPasses);
    dynamic_array::destroy(frameTextures);

    dynamic_array::destroy(graph->textureInfos);
    dynamic_array::destroy(graph->passes);
    dynamic_array::destroy(graph->graphicsPipelineInfos);
    dynamic_array::destroy(graph->computePipelineInfos);
    dynamic_array::destroy(graph->scratchBufferViews);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES;
}

void se_vk_graph_begin_pass(SeVkGraph* graph, const SeBeginPassInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    SeVkDevice* device = graph->device;

    SeVkGraphPass pass
    {
        .info           = info,
        .renderPassInfo = { },
        .commands       = dynamic_array::create<SeVkGraphCommand>(app_allocators::frame(), 64),
    };

    se_assert(se_vk_ref_type(info.pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE || se_vk_ref_type(info.pipeline) == SE_VK_TYPE_COMPUTE_PIPELINE);
    if (se_vk_ref_type(info.pipeline) == SE_VK_TYPE_GRAPHICS_PIPELINE)
    {
        //
        // Render target texture usages
        //
        
        const SeGraphicsPipelineInfo& pipelineInfo = graph->graphicsPipelineInfos[se_vk_ref_index(info.pipeline)];
        se_assert((pipelineInfo.depthState.isTestEnabled || pipelineInfo.depthState.isWriteEnabled) == info.hasDepthStencil);
        se_assert(info.numRenderTargets || info.hasDepthStencil);
        for (size_t it = 0; it < info.numRenderTargets; it++)
        {
            const SeRenderRef rt = info.renderTargets[it].texture;
            se_assert(se_vk_ref_type(rt) == SE_VK_TYPE_TEXTURE);
            if (se_vk_ref_flags(rt) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN)
            {
                continue;
            }
            SeVkTextureInfo* textureInfo = &graph->textureInfos[se_vk_ref_index(rt)];
            se_assert(!se_vk_utils_is_depth_stencil_format(textureInfo->format));
            textureInfo->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        }
        if (info.hasDepthStencil)
        {
            const SeRenderRef rt = info.depthStencilTarget.texture;
            SeVkTextureInfo* textureInfo = &graph->textureInfos[se_vk_ref_index(rt)];
            se_assert(!(se_vk_ref_flags(rt) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN));
            se_assert(se_vk_utils_is_depth_stencil_format(textureInfo->format));
            textureInfo->usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        }
        //
        // Render pass info
        //
        const SeVkTextureInfo* depthStencilTextureInfo = info.hasDepthStencil ? &graph->textureInfos[se_vk_ref_index(info.depthStencilTarget.texture)] : nullptr;
        const VkAttachmentLoadOp depthStencilLoadOp = info.hasDepthStencil ? (VkAttachmentLoadOp)info.depthStencilTarget.loadOp : (VkAttachmentLoadOp)0;
        const uint32_t numColorAttachments = se_vk_safe_cast_size_t_to_uint32_t(info.numRenderTargets);
        SeVkRenderPassInfo renderPassInfo =
        {
            .device                     = device,
            .subpasses                  = { },
            .numSubpasses               = 1,
            .colorAttachments           = { },
            .numColorAttachments        = numColorAttachments,
            .depthStencilAttachment     = depthStencilTextureInfo 
                ? SeVkRenderPassAttachment
                {
                    .format     = se_vk_device_get_depth_stencil_format(device),
                    .loadOp     = depthStencilLoadOp,
                    .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = depthStencilTextureInfo->sampling,  // @TODO : support multisampling (and resolve and stuff)
                    .clearValue = { .depthStencil = { .depth = 0, .stencil = 0 } },
                }
                : SeVkRenderPassAttachment{0},
            .hasDepthStencilAttachment  = depthStencilTextureInfo != nullptr,
        };
        renderPassInfo.subpasses[0] =
        {
            .colorRefs   = 0,
            .inputRefs   = 0,
            .resolveRefs = { },
            .depthRead   = depthStencilTextureInfo != nullptr,
            .depthWrite  = depthStencilTextureInfo != nullptr,
        };
        for (size_t it = 0; it < info.numRenderTargets; it++)
        {
            const SeRenderRef rt = info.renderTargets[it].texture;
            if (se_vk_ref_flags(rt) & SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN)
            {
                renderPassInfo.subpasses[0].colorRefs |= 1 << it;
                renderPassInfo.colorAttachments[it] =
                {
                    .format     = se_vk_device_get_swap_chain_format(graph->device),
                    .loadOp     = (VkAttachmentLoadOp)info.renderTargets[it].loadOp,
                    .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = VK_SAMPLE_COUNT_1_BIT,
                    .clearValue = { .color = {0} },
                };
            }
            else
            {
                const SeVkTextureInfo* textureInfo = &graph->textureInfos[se_vk_ref_index(rt)];
                renderPassInfo.subpasses[0].colorRefs |= 1 << it;
                renderPassInfo.colorAttachments[it] =
                {
                    .format     = textureInfo->format,
                    .loadOp     = (VkAttachmentLoadOp)info.renderTargets[it].loadOp,
                    .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                    .sampling   = textureInfo->sampling, // @TODO : support multisampling (and resolve and stuff)
                    .clearValue = { .color = {0} },
                };
            }
        }
        pass.renderPassInfo = renderPassInfo;
    }
    dynamic_array::push(graph->passes, pass);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS;
}

void se_vk_graph_end_pass(SeVkGraph* graph)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME;
}

SeRenderRef se_vk_graph_program(SeVkGraph* graph, const SeProgramInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME || graph->context == SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES);

    SeVkDevice* device = graph->device;
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    SeVkFrameManager* frameManager = &device->frameManager;

    SeVkProgramInfo vkInfo
    {
        .device         = device,
        .bytecode       = info.bytecode,
        .bytecodeSize   = info.bytecodeSize,
    };
    SeVkProgram* program = nullptr;
    if (graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME)
    {
        SeVkProgram** hashedProgram = hash_table::get(graph->programInfoToProgram, vkInfo);
        if (!hashedProgram)
        {
            hashedProgram = hash_table::set(graph->programInfoToProgram, vkInfo, object_pool::take(se_vk_memory_manager_get_pool<SeVkProgram>(memoryManager)));
            se_vk_program_construct(*hashedProgram, &vkInfo);
        }
        hash_table::set(graph->programInfoToFrame, vkInfo, frameManager->frameNumber);
        program = *hashedProgram;
    }
    else
    {
        program = object_pool::take(se_vk_memory_manager_get_pool<SeVkProgram>(memoryManager));
        se_vk_program_construct(program, &vkInfo);
    }
    se_assert(program);

    return se_vk_ref(SE_VK_TYPE_PROGRAM, 0, object_pool::index_of(se_vk_memory_manager_get_pool<SeVkProgram>(memoryManager), program));
}

SeRenderRef se_vk_graph_texture(SeVkGraph* graph, const SeTextureInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    // @NOTE : we can't create texture right here, because texture usage will be filled later
    SeVkDevice* device = graph->device;
    dynamic_array::push(graph->textureInfos,
    {
        .device     = device,
        .format     = info.format == SE_TEXTURE_FORMAT_DEPTH_STENCIL ? se_vk_device_get_depth_stencil_format(device) : se_vk_utils_to_vk_format(info.format),
        .extent     = { se_vk_safe_cast_size_t_to_uint32_t(info.width), se_vk_safe_cast_size_t_to_uint32_t(info.height), 1 },
        .usage      = 0,
        .sampling   = VK_SAMPLE_COUNT_1_BIT, // @TODO : support multisampling
    });

    return se_vk_ref(SE_VK_TYPE_TEXTURE, 0, dynamic_array::size(graph->textureInfos) - 1);
}

SeRenderRef se_vk_graph_swap_chain_texture(SeVkGraph* graph)
{
    return se_vk_ref(SE_VK_TYPE_TEXTURE, SE_VK_GRAPH_TEXTURE_FROM_SWAP_CHAIN, 0);
}

SeRenderRef se_vk_graph_memory_buffer(SeVkGraph* graph, const SeMemoryBufferInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME || graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    // @TODO : create persistent buffer if se_vk_graph_memory_buffer call was done outside of begin-end pass region
    SeVkFrameManager* frameManager = &graph->device->frameManager;
    SeVkMemoryBufferView view = se_vk_frame_manager_alloc_scratch_buffer(frameManager, info.size);
    dynamic_array::push(graph->scratchBufferViews, view);
    se_assert_msg(view.mappedMemory, "todo : support copies for non host-visible memory buffers");
    if (view.mappedMemory && info.data)
    {
        memcpy(view.mappedMemory, info.data, info.size);
    }

    return se_vk_ref(SE_VK_TYPE_MEMORY_BUFFER, SE_VK_GRAPH_MEMORY_BUFFER_SCRATCH, dynamic_array::size(graph->scratchBufferViews) - 1);
}

SeRenderRef se_vk_graph_graphics_pipeline(SeVkGraph* graph, const SeGraphicsPipelineInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);
    dynamic_array::push(graph->graphicsPipelineInfos, info);
    return se_vk_ref(SE_VK_TYPE_GRAPHICS_PIPELINE, 0, dynamic_array::size(graph->graphicsPipelineInfos) - 1);
}

SeRenderRef se_vk_graph_compute_pipeline(SeVkGraph* graph, const SeComputePipelineInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);
    dynamic_array::push(graph->computePipelineInfos, info);
    return se_vk_ref(SE_VK_TYPE_COMPUTE_PIPELINE, 0, dynamic_array::size(graph->computePipelineInfos) - 1);
}

SeRenderRef se_vk_graph_sampler(SeVkGraph* graph, const SeSamplerInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME || graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkMemoryManager* memoryManager = &graph->device->memoryManager;
    ObjectPool<SeVkSampler>& samplerPool = se_vk_memory_manager_get_pool<SeVkSampler>(memoryManager);
    SeVkFrameManager* frameManager = &graph->device->frameManager;

    SeVkSamplerInfo vkInfo
    {
        .device             = graph->device,
        .magFilter          = (VkFilter)info.magFilter,
        .minFilter          = (VkFilter)info.minFilter,
        .addressModeU       = (VkSamplerAddressMode)info.addressModeU,
        .addressModeV       = (VkSamplerAddressMode)info.addressModeV,
        .addressModeW       = (VkSamplerAddressMode)info.addressModeW,
        .mipmapMode         = (VkSamplerMipmapMode)info.mipmapMode,
        .mipLodBias         = info.mipLodBias,
        .minLod             = info.minLod,
        .maxLod             = info.maxLod,
        .anisotropyEnabled  = info.anisotropyEnable,
        .maxAnisotropy      = info.maxAnisotropy,
        .compareEnabled     = info.compareEnabled,
        .compareOp          = (VkCompareOp)info.compareOp,
    };
    SeVkSampler** sampler = hash_table::get(graph->samplerInfoToSampler, vkInfo);
    if (!sampler)
    {
        sampler = hash_table::set(graph->samplerInfoToSampler, vkInfo, object_pool::take(samplerPool));
        se_vk_sampler_construct(*sampler, &vkInfo);
    }
    hash_table::set(graph->samplerInfoToFrame, vkInfo, frameManager->frameNumber);
    
    return se_vk_ref(SE_VK_TYPE_SAMPLER, 0, object_pool::index_of(samplerPool, *sampler));
}

void se_vk_graph_command_bind(SeVkGraph* graph, const SeCommandBindInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkGraphPass* pass = &graph->passes[dynamic_array::size(graph->passes) - 1];
    se_assert(info.numBindings);
    dynamic_array::push(pass->commands,
    {
        .type = SE_VK_GRAPH_COMMAND_TYPE_BIND,
        .info = { .bind = info },
    });

    for (size_t it = 0; it < info.numBindings; it++)
    {
        const SeRenderRef object = info.bindings[it].object;
        const SeVkType type = se_vk_ref_type(object);
        se_assert(type == SE_VK_TYPE_TEXTURE || type == SE_VK_TYPE_MEMORY_BUFFER);
        if (type == SE_VK_TYPE_TEXTURE)
        {
            SeVkTextureInfo& textureInfo = graph->textureInfos[se_vk_ref_index(object)];
            textureInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        }
    }
}

void se_vk_graph_command_draw(SeVkGraph* graph, const SeCommandDrawInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkGraphPass* pass = &graph->passes[dynamic_array::size(graph->passes) - 1];
    dynamic_array::push(pass->commands,
    {
        .type = SE_VK_GRAPH_COMMAND_TYPE_DRAW,
        .info = { .draw = info },
    });
}
