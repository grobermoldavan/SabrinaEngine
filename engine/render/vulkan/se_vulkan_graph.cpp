
#include "se_vulkan_graph.hpp"
#include "se_vulkan_device.hpp"
#include "se_vulkan_frame_manager.hpp"
#include "se_vulkan_utils.hpp"

constexpr size_t SE_VK_GRAPH_MAX_SETS_IN_DESCRIPTOR_POOL = 64;
constexpr size_t SE_VK_GRAPH_OBJECT_LIFETIME             = 20;
constexpr size_t CONTAINERS_INITIAL_CAPACITY             = 32;

uint32_t se_vk_graph_get_num_bindings(const SeCommandBindInfo& info)
{
    uint32_t result = 0;
    for (size_t it = 0; it < SE_MAX_BINDINGS; it++)
    {
        const SeBinding& binding = info.bindings[it];
        if (!binding) break;
        result += 1;
    }
    return result;
}

VkStencilOpState se_vk_graph_pipeline_stencil_op_state(const SeStencilOpState* state)
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
void se_vk_graph_free_old_resources(HashTable<Key, SeVkGraphWithFrame<Value>>& table, size_t currentFrame, SeVkMemoryManager* memoryManager)
{
    DynamicArray<Key> toRemove = dynamic_array::create<Key>(allocators::frame());
    ObjectPool<Value>& pool = se_vk_memory_manager_get_pool<Value>(memoryManager);
    for (auto kv : table)
    {
        if (!hash_table::get(table, iter::key(kv)))
        {
            debug::message("Probably hash table key contains pointer to value that was changed after hash_table::set. Key: {}", iter::key(kv));
            debug::message("Table size: {}. Table members:", table.size);
            for (auto it : table)
            {
                debug::message("[index: {}, key: {}, value: {}]", it.iterator->index, iter::key(it), iter::value(it));
            }
            se_assert(false);
        }
        if ((currentFrame - iter::value(kv).frame) > SE_VK_GRAPH_OBJECT_LIFETIME)
        {
            dynamic_array::push(toRemove, iter::key(kv));
        }
    }
    for (auto it : toRemove)
    {
        SeVkGraphWithFrame<Value>* value = hash_table::get(table, iter::value(it));
        se_assert(value);
        se_vk_destroy(value->object);
        object_pool::release<Value>(pool, value->object);
        hash_table::remove(table, iter::value(it));
    }
    dynamic_array::destroy(toRemove);
}

void se_vk_graph_construct(SeVkGraph* graph, const SeVkGraphInfo* info)
{
    const AllocatorBindings persistentAllocator = allocators::persistent();
    const AllocatorBindings frameAllocator = allocators::frame();

    *graph =
    {
        .device                     = info->device,
        .context                    = SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES,
        .passes                     = { }, // @NOTE : constructed at begin frame
        .renderPassInfoToRenderPass             = { },
        .framebufferInfoToFramebuffer           = { },
        .graphicsPipelineInfoToGraphicsPipeline = { },
        .computePipelineInfoToComputePipeline   = { },
        .pipelineToDescriptorPools              = { },
    };

    hash_table::construct(graph->renderPassInfoToRenderPass, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->framebufferInfoToFramebuffer, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->graphicsPipelineInfoToGraphicsPipeline, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->computePipelineInfoToComputePipeline, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
    hash_table::construct(graph->pipelineToDescriptorPools, persistentAllocator, CONTAINERS_INITIAL_CAPACITY);
}

void se_vk_graph_destroy(SeVkGraph* graph)
{
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&graph->device->memoryManager);

    dynamic_array::destroy(graph->passes);

    hash_table::destroy(graph->renderPassInfoToRenderPass);
    hash_table::destroy(graph->framebufferInfoToFramebuffer);
    hash_table::destroy(graph->graphicsPipelineInfoToGraphicsPipeline);
    hash_table::destroy(graph->computePipelineInfoToComputePipeline);

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

    const VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    SeVkMemoryManager* const memoryManager = &graph->device->memoryManager;
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeVkFrameManager* const frameManager = &graph->device->frameManager;
    const size_t currentFrame = frameManager->frameNumber;
    const size_t frameIndex = se_vk_frame_manager_get_active_frame_index(frameManager);

    for (auto kv : graph->pipelineToDescriptorPools)
    {
        auto& pools = iter::value(kv);
        if ((currentFrame - pools.lastFrame) <= SE_VK_GRAPH_OBJECT_LIFETIME) continue;
        for (size_t it = 0; it < pools.numPools; it++)
            vkDestroyDescriptorPool(logicalHandle, pools.pools[it].handle, callbacks);
        iter::remove(kv);
    }
    se_vk_graph_free_old_resources(graph->graphicsPipelineInfoToGraphicsPipeline, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->computePipelineInfoToComputePipeline, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->framebufferInfoToFramebuffer, currentFrame, memoryManager);
    se_vk_graph_free_old_resources(graph->renderPassInfoToRenderPass, currentFrame, memoryManager);

    dynamic_array::construct(graph->passes, allocators::frame(), CONTAINERS_INITIAL_CAPACITY);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME;
}

