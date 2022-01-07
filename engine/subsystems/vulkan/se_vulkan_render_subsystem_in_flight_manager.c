
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_render_pipeline.h"
#include "engine/allocator_bindings.h"

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
        *data = (SeVkInFlightData)
        {
            .defferedDestructions       = NULL,
            .renderPipelinePools        = NULL,
            .lastResourceSet            = NULL,
            .lastCommandBuffer          = NULL,
            .imageAvailableSemaphore    = VK_NULL_HANDLE,
        };
        se_vk_check(vkCreateSemaphore(logicalHandle, &semaphoreCreateInfo, callbacks, &data->imageAvailableSemaphore));
        se_sbuffer_construct(data->defferedDestructions, 64, allocator);
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
    // Destroy command buffers and process deffered destructions in first pass.
    //
    const size_t numInFlightDatas = se_sbuffer_size(manager->inFlightDatas);
    for (size_t it = 0; it < numInFlightDatas; it++)
    {
        SeVkInFlightData* inFlightData = &manager->inFlightDatas[it];
        vkDestroySemaphore(logicalHandle, inFlightData->imageAvailableSemaphore, callbacks);
        //
        // Destroy command buffers and resource sets
        //
        SeObjectPool* commandBufferPool = se_vk_memory_manager_get_pool_manually(memoryManager, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, it);
        se_object_pool_for(commandBufferPool, buf, se_vk_command_buffer_destroy((SeRenderObject*)buf));
        SeObjectPool* resourceSetPool = se_vk_memory_manager_get_pool_manually(memoryManager, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, it);
        se_object_pool_for(resourceSetPool, set, se_vk_resource_set_destroy((SeRenderObject*)set));
    }
    for (size_t it = 0; it < numInFlightDatas; it++)
    {
        SeVkInFlightData* inFlightData = &manager->inFlightDatas[it];
        //
        // Process deffered destructions
        //
        // @NOTE : at this point all registered pipelines will be unregistered from this manager using
        //         deffered destruction. If number of registered pipelines won't be zero,
        //         something went terribly wrong.
        const size_t numDefferedDestructions = se_sbuffer_size(inFlightData->defferedDestructions);
        for (size_t destIt = 0; destIt < numDefferedDestructions; destIt++)
        {
            inFlightData->defferedDestructions[destIt].destroy(inFlightData->defferedDestructions[destIt].object);
        }
    }
    //
    // Actually destroy in flight data containers in second pass.
    // Two passes are needed because deffered destructions from first pass can access these containers
    // (if we destroying render pipelines), so memory must be valid.
    //
    for (size_t it = 0; it < numInFlightDatas; it++)
    {
        SeVkInFlightData* inFlightData = &manager->inFlightDatas[it];
        se_sbuffer_destroy(inFlightData->defferedDestructions);
        se_sbuffer_destroy(inFlightData->renderPipelinePools);
    }
    se_assert_msg(se_sbuffer_size(manager->registeredPipelines) == 0, "All render pipelines must be submitted to destruction before destroying in flight manager");
    se_sbuffer_destroy(manager->inFlightDatas);
    se_sbuffer_destroy(manager->registeredPipelines);
    se_sbuffer_destroy(manager->swapChainImageToInFlightFrameMap);
}

