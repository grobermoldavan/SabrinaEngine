
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_render_pipeline.h"
#include "engine/allocator_bindings.h"
#include "engine/render_abstraction_interface.h"

#define SE_VK_UNUSED_IN_FLIGHT_DATA_REF ~((uint32_t)0)

void se_vk_in_flight_manager_construct(SeVkInFlightManager* manager, SeVkInFlightManagerCreateInfo* createInfo)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    // 
    //
    manager->device = createInfo->device;
    se_sbuffer_construct(manager->inFlightDatas, createInfo->numImagesInFlight, allocator);
    se_sbuffer_construct(manager->registeredPipelines, 64, allocator);
    se_sbuffer_construct(manager->swapChainImageToInFlightFrameMap, createInfo->numSwapChainImages, allocator);
    se_sbuffer_set_size(manager->inFlightDatas, createInfo->numImagesInFlight);
    se_sbuffer_set_size(manager->swapChainImageToInFlightFrameMap, createInfo->numSwapChainImages);
    VkSemaphoreCreateInfo semaphoreCreateInfo = (VkSemaphoreCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    for (size_t it = 0; it < createInfo->numImagesInFlight; it++)
    {
        SeVkInFlightData* data = &manager->inFlightDatas[it];
        se_vk_check(vkCreateSemaphore(logicalHandle, &semaphoreCreateInfo, callbacks, &data->imageAvailableSemaphore));
        se_sbuffer_construct(data->submittedCommandBuffers, 8, allocator);
        se_sbuffer_construct(data->renderPipelinePools, 64, allocator);
    }
    for (size_t it = 0; it < createInfo->numSwapChainImages; it++)
    {
        manager->swapChainImageToInFlightFrameMap[it] = SE_VK_UNUSED_IN_FLIGHT_DATA_REF;
    }
    manager->currentImageInFlight = 0;
    manager->currentSwapChainImageIndex = 0;
}

void se_vk_in_flight_manager_destroy(SeVkInFlightManager* manager)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(manager->device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    //
    //
    //
    const size_t numInFlightDatas = se_sbuffer_size(manager->inFlightDatas);
    const size_t numRegisteredPipelines = se_sbuffer_size(manager->registeredPipelines);
    for (size_t it = 0; it < numInFlightDatas; it++)
    {
        SeVkInFlightData* inFlightData = &manager->inFlightDatas[it];
        vkDestroySemaphore(logicalHandle, inFlightData->imageAvailableSemaphore, callbacks);
        const size_t numSubmittedCommandBuffers = se_sbuffer_size(inFlightData->submittedCommandBuffers);
        for (size_t cbIt = 0; cbIt < numSubmittedCommandBuffers; cbIt++)
        {
            se_vk_command_buffer_destroy(inFlightData->submittedCommandBuffers[cbIt]);
        }
        for (size_t pipelineIt = 0; pipelineIt < numRegisteredPipelines; pipelineIt++)
        {
            const size_t numSets = se_sbuffer_size(inFlightData->renderPipelinePools[pipelineIt].setPools);
            for (size_t setIt = 0; setIt < numSets; setIt++)
            {
                const size_t numPools = se_sbuffer_size(inFlightData->renderPipelinePools[pipelineIt].setPools[setIt]);
                for (size_t poolIt = 0; poolIt < numPools; poolIt++)
                {
                    vkDestroyDescriptorPool(logicalHandle, inFlightData->renderPipelinePools[pipelineIt].setPools[setIt][poolIt].handle, callbacks);
                }
                se_sbuffer_destroy(inFlightData->renderPipelinePools[pipelineIt].setPools[setIt]);
            }
            se_sbuffer_destroy(inFlightData->renderPipelinePools[pipelineIt].setPools);
        }
        se_sbuffer_destroy(inFlightData->submittedCommandBuffers);
        se_sbuffer_destroy(inFlightData->renderPipelinePools);
    }
    se_sbuffer_destroy(manager->inFlightDatas);
    se_sbuffer_destroy(manager->registeredPipelines);
    se_sbuffer_destroy(manager->swapChainImageToInFlightFrameMap);
}