void se_vk_graph_end_frame(SeVkGraph* graph)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    const VkDevice logicalHandle = se_vk_device_get_logical_handle(graph->device);
    SeVkMemoryManager* const memoryManager = &graph->device->memoryManager;
    const AllocatorBindings frameAllocator = allocators::frame();

    ObjectPool<SeVkRenderPass>&     renderPassPool      = se_vk_memory_manager_get_pool<SeVkRenderPass>(memoryManager);
    ObjectPool<SeVkFramebuffer>&    framebufferPool     = se_vk_memory_manager_get_pool<SeVkFramebuffer>(memoryManager);
    ObjectPool<SeVkPipeline>&       pipelinePool        = se_vk_memory_manager_get_pool<SeVkPipeline>(memoryManager);
    
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeVkFrameManager* const frameManager = &graph->device->frameManager;
    SeVkFrame* const frame = se_vk_frame_manager_get_active_frame(frameManager);
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
        const SeVkFrame* const prevImageFrame = &frameManager->frames[frameManager->imageToFrame[swapChainTextureIndex]];
        const SeVkCommandBuffer* const* const lastBuffer = dynamic_array::last(prevImageFrame->commandBuffers);
        se_assert(lastBuffer);
        const VkFence fence = (*lastBuffer)->fence;
        vkWaitForFences(logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    frameManager->imageToFrame[swapChainTextureIndex] = frameIndex;

    //
    // Render passes
    //

    const size_t numPasses = dynamic_array::size(graph->passes);
    DynamicArray<SeVkRenderPass*> frameRenderPasses = dynamic_array::create<SeVkRenderPass*>(frameAllocator, numPasses);
    for (size_t it = 0; it < numPasses; it++)
    {
        const bool isCompute = graph->passes[it].type == SeVkGraphPass::COMPUTE;
        if (isCompute)
        {
            dynamic_array::push(frameRenderPasses, nullptr);
        }
        else
        {
            SeVkRenderPassInfo& info = graph->passes[it].renderPassInfo;
            SeVkGraphWithFrame<SeVkRenderPass>* pass = hash_table::get(graph->renderPassInfoToRenderPass, info);
            if (!pass)
            {
                pass = hash_table::set(graph->renderPassInfoToRenderPass, info, { object_pool::take(renderPassPool), currentFrame });
                se_vk_render_pass_construct(pass->object, &info);
            }
            else
            {
                pass->frame = currentFrame;
            }
            dynamic_array::push(frameRenderPasses, pass->object);
        }
    }

    //
    // Framebuffers
    //

    DynamicArray<SeVkFramebuffer*> frameFramebuffers = dynamic_array::create<SeVkFramebuffer*>(frameAllocator, numPasses);
    for (size_t it = 0; it < numPasses; it++)
    {
        const SeVkGraphPass* pass = &graph->passes[it];
        if (pass->type == SeVkGraphPass::COMPUTE)
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
                const SeTextureRef textureRef = pass->graphicsPassInfo.renderTargets[textureIt].texture;
                if (textureRef.isSwapChain)
                    info.textures[textureIt] = se_vk_device_get_swap_chain_texture(graph->device, swapChainTextureIndex);
                else
                    info.textures[textureIt] = se_vk_to_pool_ref(textureRef);
            }
            if (pass->renderPassInfo.hasDepthStencilAttachment)
                info.textures[info.numTextures - 1] = se_vk_to_pool_ref(pass->graphicsPassInfo.depthStencilTarget.texture);

            SeVkGraphWithFrame<SeVkFramebuffer>* framebuffer = hash_table::get(graph->framebufferInfoToFramebuffer, info);
            if (!framebuffer)
            {
                SeVkFramebuffer* _framebuffer = object_pool::take(framebufferPool);
                se_vk_framebuffer_construct(_framebuffer, &info);
                framebuffer = hash_table::set(graph->framebufferInfoToFramebuffer, info, { _framebuffer, currentFrame });
            }
            else
            {
                framebuffer->frame = currentFrame;
            }
            dynamic_array::push(frameFramebuffers, framebuffer->object);
        }
    }

    //
    // Pipelines
    //

    DynamicArray<SeVkPipeline*> framePipelines = dynamic_array::create<SeVkPipeline*>(frameAllocator, numPasses);
    for (size_t it = 0; it < numPasses; it++)
    {
        SeVkPipeline* pipeline = nullptr;
        const bool isCompute = graph->passes[it].type == SeVkGraphPass::COMPUTE;
        if (isCompute)
        {
            se_assert(!frameRenderPasses[it]);
            const SeComputePassInfo& seInfo = graph->passes[it].computePassInfo;
            SeVkProgramWithConstants program
            {
                .program                    = se_vk_unref(seInfo.program.program),
                .constants                  = { },
                .numSpecializationConstants = seInfo.program.numSpecializationConstants,
            };
            memcpy(program.constants, seInfo.program.specializationConstants, sizeof(SeSpecializationConstant) * seInfo.program.numSpecializationConstants);
            SeVkComputePipelineInfo vkInfo
            {
                .device = graph->device,
                .program = program,
            };
            SeVkGraphWithFrame<SeVkPipeline>* pipelineTimed = hash_table::get(graph->computePipelineInfoToComputePipeline, vkInfo);
            if (!pipelineTimed)
            {
                pipelineTimed = hash_table::set(graph->computePipelineInfoToComputePipeline, vkInfo, { object_pool::take(pipelinePool), currentFrame });
                se_vk_pipeline_compute_construct(pipelineTimed->object, &vkInfo);
            }
            else
            {
                pipelineTimed->frame = currentFrame;
            }
            pipeline = pipelineTimed->object;
        }
        else
        {
            se_assert(frameRenderPasses[it]);
            const SeGraphicsPassInfo& seInfo = graph->passes[it].graphicsPassInfo;
            SeVkProgramWithConstants vertexProgram
            {
                .program                    = se_vk_unref(seInfo.vertexProgram.program),
                .constants                  = { },
                .numSpecializationConstants = seInfo.vertexProgram.numSpecializationConstants,
            };
            memcpy(vertexProgram.constants, seInfo.vertexProgram.specializationConstants, sizeof(SeSpecializationConstant) * seInfo.vertexProgram.numSpecializationConstants);
            SeVkProgramWithConstants fragmentProgram
            {
                .program                    = se_vk_unref(seInfo.fragmentProgram.program),
                .constants                  = { },
                .numSpecializationConstants = seInfo.fragmentProgram.numSpecializationConstants,
            };
            memcpy(fragmentProgram.constants, seInfo.fragmentProgram.specializationConstants, sizeof(SeSpecializationConstant) * seInfo.fragmentProgram.numSpecializationConstants);
            SeVkGraphicsPipelineInfo vkInfo
            {
                .device                 = graph->device,
                .pass                   = frameRenderPasses[it],
                .vertexProgram          = vertexProgram,
                .fragmentProgram        = fragmentProgram,
                .subpassIndex           = 0,
                .isStencilTestEnabled   = seInfo.frontStencilOpState.isEnabled || seInfo.backStencilOpState.isEnabled,
                .frontStencilOpState    = seInfo.frontStencilOpState.isEnabled ? se_vk_graph_pipeline_stencil_op_state(&seInfo.frontStencilOpState) : VkStencilOpState{0},
                .backStencilOpState     = seInfo.backStencilOpState.isEnabled ? se_vk_graph_pipeline_stencil_op_state(&seInfo.backStencilOpState) : VkStencilOpState{0},
                .isDepthTestEnabled     = seInfo.depthState.isTestEnabled,
                .isDepthWriteEnabled    = seInfo.depthState.isWriteEnabled,
                .polygonMode            = se_vk_utils_to_vk_polygon_mode(seInfo.polygonMode),
                .cullMode               = se_vk_utils_to_vk_cull_mode(seInfo.cullMode),
                .frontFace              = se_vk_utils_to_vk_front_face(seInfo.frontFace),
                .sampling               = se_vk_utils_to_vk_sample_count(seInfo.samplingType),
            };
            SeVkGraphWithFrame<SeVkPipeline>* pipelineTimed = hash_table::get(graph->graphicsPipelineInfoToGraphicsPipeline, vkInfo);
            if (!pipelineTimed)
            {
                pipelineTimed = hash_table::set(graph->graphicsPipelineInfoToGraphicsPipeline, vkInfo, { object_pool::take(pipelinePool), currentFrame });
                se_vk_pipeline_graphics_construct(pipelineTimed->object, &vkInfo);
            }
            else
            {
                pipelineTimed->frame = currentFrame;
            }
            pipeline = pipelineTimed->object;
        }
        se_assert(pipeline);
        dynamic_array::push(framePipelines, pipeline);
    }

    //
    // Reset descriptor pools
    //

    for (auto kv : graph->pipelineToDescriptorPools)
    {
        const auto& info = iter::key(kv);
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
        SeVkGraphPass* const graphPass = &graph->passes[it];
        SeVkPipeline* const pipeline = framePipelines[it];
        SeVkFramebuffer* const framebuffer = frameFramebuffers[it];
        SeVkRenderPass* const renderPass = frameRenderPasses[it];
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
        descriptorPools->lastFrame = currentFrame;
        //
        // Create command buffer
        //
        SeVkCommandBufferInfo cmdInfo
        {
            .device = graph->device,
            .usage  = graphPass->type == SeVkGraphPass::COMPUTE
                        ? SE_VK_COMMAND_BUFFER_USAGE_COMPUTE
                        : (SE_VK_COMMAND_BUFFER_USAGE_GRAPHICS | SE_VK_COMMAND_BUFFER_USAGE_TRANSFER),
        };
        SeVkCommandBuffer* commandBuffer = se_vk_frame_manager_get_cmd(frameManager, &cmdInfo);
        //
        // Get list of all texture layout transitions necessary for this pass
        //
        HashTable<SeVkTexture*, VkImageLayout> transitions;
        hash_table::construct(transitions, allocators::frame(), SeVkConfig::FRAMEBUFFER_MAX_TEXTURES + SE_MAX_BINDINGS);
        if (graphPass->type == SeVkGraphPass::GRAPHICS)
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
                const uint32_t numBindings = se_vk_graph_get_num_bindings(command.info.bind);
                for (size_t bindingIt = 0; bindingIt < numBindings; bindingIt++)
                {
                    const SeBinding& binding = command.info.bind.bindings[bindingIt];
                    if (binding.type == SeBinding::TEXTURE)
                    {
                        SeVkTexture* const texture = se_vk_unref(binding.texture.texture);
                        const bool isDepth = se_vk_utils_is_depth_stencil_format(texture->format);
                        VkImageLayout* const layout = hash_table::get(transitions, texture);
                        if (!layout)
                        {
                            // @TODO : use more precise layout pick logic (different layouts for depth- or stencil-only formats) :
                            // https://vulkan.lunarg.com/doc/view/1.2.198.1/windows/1.2-extensions/vkspec.html#descriptorsets-combinedimagesample
                            const VkImageLayout newLayout = se_vk_utils_is_depth_stencil_format(texture->format)
                                ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                            hash_table::set(transitions, texture, newLayout);
                        }
                        else if ((*layout != VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) && (*layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL))
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
        VkImageMemoryBarrier imageBarriers[SeVkConfig::FRAMEBUFFER_MAX_TEXTURES + SE_MAX_BINDINGS];
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
        if (graphPass->type == SeVkGraphPass::GRAPHICS)
        {
            const VkViewport viewport
            {
                .x          = 0.0f,
                .y          = 0.0f,
                .width      = (float)framebuffer->extent.width,
                .height     = (float)framebuffer->extent.height,
                .minDepth   = 0.0f,
                .maxDepth   = 1.0f,
            };
            const VkRect2D scissor
            {
                .offset = { 0, 0 },
                .extent = framebuffer->extent,
            };
            vkCmdSetViewport(commandBuffer->handle, 0, 1, &viewport);
            vkCmdSetScissor(commandBuffer->handle, 0, 1, &scissor);
            const VkRenderPassBeginInfo beginInfo
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
                    const SeCommandDrawInfo* const draw = &command.info.draw;
                    vkCmdDraw(commandBuffer->handle, draw->numVertices, draw->numInstances, 0, 0);
                } break;
                case SE_VK_GRAPH_COMMAND_TYPE_DISPATCH:
                {
                    const SeCommandDispatchInfo* const dispatch = &command.info.dispatch;
                    vkCmdDispatch(commandBuffer->handle, dispatch->groupCountX, dispatch->groupCountY, dispatch->groupCountZ);
                } break;
                case SE_VK_GRAPH_COMMAND_TYPE_BIND:
                {
                    const SeCommandBindInfo* const bindCommandInfo = &command.info.bind;
                    const uint32_t numBindings = se_vk_graph_get_num_bindings(*bindCommandInfo);
                    se_assert(numBindings);
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
                            se_assert(descriptorPools->numPools < SeVkConfig::GRAPH_MAX_POOLS_IN_ARRAY);
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
                    VkDescriptorImageInfo imageInfos[SeVkConfig::RENDER_PIPELINE_MAX_DESCRIPTOR_SETS] = {};
                    VkDescriptorBufferInfo bufferInfos[SeVkConfig::RENDER_PIPELINE_MAX_DESCRIPTOR_SETS] = {};
                    VkWriteDescriptorSet writes[SeVkConfig::RENDER_PIPELINE_MAX_DESCRIPTOR_SETS] = {};
                    for (uint32_t bindingIt = 0; bindingIt < numBindings; bindingIt++)
                    {
                        const SeBinding* const binding = &bindCommandInfo->bindings[bindingIt];
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
                        if (binding->type == SeBinding::TEXTURE)
                        {
                            SeVkTexture* const texture = se_vk_unref(binding->texture.texture);
                            SeVkSampler* const sampler = se_vk_unref(binding->texture.sampler);
                            VkDescriptorImageInfo* const imageInfo = &imageInfos[bindingIt];
                            *imageInfo =
                            {
                                .sampler        = sampler->handle,
                                .imageView      = texture->view,
                                .imageLayout    = texture->currentLayout,
                            };
                            write.pImageInfo = imageInfo;
                        }
                        else
                        {
                            const auto& bufferBinding = binding->buffer;
                            const SeBufferRef bufferRef = binding->buffer.buffer;
                            const bool isScratch = bufferBinding.buffer.isScratch;

                            const SeVkMemoryBuffer* const buffer = isScratch
                                ? frame->scratchBuffer
                                : se_vk_unref(bufferRef);
                            VkDescriptorBufferInfo* const bufferInfo = &bufferInfos[bindingIt];

                            se_assert_msg(!isScratch || bufferBinding.buffer.generation == currentFrame, "Scratch buffers are meant to be created every frame");
                            se_assert_msg(!isScratch || bufferBinding.offset < frame->scratchBufferViews[bufferRef.index].size, "Scratch buffer binding offset is too big");
                            se_assert_msg(!isScratch || bufferBinding.size <= (frame->scratchBufferViews[bufferRef.index].size - bufferBinding.offset), "Scratch buffer binding size is too big");
                            se_assert_msg(isScratch || bufferBinding.offset < buffer->memory.size, "Buffer binding offset is too big");
                            se_assert_msg(isScratch || bufferBinding.size <= (buffer->memory.size - bufferBinding.offset), "Buffer binding size is too big");

                            const size_t offset = isScratch
                                    ? frame->scratchBufferViews[bufferRef.index].offset + bufferBinding.offset
                                    : bufferBinding.offset;
                            const size_t range = bufferBinding.size
                                    ? bufferBinding.size
                                    : (isScratch ? frame->scratchBufferViews[bufferRef.index].size - bufferBinding.offset : VK_WHOLE_SIZE);
                            *bufferInfo =
                            {
                                .buffer = buffer->handle,
                                .offset = offset,
                                .range  = range,
                            };
                            write.pBufferInfo = bufferInfo;
                        }
                        writes[bindingIt] = write;
                    }
                    vkUpdateDescriptorSets(logicalHandle, numBindings, writes, 0, nullptr);
                    vkCmdBindDescriptorSets(commandBuffer->handle, pipeline->bindPoint, pipeline->layout, bindCommandInfo->set, 1, &descriptorSet, 0, nullptr);
                } break;
                default: { se_assert(!"Unknown SeVkGraphCommand"); }
            };
        }
        //
        // End render pass and submit command buffer
        //
        if (graphPass->type == SeVkGraphPass::GRAPHICS)
        {
            vkCmdEndRenderPass(commandBuffer->handle);
        }
        SeVkCommandBufferSubmitInfo submitInfo = { };
        size_t depCounter = 0;
        const SePassDependencies deps = graphPass->type == SeVkGraphPass::GRAPHICS ? graphPass->graphicsPassInfo.dependencies : graphPass->computePassInfo.dependencies;
        if (deps)
        {
            for (size_t depIt = 0; depIt < SE_MAX_PASS_DEPENDENCIES; depIt++)
            {
                if (deps & (1ull << depIt))
                {
                    se_assert(depIt < it);
                    submitInfo.executeAfter[depCounter++] = frame->commandBuffers[depIt];
                    queuePresentDependencies &= ~(1ull << depIt);
                }
            }
        }
        se_vk_command_buffer_submit(commandBuffer, &submitInfo);
    }

    //
    // Present swap chain image
    //

    {
        //
        // Transition swap chain image layout
        //
        SeVkCommandBufferInfo cmdInfo
        {
            .device = graph->device,
            .usage  = SE_VK_COMMAND_BUFFER_USAGE_TRANSFER,
        };
        SeVkCommandBuffer* const transitionCmd = se_vk_frame_manager_get_cmd(frameManager, &cmdInfo);
        SeVkTexture* const swapChainTexture = *se_vk_device_get_swap_chain_texture(graph->device, swapChainTextureIndex);
        const VkImageMemoryBarrier swapChainImageBarrier
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
        SeVkCommandBufferSubmitInfo submitInfo = { };
        size_t depCounter = 0;
        for (size_t depIt = 0; depIt < SE_MAX_PASS_DEPENDENCIES; depIt++)
        {
            if (queuePresentDependencies & (1ull << depIt))
            {
                submitInfo.executeAfter[depCounter++] = frame->commandBuffers[depIt];
            }
        }
        submitInfo.waitSemaphores[0] = { frame->imageAvailableSemaphore };
        se_vk_command_buffer_submit(transitionCmd, &submitInfo);
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
    }

    dynamic_array::destroy(framePipelines);
    dynamic_array::destroy(frameFramebuffers);
    dynamic_array::destroy(frameRenderPasses);
    dynamic_array::destroy(graph->passes);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_BETWEEN_FRAMES;
}

