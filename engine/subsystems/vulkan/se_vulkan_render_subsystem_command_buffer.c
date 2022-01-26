
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_memory_buffer.h"
#include "engine/allocator_bindings.h"

enum SeVkCommandBufferFlagBits
{
    SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS     = 0x00000001,
    SE_VK_COMMAND_BUFFER_HAS_STARTED_RENDER_PASS    = 0x00000002,
    SE_VK_COMMAND_BUFFER_IS_SUBMITTED               = 0x00000004,
};

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
    // Allocate and set initial info
    //
    SeVkCommandBuffer* cmd = se_object_pool_take(SeVkCommandBuffer, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER));
    *cmd = (SeVkCommandBuffer)
    {
        .object                     = se_vk_render_object(SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, se_vk_command_buffer_assert_no_manual_destroy_call),
        .device                     = requestInfo->device,
        .handle                     = VK_NULL_HANDLE,
        .usage                      = requestInfo->usage,
        .queueFlags                 =
            requestInfo->usage == SE_COMMAND_BUFFER_USAGE_GRAPHICS ? SE_VK_CMD_QUEUE_GRAPHICS :
            requestInfo->usage == SE_COMMAND_BUFFER_USAGE_TRANSFER ? SE_VK_CMD_QUEUE_TRANSFER :
            requestInfo->usage == SE_COMMAND_BUFFER_USAGE_COMPUTE ? SE_VK_CMD_QUEUE_COMPUTE :
            0,
        .executionFinishedSemaphore = VK_NULL_HANDLE,
        .executionFence             = VK_NULL_HANDLE,
        .flags                      = 0,
    };
    se_assert_msg(cmd->queueFlags, "Unsupported command buffer usage");
    //
    // Create vk handles and begin command buffer
    //
    VkCommandBufferAllocateInfo allocateInfo = (VkCommandBufferAllocateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext              = NULL,
        .commandPool        = se_vk_device_get_command_pool(requestInfo->device, cmd->queueFlags),
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    se_vk_check(vkAllocateCommandBuffers(logicalHandle, &allocateInfo, &cmd->handle));
    VkSemaphoreCreateInfo semaphoreCreateInfo = (VkSemaphoreCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    se_vk_check(vkCreateSemaphore(logicalHandle, &semaphoreCreateInfo, callbacks, &cmd->executionFinishedSemaphore));
    VkFenceCreateInfo fenceCreateInfo = (VkFenceCreateInfo)
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
    };
    se_vk_check(vkCreateFence(logicalHandle, &fenceCreateInfo, callbacks, &cmd->executionFence));
    VkCommandBufferBeginInfo beginInfo = (VkCommandBufferBeginInfo)
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = NULL,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = NULL,
    };
    se_vk_check(vkBeginCommandBuffer(cmd->handle, &beginInfo));
    //
    // Add external dependencies
    //
    se_vk_add_external_resource_dependency(cmd->device);
    //
    //
    //
    return (SeRenderObject*)cmd;
}

void se_vk_command_buffer_submit(SeRenderObject* _cmd)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't submit command buffer");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(cmd->device);
    //
    //  End command buffer
    //
    se_assert_msg(!(cmd->flags & SE_VK_COMMAND_BUFFER_IS_SUBMITTED), "Can't submit same command buffer twise");
    se_assert_msg(cmd->flags & SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS, "Can't submit command buffer - no commands were provided");
    if (cmd->flags & SE_VK_COMMAND_BUFFER_HAS_STARTED_RENDER_PASS)
    {
        vkCmdEndRenderPass(cmd->handle);
    }
    se_vk_check(vkEndCommandBuffer(cmd->handle));
    //
    // Submit
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
        .pCommandBuffers        = &cmd->handle,
        .signalSemaphoreCount   = 1,
        .pSignalSemaphores      = &cmd->executionFinishedSemaphore,
    };
    se_vk_check(vkQueueSubmit(se_vk_device_get_command_queue(cmd->device, cmd->queueFlags), 1, &submitInfo, cmd->executionFence));
    se_vk_in_flight_manager_register_command_buffer(inFlightManager, (SeRenderObject*)cmd);
    cmd->flags |= SE_VK_COMMAND_BUFFER_IS_SUBMITTED;
}

void se_vk_command_buffer_destroy(SeRenderObject* _cmd)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't destroy command buffer");
    se_vk_check_external_resource_dependencies(_cmd);
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(se_vk_device_get_memory_manager(cmd->device));
    VkDevice logicalHandle = se_vk_device_get_logical_handle(cmd->device);
    //
    // Destroy vk handles
    //
    vkDestroySemaphore(logicalHandle, cmd->executionFinishedSemaphore, callbacks);
    vkDestroyFence(logicalHandle, cmd->executionFence, callbacks);
    vkFreeCommandBuffers(logicalHandle, se_vk_device_get_command_pool(cmd->device, cmd->queueFlags), 1, &cmd->handle);
    //
    // Remove external dependencies
    //
    se_vk_remove_external_resource_dependency(cmd->device);
    // @NOTE : Object memory is handled by SeVkInFlightManager and SeVkMemoryManager
}