void se_vk_in_flight_manager_advance_frame(SeVkInFlightManager* manager)
{
    /*
        1. Advance in flight index
        2.  Wait until previous command buffers of this frame finish execution, then delete previous command buffers and resource sets
        3. Process deffered destructions
        4. Reset all resource set pools
        5. Acquire next image and set the image semaphore to wait for
        6. If this image is already used by another in flight frame, wait for it's execution fence too
        7. Save in flight frame reference to the map
    */
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(manager->device);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    //
    // 1. Advance in flight index
    //
    manager->currentImageInFlight = (manager->currentImageInFlight + 1) % se_sbuffer_size(manager->inFlightDatas);
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    //
    // 2. Wait until previous command buffers of this frame finish execution, then delete previous command buffers and resource sets
    //
    if (inFlightData->lastCommandBuffer)
    {
        SeObjectPool* commandBufferPool = se_vk_memory_manager_get_pool_manually(memoryManager, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, manager->currentImageInFlight);
        VkFence fence = se_vk_command_buffer_get_fence(inFlightData->lastCommandBuffer);
        vkWaitForFences(logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
        se_object_pool_for(commandBufferPool, buf, se_vk_command_buffer_destroy((SeRenderObject*)buf));
        se_object_pool_reset(commandBufferPool);
        inFlightData->lastCommandBuffer = NULL;
    }
    {
        SeObjectPool* resourceSetPool = se_vk_memory_manager_get_pool_manually(memoryManager, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, manager->currentImageInFlight);
        se_object_pool_for(resourceSetPool, set, se_vk_resource_set_destroy((SeRenderObject*)set));
        se_object_pool_reset(resourceSetPool);
    }
    //
    // 3. Process deffered destructions
    //
    se_sbuffer(SeVkDefferedDestruction) defferedDestructions = inFlightData->defferedDestructions;
    const size_t numDefferedDestructions = se_sbuffer_size(defferedDestructions);
    for (size_t destIt = 0; destIt < numDefferedDestructions; destIt++)
    {
        defferedDestructions[destIt].destroy(defferedDestructions[destIt].object);
    }
    se_sbuffer_set_size(defferedDestructions, 0);
    //
    // 4. Reset all resource set pools
    //
    const size_t numRegisteredPipelines = se_sbuffer_size(inFlightData->renderPipelinePools);
    for (size_t pipelineIt = 0; pipelineIt < numRegisteredPipelines; pipelineIt++)
    {
        SeVkRenderPipelinePools* pools = &inFlightData->renderPipelinePools[pipelineIt];
        const size_t numSets = se_sbuffer_size(pools->setPools);
        for (size_t setIt = 0; setIt < numSets; setIt++)
        {
            se_sbuffer(SeVkDescriptorSetPool) setPools = pools->setPools[setIt];
            const size_t numPools = se_sbuffer_size(setPools);
            for (size_t poolIt = 0; poolIt < numPools; poolIt++)
            {
                vkResetDescriptorPool(logicalHandle, setPools[poolIt].handle, 0);
                setPools[poolIt].numAllocations = 0;
                setPools[poolIt].isLastAllocationSuccessful = true;
            }
        }
    }
    //
    // 5. Acquire next image and set the image semaphore to wait for
    //
    se_vk_check(vkAcquireNextImageKHR(logicalHandle, se_vk_device_get_swap_chain_handle(manager->device), UINT64_MAX, inFlightData->imageAvailableSemaphore, VK_NULL_HANDLE, &manager->currentSwapChainImageIndex));
    //
    // 6. If this image is already used by another in flight frame, wait for it's execution fence too
    //
    const uint32_t inFlightImageReferencedBySwapChainImage = manager->swapChainImageToInFlightFrameMap[manager->currentSwapChainImageIndex];
    if (inFlightImageReferencedBySwapChainImage != SE_VK_UNUSED_IN_FLIGHT_DATA_REF)
    {
        SeRenderObject* lastCommandBuffer = manager->inFlightDatas[inFlightImageReferencedBySwapChainImage].lastCommandBuffer;
        if (lastCommandBuffer)
        {
            VkFence fence = se_vk_command_buffer_get_fence(lastCommandBuffer);
            vkWaitForFences(logicalHandle, 1, &fence, VK_TRUE, UINT64_MAX);
        }
    }
    //
    // 7. Save in flight frame reference to tha map
    //
    manager->swapChainImageToInFlightFrameMap[manager->currentSwapChainImageIndex] = manager->currentImageInFlight;
}

void se_vk_in_flight_manager_register_pipeline(SeVkInFlightManager* manager, SeRenderObject* pipeline)
{
    se_vk_expect_handle(pipeline, SE_RENDER_HANDLE_TYPE_PIPELINE, "Can't register pipeline");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(manager->device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    //
    // Push pipeline to registeredPipelines array
    //
    se_sbuffer_push(manager->registeredPipelines, pipeline);
    //
    // Allocate first pools for every descriptor set of the pipeline
    //
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
            SeVkDescriptorSetPool pool =
            {
                .handle                     = se_vk_render_pipeline_create_descriptor_pool(pipeline, setIt),
                .numAllocations             = 0,
                .isLastAllocationSuccessful = true,
            };
            se_sbuffer_push(poolsArray, pool);
            se_sbuffer_push(pools.setPools, poolsArray);
        }
        se_sbuffer_push(inFlightData->renderPipelinePools, pools);
    }
}