SePassDependencies se_vk_graph_begin_graphics_pass(SeVkGraph* graph, const SeGraphicsPassInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    SeVkGraphPass pass
    {
        .type               = SeVkGraphPass::GRAPHICS,
        .graphicsPassInfo   = info,
        .renderPassInfo     = { },
        .commands           = dynamic_array::create<SeVkGraphCommand>(allocators::frame(), 64),
    };
    const SeVkTexture* const depthStencilTexture = info.depthStencilTarget ? se_vk_unref(info.depthStencilTarget.texture) : nullptr;
    se_assert_msg
    (
        (info.depthState.isTestEnabled | info.depthState.isWriteEnabled) == (depthStencilTexture != nullptr),
        "Pipeline must have depth stensil target if depth test or depth write are enabled. If both options are disabled depth texture must be null"
    );
    const VkAttachmentLoadOp depthStencilLoadOp = info.depthStencilTarget ? se_vk_utils_to_vk_load_op(info.depthStencilTarget.loadOp) : (VkAttachmentLoadOp)0;
    const uint32_t numColorAttachments = [info]() -> uint32_t
    {
        uint32_t result = 0;
        for (; result < SE_MAX_PASS_RENDER_TARGETS; result++) if (!info.renderTargets[result]) break;
        return result;
    }();

    //
    // Render pass info
    //

    SeVkRenderPassInfo renderPassInfo =
    {
        .device                     = graph->device,
        .subpasses                  = { },
        .numSubpasses               = 1,
        .colorAttachments           = { },
        .numColorAttachments        = numColorAttachments,
        .depthStencilAttachment     = depthStencilTexture 
            ? SeVkRenderPassAttachment
            {
                .format     = depthStencilTexture->format,
                .loadOp     = depthStencilLoadOp,
                .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
                .sampling   = VK_SAMPLE_COUNT_1_BIT,  // @TODO : support multisampling (and resolve and stuff)
                .clearValue = { .depthStencil = { .depth = 0, .stencil = 0 } },
            }
            : SeVkRenderPassAttachment{ },
        .hasDepthStencilAttachment  = depthStencilTexture != nullptr,
    };
    renderPassInfo.subpasses[0] =
    {
        .colorRefs   = 0,
        .inputRefs   = 0,
        .resolveRefs = { },
        .depthRead   = depthStencilTexture != nullptr,
        .depthWrite  = depthStencilTexture != nullptr,
    };
    for (uint32_t it = 0; it < numColorAttachments; it++)
    {
        renderPassInfo.subpasses[0].colorRefs |= 1 << it;
        const SePassRenderTarget& target = info.renderTargets[it];
        const VkFormat format = target.texture.isSwapChain ? se_vk_device_get_swap_chain_format(graph->device) : se_vk_unref(target.texture)->format;
        const bool isDefaultClearValue = utils::compare(target.clearColor, SeColorUnpacked{});
        // @TODO : support clear values for INT and UINT textures
        //         https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkClearColorValue.html
        se_assert_msg(isDefaultClearValue || (se_vk_utils_get_format_info(format).sampledType == SeVkFormatInfo::Type::FLOAT), "Clear values are only supported for floating point textures");
        se_assert_msg(target.clearColor.r >= 0.0f && target.clearColor.r <= 1.0f, "Clear values must be in range [0.0, 1.0]");
        se_assert_msg(target.clearColor.g >= 0.0f && target.clearColor.g <= 1.0f, "Clear values must be in range [0.0, 1.0]");
        se_assert_msg(target.clearColor.b >= 0.0f && target.clearColor.b <= 1.0f, "Clear values must be in range [0.0, 1.0]");
        se_assert_msg(target.clearColor.a >= 0.0f && target.clearColor.a <= 1.0f, "Clear values must be in range [0.0, 1.0]");
        renderPassInfo.colorAttachments[it] =
        {
            .format     = format,
            .loadOp     = se_vk_utils_to_vk_load_op(target.loadOp),
            .storeOp    = VK_ATTACHMENT_STORE_OP_STORE,
            .sampling   = VK_SAMPLE_COUNT_1_BIT, // @TODO : support multisampling (and resolve and stuff)
            .clearValue = { .color = { .float32 = { target.clearColor.r, target.clearColor.g, target.clearColor.b, target.clearColor.a } } },
        };
    }
    pass.renderPassInfo = renderPassInfo;
    dynamic_array::push(graph->passes, pass);

    //
    // Pipeline info
    //

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS;
    return 1ull << (dynamic_array::size(graph->passes) - 1);
}

