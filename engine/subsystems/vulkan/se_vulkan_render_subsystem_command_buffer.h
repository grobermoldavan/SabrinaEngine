#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject;

struct SeRenderObject*  se_vk_command_buffer_request(struct SeCommandBufferRequestInfo* requestInfo);
void                    se_vk_command_buffer_submit(struct SeRenderObject* buffer);
void                    se_vk_command_buffer_submit_for_deffered_destruction(struct SeRenderObject* buffer);
void                    se_vk_command_buffer_destroy(struct SeRenderObject* buffer);
void                    se_vk_command_buffer_bind_pipeline(struct SeRenderObject* buffer, struct SeCommandBindPipelineInfo* commandInfo);
void                    se_vk_command_buffer_draw(struct SeRenderObject* buffer, struct SeCommandDrawInfo* commandInfo);
void                    se_vk_command_bind_resource_set(struct SeRenderObject* cmdBuffer, struct SeCommandBindResourceSetInfo* commandInfo);

VkFence     se_vk_command_buffer_get_fence(struct SeRenderObject* buffer);
VkSemaphore se_vk_command_buffer_get_semaphore(struct SeRenderObject* buffer);
void        se_vk_command_buffer_transition_image_layout(struct SeRenderObject* buffer, struct SeRenderObject* texture, VkImageLayout targetLayout);

#endif