void se_vk_command_buffer_bind_pipeline(SeRenderObject* _cmd, SeCommandBindPipelineInfo* commandInfo)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't process bind pipleine command");
    se_vk_expect_handle(commandInfo->pipeline, SE_RENDER_HANDLE_TYPE_PIPELINE, "Can't process bind pipleine command");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    if (commandInfo->framebuffer)
    {
        se_assert(se_vk_render_pipeline_get_bind_point(commandInfo->pipeline) == VK_PIPELINE_BIND_POINT_GRAPHICS);
        se_vk_expect_handle(commandInfo->framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't process bind pipleine command");
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
        vkCmdSetViewport(cmd->handle, 0, 1, &viewport);
        vkCmdSetScissor(cmd->handle, 0, 1, &scissor);
        vkCmdBeginRenderPass(cmd->handle, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmd->handle, se_vk_render_pipeline_get_bind_point(commandInfo->pipeline), se_vk_render_pipeline_get_handle(commandInfo->pipeline));
        cmd->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS | SE_VK_COMMAND_BUFFER_HAS_STARTED_RENDER_PASS;
    }
    else
    {
        se_assert(se_vk_render_pipeline_get_bind_point(commandInfo->pipeline) == VK_PIPELINE_BIND_POINT_COMPUTE);
        vkCmdBindPipeline(cmd->handle, se_vk_render_pipeline_get_bind_point(commandInfo->pipeline), se_vk_render_pipeline_get_handle(commandInfo->pipeline));
        cmd->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
    }
}

void se_vk_command_buffer_draw(SeRenderObject* _cmd, SeCommandDrawInfo* commandInfo)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't process draw command");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    vkCmdDraw(cmd->handle, commandInfo->numVertices, commandInfo->numInstances, 0, 0);
    cmd->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}

void se_vk_command_buffer_dispatch(SeRenderObject* _cmd, SeCommandDispatchInfo* commandInfo)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't process dispatch command");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    vkCmdDispatch(cmd->handle, commandInfo->groupCountX, commandInfo->groupCountY, commandInfo->groupCountZ);
    cmd->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}

void se_vk_command_bind_resource_set(SeRenderObject* _cmd, SeCommandBindResourceSetInfo* commandInfo)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't process bind resource set command");
    se_vk_expect_handle(commandInfo->resourceSet, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't process bind resource set command");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    SeRenderObject* pipeline = se_vk_resource_set_get_pipeline(commandInfo->resourceSet);
    VkDescriptorSet set = se_vk_resource_set_get_descriptor_set(commandInfo->resourceSet);
    se_vk_resource_set_prepare(commandInfo->resourceSet);
    vkCmdBindDescriptorSets
    (
        cmd->handle,
        se_vk_render_pipeline_get_bind_point(pipeline),
        se_vk_render_pipeline_get_layout(pipeline),
        se_vk_resource_set_get_set_index(commandInfo->resourceSet),
        1,
        &set,
        0,
        NULL
    );
    cmd->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}

VkFence se_vk_command_buffer_get_fence(SeRenderObject* _cmd)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't get command buffer fence");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    return cmd->executionFence;
}

VkSemaphore se_vk_command_buffer_get_semaphore(SeRenderObject* _cmd)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't get command buffer semaphore");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    return cmd->executionFinishedSemaphore;
}

void se_vk_command_buffer_transition_image_layout(SeRenderObject* _cmd, SeRenderObject* _texture, VkImageLayout targetLayout)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't transition image layout");
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't transition image layout");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    SeVkTexture* texture = (SeVkTexture*)_texture;
    VkImageMemoryBarrier imageBarrier = (VkImageMemoryBarrier)
    {
        .sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext                  = NULL,
        .srcAccessMask          = se_vk_utils_image_layout_to_access_flags(texture->currentLayout),
        .dstAccessMask          = se_vk_utils_image_layout_to_access_flags(targetLayout),
        .oldLayout              = texture->currentLayout,
        .newLayout              = targetLayout,
        .srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
        .image                  = texture->image,
        .subresourceRange       = texture->fullSubresourceRange,
    };
    vkCmdPipelineBarrier
    (
        cmd->handle,
        se_vk_utils_image_layout_to_pipeline_stage_flags(texture->currentLayout),
        se_vk_utils_image_layout_to_pipeline_stage_flags(targetLayout),
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &imageBarrier
    );
    texture->currentLayout = targetLayout;
    cmd->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}

void se_vk_command_buffer_copy_buffer(SeRenderObject* _cmd, SeRenderObject* _dst, SeRenderObject* _src, size_t srcOffset, size_t dstOffset, size_t size)
{
    se_vk_expect_handle(_cmd, SE_RENDER_HANDLE_TYPE_COMMAND_BUFFER, "Can't copy buffer");
    se_vk_expect_handle(_dst, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER, "Can't copy buffer");
    se_vk_expect_handle(_src, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER, "Can't copy buffer");
    SeVkCommandBuffer* cmd = (SeVkCommandBuffer*)_cmd;
    SeVkMemoryBuffer* dst = (SeVkMemoryBuffer*)_dst;
    SeVkMemoryBuffer* src = (SeVkMemoryBuffer*)_src;
    VkBufferCopy copy = (VkBufferCopy)
    {
        .srcOffset = srcOffset,
        .dstOffset = dstOffset,
        .size = size,
    };
    vkCmdCopyBuffer(cmd->handle, src->handle, dst->handle, 1, &copy);
    cmd->flags |= SE_VK_COMMAND_BUFFER_HAS_SUBMITTED_COMMANDS;
}
