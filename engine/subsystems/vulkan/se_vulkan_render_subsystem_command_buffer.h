#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

typedef struct SeVkCommandBuffer
{
    SeVkRenderObject        object;
    SeRenderObject*         device;
    VkCommandBuffer         handle;
    SeCommandBufferUsage    usage;
    SeVkCommandQueueFlags   queueFlags;
    VkSemaphore             executionFinishedSemaphore;
    VkFence                 executionFence;
    uint64_t                flags;
} SeVkCommandBuffer;

SeRenderObject* se_vk_command_buffer_request(SeCommandBufferRequestInfo* requestInfo);
void            se_vk_command_buffer_submit(SeRenderObject* buffer);
void            se_vk_command_buffer_destroy(SeRenderObject* buffer);
void            se_vk_command_buffer_bind_pipeline(SeRenderObject* buffer, SeCommandBindPipelineInfo* commandInfo);
void            se_vk_command_buffer_draw(SeRenderObject* buffer, SeCommandDrawInfo* commandInfo);
void            se_vk_command_bind_resource_set(SeRenderObject* cmdBuffer, SeCommandBindResourceSetInfo* commandInfo);

VkFence     se_vk_command_buffer_get_fence(SeRenderObject* buffer);
VkSemaphore se_vk_command_buffer_get_semaphore(SeRenderObject* buffer);
void        se_vk_command_buffer_transition_image_layout(SeRenderObject* buffer, SeRenderObject* texture, VkImageLayout targetLayout);

#endif
