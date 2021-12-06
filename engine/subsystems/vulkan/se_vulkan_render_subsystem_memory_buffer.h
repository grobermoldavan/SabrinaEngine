#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_BUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_BUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

struct SeRenderObject* se_vk_memory_buffer_create(struct SeMemoryBufferCreateInfo* createInfo);
void se_vk_memory_buffer_destroy(struct SeRenderObject* buffer);
void* se_vk_memory_buffer_get_mapped_address(struct SeRenderObject* buffer);

VkBuffer se_vk_memory_buffer_get_handle(struct SeRenderObject* buffer);

#endif
