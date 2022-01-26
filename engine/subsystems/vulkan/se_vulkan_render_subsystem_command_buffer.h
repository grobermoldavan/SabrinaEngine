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
void            se_vk_command_buffer_submit(SeRenderObject* cmd);
void            se_vk_command_buffer_destroy(SeRenderObject* cmd);
void            se_vk_command_buffer_bind_pipeline(SeRenderObject* cmd, SeCommandBindPipelineInfo* commandInfo);
void            se_vk_command_buffer_draw(SeRenderObject* cmd, SeCommandDrawInfo* commandInfo);
void            se_vk_command_buffer_dispatch(SeRenderObject* cmd, SeCommandDispatchInfo* commandInfo);
void            se_vk_command_bind_resource_set(SeRenderObject* cmdBuffer, SeCommandBindResourceSetInfo* commandInfo);

VkFence     se_vk_command_buffer_get_fence(SeRenderObject* cmd);
VkSemaphore se_vk_command_buffer_get_semaphore(SeRenderObject* cmd);
void        se_vk_command_buffer_transition_image_layout(SeRenderObject* cmd, SeRenderObject* texture, VkImageLayout targetLayout);
void        se_vk_command_buffer_copy_buffer(SeRenderObject* cmd, SeRenderObject* dst, SeRenderObject* src, size_t srcOffset, size_t dstOffset, size_t size);


#endif