void se_vk_in_flight_manager_unregister_pipeline(SeVkInFlightManager* manager, SeRenderObject* pipeline)
{
    se_vk_expect_handle(pipeline, SE_RENDER_HANDLE_TYPE_PIPELINE, "Can't unregister pipeline");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(manager->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    //
    // Find pipeline index
    //
    const size_t numRegisteredPipelines = se_sbuffer_size(manager->registeredPipelines);
    size_t pipelineIndex = numRegisteredPipelines;
    for (size_t it = 0; it < numRegisteredPipelines; it++)
    {
        if (manager->registeredPipelines[it] == pipeline)
        {
            pipelineIndex = it;
            break;
        }
    }
    se_assert(pipelineIndex != numRegisteredPipelines && "Trying to unregister already unregistered pipeline");
    //
    // Remove pipeline from registeredPipelines array and destroy all pipeline pools
    //
    se_sbuffer_remove_idx(manager->registeredPipelines, pipelineIndex);
    const size_t numImagesInFlight = se_sbuffer_size(manager->inFlightDatas);
    for (size_t it = 0; it < numImagesInFlight; it++)
    {
        SeVkInFlightData* inFlightData = &manager->inFlightDatas[it];
        SeVkRenderPipelinePools* pools = &inFlightData->renderPipelinePools[pipelineIndex];
        const size_t numSets = se_sbuffer_size(pools->setPools);
        for (size_t setIt = 0; setIt < numSets; setIt++)
        {
            se_sbuffer(SeVkDescriptorSetPool) setPools = pools->setPools[setIt];
            const size_t numPools = se_sbuffer_size(setPools);
            for (size_t poolIt = 0; poolIt < numPools; poolIt++)
            {
                vkDestroyDescriptorPool(logicalHandle, setPools[poolIt].handle, callbacks);
            }
            se_sbuffer_destroy(setPools);
        }
        se_sbuffer_destroy(pools->setPools);
        se_sbuffer_remove_idx(inFlightData->renderPipelinePools, pipelineIndex);
    }
}

void se_vk_in_flight_manager_register_resource_set(SeVkInFlightManager* manager, SeRenderObject* resourceSet)
{
    se_vk_expect_handle(resourceSet, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't register resource set");
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    inFlightData->lastResourceSet = resourceSet;
}

void se_vk_in_flight_manager_register_command_buffer(SeVkInFlightManager* manager, SeRenderObject* commandBuffer)
{
    se_vk_expect_handle(commandBuffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't submit command buffer");
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    inFlightData->lastCommandBuffer = commandBuffer;
}

void se_vk_in_flight_manager_submit_deffered_destruction(SeVkInFlightManager* manager, SeVkDefferedDestruction destruction)
{
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    se_sbuffer_push(inFlightData->defferedDestructions, destruction);
}

SeRenderObject* se_vk_in_flight_manager_get_last_command_buffer(SeVkInFlightManager* manager)
{
    SeVkInFlightData* inFlightData = &manager->inFlightDatas[manager->currentImageInFlight];
    return inFlightData->lastCommandBuffer;
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
    se_vk_expect_handle(pipeline, SE_RENDER_HANDLE_TYPE_PIPELINE, "Can't create descriptor set");
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    VkDescriptorSetLayout layout = se_vk_render_pipeline_get_descriptor_set_layout(pipeline, set);
    //
    // First find pipeline index in the array of registered pipelines
    //
    const size_t numRegisteredPipelines = se_sbuffer_size(manager->registeredPipelines);
    size_t pipelineIndex = numRegisteredPipelines;
    for (size_t it = 0; it < numRegisteredPipelines; it++)
    {
        if (manager->registeredPipelines[it] == pipeline)
        {
            pipelineIndex = it;
            break;
        }
    }
    se_assert(pipelineIndex != numRegisteredPipelines && "Trying to create descriptor set for an unregistered pipeline");
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
        se_assert(result != VK_NULL_HANDLE);
    }
    return result;
}

void se_vk_in_flight_manager_handle_swap_chain_recreate(SeVkInFlightManager* manager)
{
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(manager->device);
    //
    // We must destroy resource sets on swap chain recreation because they could be referencing swap chain dependent textures
    //
    const size_t numInFlightDatas = se_sbuffer_size(manager->inFlightDatas);
    for (size_t it = 0; it < numInFlightDatas; it++)
    {
        SeVkInFlightData* inFlightData = &manager->inFlightDatas[it];
        SeObjectPool* resourceSetPool = se_vk_memory_manager_get_pool_manually(memoryManager, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, it);
        se_object_pool_for(resourceSetPool, set, se_vk_resource_set_destroy((SeRenderObject*)set));
        se_object_pool_reset(resourceSetPool);
        //
        // Reset all resource set pools (to destroy vk descriptor sets)
        //
        const size_t numRegisteredPipelines = se_sbuffer_size(inFlightData->renderPipelinePools);
        for (size_t pipelineIt = 0; pipelineIt < numRegisteredPipelines; pipelineIt++)
        {
            SeVkRenderPipelinePools* pools = &inFlightData->renderPipelinePools[pipelineIt];
            const size_t numSets = se_sbuffer_size(pools->setPools);
            for (size_t setIt = 0; setIt < numSets; setIt++)
            {
                se_sbuffer(SeVkDescriptorSetPool) setPools = pools->setPools[setIt];
                const size_t numPools = se_sbuffer_size(setPools);
                for (size_t poolIt = 0; poolIt < numPools; poolIt++)
                {
                    vkResetDescriptorPool(logicalHandle, setPools[poolIt].handle, 0);
                    setPools[poolIt].numAllocations = 0;
                    setPools[poolIt].isLastAllocationSuccessful = true;
                }
            }
        }
    }
}
