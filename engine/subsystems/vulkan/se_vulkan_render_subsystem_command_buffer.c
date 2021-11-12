
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "engine/render_abstraction_interface.h"
#include "engine/allocator_bindings.h"

enum SeVkCommandBufferFlagBits
{
    SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS = 0x00000001,
    SE_VK_COMMAND_BUFFER_HAS_STARTED_RENDER_PASS = 0x00000002,
};
typedef uint32_t SeVkCommandBufferFlags;

typedef struct SeVkCommandBuffer
{
    SeRenderObject renderObject;
    SeRenderObject* device;
    SeCommandBufferUsage usage;
    SeVkCommandBufferFlags flags;
    VkCommandBuffer handle;
    VkSemaphore executionFinishedSemaphore;
    VkFence executionFence;
} SeVkCommandBuffer;

SeRenderObject* se_vk_command_buffer_request(SeCommandBufferRequestInfo* requestInfo)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(requestInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(requestInfo->device);
    
    SeVkCommandBuffer* buffer = allocator->alloc(allocator->allocator, sizeof(SeVkCommandBuffer), se_default_alignment);
    buffer->renderObject.handleType = SE_RENDER_COMMAND_BUFFER;
    buffer->renderObject.destroy = NULL; // ????
    buffer->device = requestInfo->device;
    buffer->usage = requestInfo->usage;
    buffer->flags = 0;

    se_assert(requestInfo->usage == SE_USAGE_GRAPHICS && requestInfo->usage == SE_USAGE_TRANSFER);
    SeVkCommandQueueFlags queueFlags =
        requestInfo->usage == SE_USAGE_GRAPHICS ? SE_VK_CMD_QUEUE_GRAPHICS :
        requestInfo->usage == SE_USAGE_TRANSFER ? SE_VK_CMD_QUEUE_TRANSFER :
        0;
    VkCommandBufferAllocateInfo allocateInfo = (VkCommandBufferAllocateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = NULL,
        .commandPool        = se_vk_device_get_command_pool(requestInfo->device, queueFlags),
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    se_vk_check(vkAllocateCommandBuffers(logicalHandle, &allocateInfo, &buffer->handle));
    VkSemaphoreCreateInfo semaphoreCreateInfo = (VkSemaphoreCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    se_vk_check(vkCreateSemaphore(logicalHandle, &semaphoreCreateInfo, callbacks, &buffer->executionFinishedSemaphore));
    VkFenceCreateInfo fenceCreateInfo = (VkFenceCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    se_vk_check(vkCreateFence(logicalHandle, &fenceCreateInfo, callbacks, &buffer->executionFence));
    VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo)
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    se_vk_check(vkBeginCommandBuffer(buffer->handle, &beginInfo));
    return (SeRenderObject*)buffer;
}

void se_vk_command_buffer_submit(SeRenderObject* _buffer)
{
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)(_buffer);

    se_assert(buffer->flags & SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS && "Can't submit command buffer - no commands were provided");
    if (buffer->flags & SE_VK_COMMAND_BUFFER_HAS_STARTED_RENDER_PASS)
    {
        vkCmdEndRenderPass(buffer->handle);
    }
    se_vk_check(vkEndCommandBuffer(buffer->handle));

    SeVkCommandBuffer* waitBuffer = (SeVkCommandBuffer*)se_vk_device_get_last_command_buffer(buffer->device);
    VkSemaphore waitSemaphores[] = { waitBuffer ? waitBuffer->executionFinishedSemaphore : VK_NULL_HANDLE };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT }; // TODO : optimize this
    VkSubmitInfo submitInfo = (VkSubmitInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext                  = NULL,
        .waitSemaphoreCount     = waitBuffer ? 1 : 0,
        .pWaitSemaphores        = waitSemaphores,
        .pWaitDstStageMask      = waitStages,
        .commandBufferCount     = 1,
        .pCommandBuffers        = &buffer->handle,
        .signalSemaphoreCount   = 1,
        .pSignalSemaphores      = &buffer->executionFinishedSemaphore,
    };

    SeVkCommandQueueFlags queueFlags =
        buffer->usage == SE_USAGE_GRAPHICS ? SE_VK_CMD_QUEUE_GRAPHICS :
        buffer->usage == SE_USAGE_TRANSFER ? SE_VK_CMD_QUEUE_TRANSFER :
        0;
    se_vk_device_submit_command_buffer(buffer->device, &submitInfo, (SeRenderObject*)buffer, se_vk_device_get_command_queue(buffer->device, queueFlags));
}

void se_vk_command_bind_pipeline(SeRenderObject* buffer, SeCommandBindPipelineInfo* commandInfo)
{

}

void se_vk_command_draw(SeRenderObject* buffer, SeCommandDrawInfo* commandInfo)
{

}

VkFence se_vk_command_buffer_get_fence(SeRenderObject* _buffer)
{
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)(_buffer);
    return buffer->executionFence;
}
