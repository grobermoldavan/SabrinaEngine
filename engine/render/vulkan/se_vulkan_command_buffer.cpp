
#include "se_vulkan_command_buffer.hpp"
#include "se_vulkan_device.hpp"
#include "se_vulkan_memory.hpp"

size_t g_commandBufferIndex = 0;

void se_vk_command_buffer_construct(SeVkCommandBuffer* buffer, SeVkCommandBufferInfo* info)
{
    SeVkMemoryManager* const memoryManager = &info->device->memoryManager;
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    
    se_assert(info->usage);
    const SeVkCommandQueueFlags queueFlags =
        (info->usage & SE_VK_COMMAND_BUFFER_USAGE_GRAPHICS ? SE_VK_CMD_QUEUE_GRAPHICS : 0) |
        (info->usage & SE_VK_COMMAND_BUFFER_USAGE_TRANSFER ? SE_VK_CMD_QUEUE_TRANSFER : 0) |
        (info->usage & SE_VK_COMMAND_BUFFER_USAGE_COMPUTE  ? SE_VK_CMD_QUEUE_COMPUTE  : 0) ;
    se_assert(queueFlags);
    *buffer =
    {
        .object         = { SeVkObject::Type::COMMAND_BUFFER, 0, g_commandBufferIndex++ },
        .device         = info->device,
        .pool           = se_vk_device_get_command_pool(info->device, queueFlags),
        .queue          = se_vk_device_get_command_queue(info->device, queueFlags),
        .handle         = VK_NULL_HANDLE,
        .semaphore      = VK_NULL_HANDLE,
        .fence          = VK_NULL_HANDLE,
    };
    //
    // @TODO :  handle unsupported queueFlags combinations somehow
    //          (if SE_VK_CMD_QUEUE_GRAPHICS | SE_VK_CMD_QUEUE_TRANSFER combination is unsupported by the users device)
    //          (split single command buffer into two/three/... command buffers?)
    //
    const VkCommandBufferAllocateInfo allocateInfo =
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = nullptr,
        .commandPool        = buffer->pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    se_vk_check(vkAllocateCommandBuffers(logicalHandle, &allocateInfo, &buffer->handle));
    const VkSemaphoreCreateInfo semaphoreCreateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    se_vk_check(vkCreateSemaphore(logicalHandle, &semaphoreCreateInfo, callbacks, &buffer->semaphore));
    const VkFenceCreateInfo fenceCreateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    se_vk_check(vkCreateFence(logicalHandle, &fenceCreateInfo, callbacks, &buffer->fence));
    const VkCommandBufferBeginInfo beginInfo =
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext              = nullptr,
        .flags              = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo   = nullptr,
    };
    se_vk_check(vkBeginCommandBuffer(buffer->handle, &beginInfo));
}

void se_vk_command_buffer_destroy(SeVkCommandBuffer* buffer)
{
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(&buffer->device->memoryManager);
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(buffer->device);
    vkDestroySemaphore(logicalHandle, buffer->semaphore, callbacks);
    vkDestroyFence(logicalHandle, buffer->fence, callbacks);
    vkFreeCommandBuffers(logicalHandle, buffer->pool, 1, &buffer->handle);
}

void se_vk_command_buffer_submit(SeVkCommandBuffer* buffer, SeVkCommandBufferSubmitInfo* info)
{
    se_vk_check(vkEndCommandBuffer(buffer->handle));
    uint32_t waitSemaphoreCount = 0;
    VkSemaphore waitSemaphores[SeVkConfig::COMMAND_BUFFER_EXECUTE_AFTER_MAX + SeVkConfig::COMMAND_BUFFER_WAIT_SEMAPHORES_MAX];
    VkPipelineStageFlags waitStages[SeVkConfig::COMMAND_BUFFER_EXECUTE_AFTER_MAX + SeVkConfig::COMMAND_BUFFER_WAIT_SEMAPHORES_MAX];
    for (size_t it = 0; it < SeVkConfig::COMMAND_BUFFER_EXECUTE_AFTER_MAX; it++)
    {
        if (!info->executeAfter[it]) break;
        waitSemaphores[waitSemaphoreCount] = info->executeAfter[it]->semaphore;
        waitStages[waitSemaphoreCount] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT }; // TODO : optimize this
        waitSemaphoreCount += 1;
    }
    for (size_t it = 0; it < SeVkConfig::COMMAND_BUFFER_WAIT_SEMAPHORES_MAX; it++)
    {
        if (!info->waitSemaphores[it]) break;
        waitSemaphores[waitSemaphoreCount] = info->waitSemaphores[it];
        waitStages[waitSemaphoreCount] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT }; // TODO : optimize this
        waitSemaphoreCount += 1;
    }
    const VkSubmitInfo submitInfo =
    {
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                  = nullptr,
        .waitSemaphoreCount     = waitSemaphoreCount,
        .pWaitSemaphores        = waitSemaphores,
        .pWaitDstStageMask      = waitStages,
        .commandBufferCount     = 1,
        .pCommandBuffers        = &buffer->handle,
        .signalSemaphoreCount   = 1,
        .pSignalSemaphores      = &buffer->semaphore,
    };
    se_vk_check(vkQueueSubmit(buffer->queue, 1, &submitInfo, buffer->fence));
}
