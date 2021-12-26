
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
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
    SeVkCommandQueueFlags queueFlags;
    VkCommandBuffer handle;
    VkSemaphore executionFinishedSemaphore;
    VkFence executionFence;
} SeVkCommandBuffer;

static void se_vk_command_buffer_assert_no_manual_destroy_call(SeRenderObject* _)
{
    se_assert_msg(false, "Command buffers must not be destroyed manually - their lifetime is handled by the rendering backed");
}

SeRenderObject* se_vk_command_buffer_request(SeCommandBufferRequestInfo* requestInfo)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(requestInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(requestInfo->device);
    //
    //
    //
    SeVkCommandBuffer* buffer = allocator->alloc(allocator->allocator, sizeof(SeVkCommandBuffer), se_default_alignment, se_alloc_tag);
    buffer->renderObject.handleType = SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER;
    buffer->renderObject.destroy = se_vk_command_buffer_assert_no_manual_destroy_call;
    buffer->device = requestInfo->device;
    buffer->usage = requestInfo->usage;
    buffer->flags = 0;
    buffer->queueFlags =
        requestInfo->usage == SE_COMMAND_BUFFER_USAGE_GRAPHICS ? SE_VK_CMD_QUEUE_GRAPHICS :
        requestInfo->usage == SE_COMMAND_BUFFER_USAGE_TRANSFER ? SE_VK_CMD_QUEUE_TRANSFER :
        0;
    //
    //
    //
    se_assert(requestInfo->usage == SE_COMMAND_BUFFER_USAGE_GRAPHICS || requestInfo->usage == SE_COMMAND_BUFFER_USAGE_TRANSFER);
    VkCommandBufferAllocateInfo allocateInfo = (VkCommandBufferAllocateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = NULL,
        .commandPool        = se_vk_device_get_command_pool(requestInfo->device, buffer->queueFlags),
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
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't submit command buffer");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(buffer->device);
    //
    //
    //
    se_assert(buffer->flags & SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS && "Can't submit command buffer - no commands were provided");
    if (buffer->flags & SE_VK_COMMAND_BUFFER_HAS_STARTED_RENDER_PASS)
    {
        vkCmdEndRenderPass(buffer->handle);
    }
    se_vk_check(vkEndCommandBuffer(buffer->handle));
    //
    //
    //
    SeVkCommandBuffer* waitBuffer = (SeVkCommandBuffer*)se_vk_in_flight_manager_get_last_command_buffer(inFlightManager);
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
    se_vk_check(vkQueueSubmit(se_vk_device_get_command_queue(buffer->device, buffer->queueFlags), 1, &submitInfo, buffer->executionFence));
    se_vk_in_flight_manager_register_command_buffer(inFlightManager, (SeRenderObject*)buffer);
}

void se_vk_command_buffer_destroy(SeRenderObject* _buffer)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't destroy command buffer");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(buffer->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(buffer->device);
    //
    //
    //
    vkDestroySemaphore(logicalHandle, buffer->executionFinishedSemaphore, callbacks);
    vkDestroyFence(logicalHandle, buffer->executionFence, callbacks);
    vkFreeCommandBuffers(logicalHandle, se_vk_device_get_command_pool(buffer->device, buffer->queueFlags), 1, &buffer->handle);
    allocator->dealloc(allocator->allocator, buffer, sizeof(SeVkCommandBuffer));
}

void se_vk_command_buffer_bind_pipeline(SeRenderObject* _buffer, SeCommandBindPipelineInfo* commandInfo)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't process bind pipleine command");
    se_vk_expect_handle(commandInfo->framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't process bind pipleine command");
    se_vk_expect_handle(commandInfo->pipeline, SE_RENDER_HANDLE_TYPE_PIPELINE, "Can't process bind pipleine command");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    se_vk_framebuffer_prepare(commandInfo->framebuffer);
    VkExtent2D framebufferExtent = se_vk_framebuffer_get_extent(commandInfo->framebuffer);
    VkRenderPassBeginInfo renderPassInfo = se_vk_render_pass_get_begin_info
    (
        se_vk_render_pipeline_get_render_pass(commandInfo->pipeline),
        commandInfo->framebuffer,
        (VkRect2D) { .offset = { 0, 0 }, .extent = framebufferExtent }
    );
    VkViewport viewport = (VkViewport)
    {
        .x          = 0.0f,
        .y          = 0.0f,
        .width      = (float)framebufferExtent.width,
        .height     = (float)framebufferExtent.height,
        .minDepth   = 0.0f,
        .maxDepth   = 1.0f,
    };
    VkRect2D scissor = (VkRect2D)
    {
        .offset = (VkOffset2D) { 0, 0 },
        .extent = framebufferExtent,
    };
    vkCmdSetViewport(buffer->handle, 0, 1, &viewport);
    vkCmdSetScissor(buffer->handle, 0, 1, &scissor);
    vkCmdBeginRenderPass(buffer->handle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(buffer->handle, se_vk_render_pipeline_get_bind_point(commandInfo->pipeline), se_vk_render_pipeline_get_handle(commandInfo->pipeline));
    buffer->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS | SE_VK_COMMAND_BUFFER_HAS_STARTED_RENDER_PASS;
}

void se_vk_command_buffer_draw(SeRenderObject* _buffer, SeCommandDrawInfo* commandInfo)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't process draw command");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    vkCmdDraw(buffer->handle, commandInfo->numVertices, commandInfo->numInstances, 0, 0);
    buffer->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}

void se_vk_command_bind_resource_set(SeRenderObject* _buffer, SeCommandBindResourceSetInfo* commandInfo)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't process bind resource set command");
    se_vk_expect_handle(commandInfo->resourceSet, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't process bind resource set command");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    SeRenderObject* pipeline = se_vk_resource_set_get_pipeline(commandInfo->resourceSet);
    VkDescriptorSet set = se_vk_resource_set_get_descriptor_set(commandInfo->resourceSet);
    se_vk_resource_set_prepare(commandInfo->resourceSet);
    vkCmdBindDescriptorSets
    (
        buffer->handle,
        se_vk_render_pipeline_get_bind_point(pipeline),
        se_vk_render_pipeline_get_layout(pipeline),
        se_vk_resource_set_get_set_index(commandInfo->resourceSet),
        1,
        &set,
        0,
        NULL
    );
    buffer->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}

VkFence se_vk_command_buffer_get_fence(SeRenderObject* _buffer)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't get buffer fence");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    return buffer->executionFence;
}

VkSemaphore se_vk_command_buffer_get_semaphore(SeRenderObject* _buffer)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't get buffer semaphore");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    return buffer->executionFinishedSemaphore;
}

void se_vk_command_buffer_transition_image_layout(SeRenderObject* _buffer, SeRenderObject* texture, VkImageLayout targetLayout)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't transition image layout");
    se_vk_expect_handle(texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't transition image layout");
    SeVkCommandBuffer* buffer = (SeVkCommandBuffer*)_buffer;
    se_vk_texture_transition_image_layout(texture, buffer->handle, targetLayout);
    buffer->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}
