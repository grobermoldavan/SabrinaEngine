
#include "se_vulkan_render_subsystem_frame_manager.hpp"
#include "se_vulkan_render_subsystem_device.hpp"

void se_vk_frame_manager_construct(SeVkFrameManager* manager, SeVkFrameManagerCreateInfo* createInfo)
{
    se_assert(createInfo->numFrames <= SE_VK_FRAME_MANAGER_MAX_NUM_FRAMES);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&createInfo->device->memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    *manager =
    {
        .device         = createInfo->device,
        .frames         = {0},
        .numFrames      = createInfo->numFrames,
        .frameNumber    = 0,
    };
    for (size_t it = 0; it < manager->numFrames; it++)
    {
        SeVkFrame* frame = &manager->frames[it];
        *frame =
        {
            .scratchBuffer              = object_pool::take(se_vk_memory_manager_get_pool<SeVkMemoryBuffer>(&createInfo->device->memoryManager)),
            .scratchBufferTop           = 0,
            .imageAvailableSemaphore    = VK_NULL_HANDLE,
            .lastBuffer                 = nullptr,
        };
        SeVkMemoryBufferInfo bufferInfo
        {
            .device     = manager->device,
            .size       = se_megabytes(16),
            .usage      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT   | 
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT   ,
            .visibility = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        };
        se_vk_memory_buffer_construct(frame->scratchBuffer, &bufferInfo);
        const VkSemaphoreCreateInfo semaphoreCreateInfo
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
        se_vk_check(vkCreateSemaphore(logicalHandle, &semaphoreCreateInfo, callbacks, &frame->imageAvailableSemaphore));
    }
    for (size_t it = 0; it < SE_VK_FRAME_MANAGER_MAX_NUM_FRAMES; it++)
    {
        manager->imageToFrame[it] = SE_VK_FRAME_MANAGER_INVALID_FRAME;
    }
}

void se_vk_frame_manager_destroy(SeVkFrameManager* manager)
{
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(&manager->device->memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(manager->device);
    for (size_t it = 0; it < manager->numFrames; it++)
    {
        SeVkFrame* frame = &manager->frames[it];
        vkDestroySemaphore(logicalHandle, frame->imageAvailableSemaphore, callbacks);
    }
}

void se_vk_frame_manager_advance(SeVkFrameManager* manager)
{
    manager->frameNumber += 1;
    const size_t activeFrameIndex = se_vk_frame_manager_get_active_frame_index(manager);
    SeVkFrame* activeFrame = se_vk_frame_manager_get_active_frame(manager);
    activeFrame->scratchBufferTop = 0;
}

SeVkMemoryBufferView se_vk_frame_manager_alloc_scratch_buffer(SeVkFrameManager* manager, size_t size)
{
    SeVkFrame* frame = se_vk_frame_manager_get_active_frame(manager);
    se_assert_msg((frame->scratchBuffer->memory.size - frame->scratchBufferTop) >= size, "todo : expand frame memory");
    SeVkMemoryBufferView view
    {
        .buffer         = frame->scratchBuffer,
        .offset         = frame->scratchBufferTop,
        .size           = size,
        .mappedMemory   = frame->scratchBuffer->memory.mappedMemory ? ((char*)frame->scratchBuffer->memory.mappedMemory) + frame->scratchBufferTop : nullptr,
    };
    frame->scratchBufferTop += size;
    return view;
}