void se_vk_in_flight_manager_advance_frame(SeVkInFlightManager* manager)
{
    /*
        1. Advance in flight index
        2. Wait until previous command buffers of this frame finish execution
        3. Reset all resource set pools
        4. Acquire next image and set the image semaphore to wait for
        5. If this image is already used by another in flight frame, wait for it's execution fence too
        6. Save in flight frame reference to tha map
    */
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    //
    // 1. Advance in flight index
    //
    manager->currentImageInFlight = (manager->currentImageInFlight + 1) % se_sbuffer_size(manager->inFlightDatas);
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    //
    // 2. Wait until previous command buffers of this frame finish execution
    //
    se_sbuffer(SeRenderObject*) activeFrameCommandBuffers = inFlightData->submittedCommandBuffers;
    const size_t numCommandBuffers = se_sbuffer_size(activeFrameCommandBuffers);
    if (numCommandBuffers)
    {
        SeRenderObject* lastCommandBuffer = activeFrameCommandBuffers[numCommandBuffers - 1];
        VkFence fence = se_vk_command_buffer_get_fence(lastCommandBuffer);
        vkWaitForFences(logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
        for (size_t it = 0; it < numCommandBuffers; it++)
        {
            se_vk_command_buffer_destroy(activeFrameCommandBuffers[it]);
        }
        se_sbuffer_set_size(activeFrameCommandBuffers, 0);
    }
    //
    // 3. Reset all resource set pools
    //
    const size_t numRegisteredPipelines = se_sbuffer_size(inFlightData->renderPipelinePools);
    for (size_t pipelineIt = 0; pipelineIt < numRegisteredPipelines; pipelineIt++)
    {
        const size_t numSets = se_sbuffer_size(inFlightData->renderPipelinePools[pipelineIt].setPools);
        for (size_t setIt = 0; setIt < numSets; setIt++)
        {
            const size_t numPools = se_sbuffer_size(inFlightData->renderPipelinePools[pipelineIt].setPools[setIt]);
            for (size_t poolIt = 0; poolIt < numPools; poolIt++)
            {
                vkResetDescriptorPool(logicalHandle, inFlightData->renderPipelinePools[pipelineIt].setPools[setIt][poolIt].handle, 0);
                inFlightData->renderPipelinePools[pipelineIt].setPools[setIt][poolIt].numAllocations = 0;
                inFlightData->renderPipelinePools[pipelineIt].setPools[setIt][poolIt].isLastAllocationSuccessful = true;
            }
        }
    }
    //
    // 4. Acquire next image and set the image semaphore to wait for
    //
    se_vk_check(vkAcquireNextImageKHR(logicalHandle, se_vk_device_get_swap_chain_handle(manager->device), UINT64_MAX, inFlightData->imageAvailableSemaphore, VK_NULL_HANDLE, &manager->currentSwapChainImageIndex));
    //
    // 5. If this image is already used by another in flight frame, wait for it's execution fence too
    //
    const uint32_t inFlightImageReferencedBySwapChainImage = manager->swapChainImageToInFlightFrameMap[manager->currentSwapChainImageIndex];
    if (inFlightImageReferencedBySwapChainImage != SE_VK_UNUSED_IN_FLIGHT_DATA_REF)
    {
        se_sbuffer(SeRenderObject*) referencedCommandBuffers = manager->inFlightDatas[inFlightImageReferencedBySwapChainImage].submittedCommandBuffers;
        if (se_sbuffer_size(referencedCommandBuffers))
        {
            SeRenderObject* lastCommandBuffer = referencedCommandBuffers[se_sbuffer_size(referencedCommandBuffers) - 1];
            VkFence fence = se_vk_command_buffer_get_fence(lastCommandBuffer);
            vkWaitForFences(logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
        }
    }
    //
    // 6. Save in flight frame reference to tha map
    //
    manager->swapChainImageToInFlightFrameMap[manager->currentSwapChainImageIndex] = manager->currentImageInFlight;
}

void se_vk_in_flight_manager_register_pipeline(SeVkInFlightManager* manager, SeRenderObject* pipeline)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(manager->device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    //
    //
    //
    se_sbuffer_push(manager->registeredPipelines, pipeline);
    const size_t numImagesInFlight = se_sbuffer_size(manager->inFlightDatas);
    const size_t numSets = se_vk_render_pipeline_get_biggest_set_index(pipeline);
    for (size_t it = 0; it < numImagesInFlight; it++)
    {
        SeVkInFlightData* inFlightData = &manager->inFlightDatas[it];
        SeVkRenderPipelinePools pools = {0};
        se_sbuffer_construct(pools.setPools, numSets, allocator);
        for (size_t setIt = 0; setIt < numSets; setIt++)
        {
            se_sbuffer(SeVkDescriptorSetPool) poolsArray = NULL;
            se_sbuffer_construct(poolsArray, 4, allocator);
            SeVkDescriptorSetPool pool = {0};
            pool.handle = se_vk_render_pipeline_create_descriptor_pool(pipeline, setIt);
            pool.isLastAllocationSuccessful = true;
            se_sbuffer_push(poolsArray, pool);
            se_sbuffer_push(pools.setPools, poolsArray);
        }
        se_sbuffer_push(inFlightData->renderPipelinePools, pools);
    }
}

void se_vk_in_flight_manager_unregister_pipeline(SeVkInFlightManager* manager, SeRenderObject* pipeline)
{
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    // todo
    
}

void se_vk_in_flight_manager_submit_command_buffer(SeVkInFlightManager* manager, SeRenderObject* commandBuffer)
{
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    se_sbuffer_push(inFlightData->submittedCommandBuffers, commandBuffer);
}

SeRenderObject* se_vk_in_flight_manager_get_last_submitted_command_buffer(SeVkInFlightManager* manager)
{
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    const size_t numSubmittedCommandBuffers = se_sbuffer_size(inFlightData->submittedCommandBuffers);
    return numSubmittedCommandBuffers ? inFlightData->submittedCommandBuffers[numSubmittedCommandBuffers - 1] : NULL;
}

VkSemaphore se_vk_in_flight_manager_get_image_available_semaphore(SeVkInFlightManager* manager)
{
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    return inFlightData->imageAvailableSemaphore;
}

uint32_t se_vk_in_flight_manager_get_current_swap_chain_image_index(SeVkInFlightManager* manager)
{
    return manager->currentSwapChainImageIndex;
}

VkDescriptorSet se_vk_in_flight_manager_create_descriptor_set(SeVkInFlightManager* manager, SeRenderObject* pipeline, size_t set)
{
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    VkDescriptorSetLayout layout = se_vk_render_pipeline_get_descriptor_set_layout(pipeline, set);
    //
    // First find pipeline index in the array of registered pipelines
    //
    size_t pipelineIndex = 0;
    const size_t numRegisteredPipelines = se_sbuffer_size(manager->registeredPipelines);
    for (size_t it = 0; it < numRegisteredPipelines; it++)
    {
        if (manager->registeredPipelines[it] == pipeline)
        {
            pipelineIndex = it;
            break;
        }
    }
    //
    // Using pipeline index get the pipeline set pools (array of pools for a given set) from the current in flight data
    //
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    SeVkRenderPipelinePools* pipelinePools = &inFlightData->renderPipelinePools[pipelineIndex];
    se_sbuffer(SeVkDescriptorSetPool) setPools = pipelinePools->setPools[set];
    //
    // First try to allocate descriptor set from an existing pool
    //
    VkDescriptorSet result = VK_NULL_HANDLE;
    const size_t numSetPools = se_sbuffer_size(setPools);
    for (size_t it = 0; it < numSetPools; it++)
    {
        SeVkDescriptorSetPool* pool = &setPools[it];
        if (pool->isLastAllocationSuccessful)
        {
            VkDescriptorSetAllocateInfo allocateInfo = (VkDescriptorSetAllocateInfo)
            {
                .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .pNext              = NULL,
                .descriptorPool     = pool->handle,
                .descriptorSetCount = 1,
                .pSetLayouts        = &layout,
            };
            VkResult allocationResult = vkAllocateDescriptorSets(logicalHandle, &allocateInfo, &result);
            if (allocationResult == VK_SUCCESS)                         break;
            else if (allocationResult == VK_ERROR_OUT_OF_POOL_MEMORY)   { pool->isLastAllocationSuccessful = false; continue; }
            else                                                        se_assert(!"vkAllocateDescriptorSets failed");
        }
    }
    //
    // If allocation from exitsting pools failed, add new pool and allocate set from it
    //
    if (result == VK_NULL_HANDLE)
    {
        SeVkDescriptorSetPool pool = {0};
        pool.handle = se_vk_render_pipeline_create_descriptor_pool(pipeline, set);
        pool.isLastAllocationSuccessful = true;
        VkDescriptorSetAllocateInfo allocateInfo = (VkDescriptorSetAllocateInfo)
        {
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext              = NULL,
            .descriptorPool     = pool.handle,
            .descriptorSetCount = 1,
            .pSetLayouts        = &layout,
        };
        se_vk_check(vkAllocateDescriptorSets(logicalHandle, &allocateInfo, &result));
        se_sbuffer_push(setPools, pool);
    }
    se_assert(result != VK_NULL_HANDLE);
    return result;
}
