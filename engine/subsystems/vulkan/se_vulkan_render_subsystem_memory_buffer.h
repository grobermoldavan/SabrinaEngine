#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_BUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_BUFFER_H_

#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_memory.h"

typedef struct SeVkMemoryBuffer
{
    SeVkRenderObject object;
    SeRenderObject* device;
    VkBuffer handle;
    SeVkMemory memory;
} SeVkMemoryBuffer;

SeRenderObject* se_vk_memory_buffer_create(SeMemoryBufferCreateInfo* createInfo);
void            se_vk_memory_buffer_submit_for_deffered_destruction(SeRenderObject* buffer);
void            se_vk_memory_buffer_destroy(SeRenderObject* buffer);
void*           se_vk_memory_buffer_get_mapped_address(SeRenderObject* buffer);

VkBuffer        se_vk_memory_buffer_get_handle(SeRenderObject* buffer);

#endif