SePassDependencies se_vk_graph_begin_compute_pass(SeVkGraph* graph, const SeComputePassInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME);

    dynamic_array::push(graph->passes,
    {
        .type               = SeVkGraphPass::COMPUTE,
        .computePassInfo    = info,
        .renderPassInfo     = { },
        .commands           = dynamic_array::create<SeVkGraphCommand>(allocators::frame(), 64),
    });

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS;
    return 1ull << (dynamic_array::size(graph->passes) - 1);
}

void se_vk_graph_end_pass(SeVkGraph* graph)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    graph->context = SE_VK_GRAPH_CONTEXT_TYPE_IN_FRAME;
}

void se_vk_graph_command_bind(SeVkGraph* graph, const SeCommandBindInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkGraphPass* const pass = &graph->passes[dynamic_array::size(graph->passes) - 1];
    dynamic_array::push(pass->commands,
    {
        .type = SE_VK_GRAPH_COMMAND_TYPE_BIND,
        .info = { .bind = info },
    });
}

void se_vk_graph_command_draw(SeVkGraph* graph, const SeCommandDrawInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkGraphPass* const pass = &graph->passes[dynamic_array::size(graph->passes) - 1];
    dynamic_array::push(pass->commands,
    {
        .type = SE_VK_GRAPH_COMMAND_TYPE_DRAW,
        .info = { .draw = info },
    });
}

void se_vk_graph_command_dispatch(SeVkGraph* graph, const SeCommandDispatchInfo& info)
{
    se_assert(graph->context == SE_VK_GRAPH_CONTEXT_TYPE_IN_PASS);

    SeVkGraphPass* const pass = &graph->passes[dynamic_array::size(graph->passes) - 1];
    dynamic_array::push(pass->commands,
    {
        .type = SE_VK_GRAPH_COMMAND_TYPE_DISPATCH,
        .info = { .dispatch = info },
    });
}
