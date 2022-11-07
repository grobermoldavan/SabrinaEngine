
#include "se_vulkan_frame_manager.hpp"
#include "se_vulkan_device.hpp"
#include "se_vulkan_command_buffer.hpp"

void se_vk_frame_manager_construct(SeVkFrameManager* manager, const SeVkFrameManagerCreateInfo* createInfo)
{
    SeVkDevice* const device = createInfo->device;
    
    const size_t texelAlignment = device->gpu.deviceProperties_10.limits.minTexelBufferOffsetAlignment;
    const size_t uniformAlignment = device->gpu.deviceProperties_10.limits.minUniformBufferOffsetAlignment;
    const size_t storageAlignment = device->gpu.deviceProperties_10.limits.minStorageBufferOffsetAlignment;
    size_t scratchBufferAlignment = texelAlignment > uniformAlignment ? texelAlignment : uniformAlignment;
    scratchBufferAlignment = scratchBufferAlignment > storageAlignment ? scratchBufferAlignment : storageAlignment;

    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&device->memoryManager);
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    auto& memoryBufferPool = se_vk_memory_manager_get_pool<SeVkMemoryBuffer>(&device->memoryManager);

    *manager = 
    {
        .device                 = device,
        .frames                 = { },
        .imageToFrame           = { },
        .frameNumber            = 0,
        .scratchBufferAlignment = scratchBufferAlignment,
    };
    for (size_t it = 0; it < SeVkConfig::NUM_FRAMES_IN_FLIGHT; it++)
    {
        SeVkFrame* const frame = &manager->frames[it];
        *frame =
        {
            .imageAvailableSemaphore    = VK_NULL_HANDLE,
            .commandBuffers             = { },
            .scratchBuffer              = object_pool::take(memoryBufferPool),
            .scratchBufferViews         = { },
            .scratchBufferTop           = 0,
        };

        const VkSemaphoreCreateInfo semaphoreCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
        se_vk_check(vkCreateSemaphore(logicalHandle, &semaphoreCreateInfo, callbacks, &frame->imageAvailableSemaphore));
        dynamic_array::construct(frame->commandBuffers, allocators::persistent(), SeVkConfig::COMMAND_BUFFERS_ARRAY_INITIAL_CAPACITY);

        SeVkMemoryBufferInfo bufferInfo
        {
            .device     = device,
            .size       = se_megabytes(8),
            .usage      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT   | 
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT   ,
            .visibility = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        };
        se_vk_memory_buffer_construct(frame->scratchBuffer, &bufferInfo);
        dynamic_array::construct(frame->scratchBufferViews, allocators::persistent(), SeVkConfig::SCRATCH_BUFFERS_ARRAY_INITIAL_CAPACITY);
    }
    for (size_t it = 0; it < SeVkConfig::MAX_SWAP_CHAIN_IMAGES; it++)
    {
        manager->imageToFrame[it] = SE_VK_FRAME_MANAGER_INVALID_FRAME;
    }
}

void se_vk_frame_manager_destroy(SeVkFrameManager* manager)
{
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&manager->device->memoryManager);
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    for (size_t it = 0; it < SeVkConfig::NUM_FRAMES_IN_FLIGHT; it++)
    {
        SeVkFrame* const frame = &manager->frames[it];
        vkDestroySemaphore(logicalHandle, frame->imageAvailableSemaphore, callbacks);
        dynamic_array::destroy(frame->commandBuffers);
        dynamic_array::destroy(frame->scratchBufferViews);
    }
}

void se_vk_frame_manager_advance(SeVkFrameManager* manager)
{
    manager->frameNumber += 1;
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    SeVkFrame* const frame = se_vk_frame_manager_get_active_frame(manager);

    //
    // Wait and reset command buffers
    //
    if (SeVkCommandBuffer** const lastCommandBufferFromThisFrame = dynamic_array::last(frame->commandBuffers))
    {
        se_vk_check(vkWaitForFences(
            logicalHandle,
            1,
            &(*lastCommandBufferFromThisFrame)->fence,
            VK_TRUE,
            UINT64_MAX
        ));

        auto& commandBufferPool = se_vk_memory_manager_get_pool<SeVkCommandBuffer>(&manager->device->memoryManager);
        for (auto it : frame->commandBuffers)
        {
            SeVkCommandBuffer* const value = iter::value(it);
            se_vk_command_buffer_destroy(value);
            object_pool::release(commandBufferPool, value);
        }

        dynamic_array::reset(frame->commandBuffers);
    }

    //
    // Reset scratch buffer
    //
    dynamic_array::reset(frame->scratchBufferViews);
    frame->scratchBufferTop = 0;
}

SeVkCommandBuffer* se_vk_frame_manager_get_cmd(SeVkFrameManager* manager, SeVkCommandBufferInfo* info)
{
    auto& commandBufferPool = se_vk_memory_manager_get_pool<SeVkCommandBuffer>(&manager->device->memoryManager);
    SeVkCommandBuffer* const cmd = object_pool::take(commandBufferPool);
    dynamic_array::push(se_vk_frame_manager_get_active_frame(manager)->commandBuffers, cmd);
    se_vk_command_buffer_construct(cmd, info);
    return cmd;
}

uint32_t se_vk_frame_manager_alloc_scratch_buffer(SeVkFrameManager* manager, DataProvider data)
{
    se_assert(data_provider::is_valid(data));
    const auto [sourcePtr, sourceSize] = data_provider::get(data);
    
    SeVkFrame* const frame = se_vk_frame_manager_get_active_frame(manager);

    const size_t alignedBase = (frame->scratchBufferTop % manager->scratchBufferAlignment) == 0
        ? frame->scratchBufferTop
        : (frame->scratchBufferTop / manager->scratchBufferAlignment) * manager->scratchBufferAlignment + manager->scratchBufferAlignment;
    se_assert_msg((frame->scratchBuffer->memory.size - alignedBase) >= sourceSize, "Not enough scratch memory");

    frame->scratchBufferTop = alignedBase + sourceSize;
    dynamic_array::push(frame->scratchBufferViews,
    {
        .offset = alignedBase,
        .size   = sourceSize,
    });

    if (sourcePtr)
    {
        void* const base = (void*)(uintptr_t(frame->scratchBuffer->memory.mappedMemory) + alignedBase);
        memcpy(base, sourcePtr, sourceSize);
    }

    return dynamic_array::size<uint32_t>(frame->scratchBufferViews) - 1;
}