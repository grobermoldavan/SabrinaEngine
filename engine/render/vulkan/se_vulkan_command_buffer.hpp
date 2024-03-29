#ifndef _SE_VULKAN_COMMAND_BUFFER_H_
#define _SE_VULKAN_COMMAND_BUFFER_H_

#include "se_vulkan_base.hpp"

using SeVkCommandBufferUsageFlags = SeVkFlags;
enum SeVkCommandBufferUsage : SeVkCommandBufferUsageFlags
{
    SE_VK_COMMAND_BUFFER_USAGE_GRAPHICS = 0x00000001,
    SE_VK_COMMAND_BUFFER_USAGE_TRANSFER = 0x00000002,
    SE_VK_COMMAND_BUFFER_USAGE_COMPUTE  = 0x00000004,
};

struct SeVkCommandBuffer
{
    SeVkObject      object;
    SeVkDevice*     device;
    VkCommandPool   pool;
    VkQueue         queue;
    VkCommandBuffer handle;
    VkSemaphore     semaphore;
    VkFence         fence;
};

struct SeVkCommandBufferInfo
{
    SeVkDevice* device;
    SeVkCommandBufferUsageFlags usage;
};

struct SeVkCommandBufferSubmitInfo
{
    SeVkCommandBuffer* executeAfter[SeVkConfig::COMMAND_BUFFER_EXECUTE_AFTER_MAX];
    VkSemaphore waitSemaphores[SeVkConfig::COMMAND_BUFFER_WAIT_SEMAPHORES_MAX];
};

void se_vk_command_buffer_construct(SeVkCommandBuffer* buffer, SeVkCommandBufferInfo* info);
void se_vk_command_buffer_destroy(SeVkCommandBuffer* buffer);
void se_vk_command_buffer_submit(SeVkCommandBuffer* buffer, SeVkCommandBufferSubmitInfo* info);

template<>
void se_vk_destroy<SeVkCommandBuffer>(SeVkCommandBuffer* res)
{
    se_vk_command_buffer_destroy(res);
}

#endif
