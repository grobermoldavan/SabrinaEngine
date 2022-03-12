#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_BUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_MEMORY_BUFFER_H_

#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_memory.h"

typedef struct SeVkMemoryBuffer
{
    SeVkObject          object;
    struct SeVkDevice*  device;
    VkBuffer            handle;
    SeVkMemory          memory;
} SeVkMemoryBuffer;

typedef struct SeVkMemoryBufferView
{
    SeVkMemoryBuffer* buffer;
    size_t offset;
    size_t size;
    void* mappedMemory;
} SeVkMemoryBufferView;

typedef struct SeVkMemoryBufferInfo
{
    struct SeVkDevice*      device;
    size_t                  size;
    VkBufferUsageFlags      usage;
    VkMemoryPropertyFlags   visibility;
} SeVkMemoryBufferInfo;

void se_vk_memory_buffer_construct(SeVkMemoryBuffer* buffer, SeVkMemoryBufferInfo* info);
void se_vk_memory_buffer_destroy(SeVkMemoryBuffer* buffer);

#endif
