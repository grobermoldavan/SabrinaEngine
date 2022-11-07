#ifndef _SE_VULKAN_MEMORY_BUFFER_H_
#define _SE_VULKAN_MEMORY_BUFFER_H_

#include "se_vulkan_base.hpp"
#include "se_vulkan_memory.hpp"

struct SeVkMemoryBuffer
{
    SeVkObject          object;
    SeVkDevice*         device;
    VkBuffer            handle;
    SeVkMemory          memory;
};

struct SeVkMemoryBufferInfo
{
    SeVkDevice*             device;
    size_t                  size;
    VkBufferUsageFlags      usage;
    VkMemoryPropertyFlags   visibility;
};

void se_vk_memory_buffer_construct(SeVkMemoryBuffer* buffer, SeVkMemoryBufferInfo* info);
void se_vk_memory_buffer_destroy(SeVkMemoryBuffer* buffer);

template<>
void se_vk_destroy<SeVkMemoryBuffer>(SeVkMemoryBuffer* res)
{
    se_vk_memory_buffer_destroy(res);
}

#endif
