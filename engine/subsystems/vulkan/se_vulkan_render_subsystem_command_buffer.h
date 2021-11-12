#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject;

struct SeRenderObject* se_vk_command_buffer_request(struct SeCommandBufferRequestInfo* requestInfo);
void            se_vk_command_buffer_submit(struct SeRenderObject* buffer);
void            se_vk_command_bind_pipeline(struct SeRenderObject* buffer, struct SeCommandBindPipelineInfo* commandInfo);
void            se_vk_command_draw(struct SeRenderObject* buffer, struct SeCommandDrawInfo* commandInfo);

VkFence se_vk_command_buffer_get_fence(struct SeRenderObject* buffer);

#endif
