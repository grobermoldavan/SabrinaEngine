#ifndef _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_
#define _SE_VULKAN_RENDER_SUBSYSTEM_COMMAND_BUFFER_H_

#include "se_vulkan_render_subsystem_base.h"

#define SE_VK_COMMAND_BUFFER_EXECUTE_AFTER_MAX 16

typedef enum SeVkCommandBufferUsage
{
    SE_VK_COMMAND_BUFFER_USAGE_GRAPHICS = 0x00000001,
    SE_VK_COMMAND_BUFFER_USAGE_TRANSFER = 0x00000002,
    SE_VK_COMMAND_BUFFER_USAGE_COMPUTE  = 0x00000004,
} SeVkCommandBufferUsage;
typedef SeVkFlags SeVkCommandBufferUsageFlags;

typedef struct SeVkCommandBuffer
{
    SeVkObject          object;
    struct SeVkDevice*  device;
    VkCommandPool       pool;
    VkQueue             queue;
    VkCommandBuffer     handle;
    VkSemaphore         semaphore;
    VkFence             fence;
} SeVkCommandBuffer;

typedef struct SeVkCommandBufferInfo
{
    struct SeVkDevice* device;
    SeVkCommandBufferUsageFlags usage;
} SeVkCommandBufferInfo;

typedef struct SeVkCommandBufferSubmitInfo
{
    SeVkCommandBuffer* executeAfter[SE_VK_COMMAND_BUFFER_EXECUTE_AFTER_MAX];
} SeVkCommandBufferSubmitInfo;

void se_vk_command_buffer_construct(SeVkCommandBuffer* buffer, SeVkCommandBufferInfo* info);
void se_vk_command_buffer_destroy(SeVkCommandBuffer* buffer);
void se_vk_command_buffer_submit(SeVkCommandBuffer* buffer, SeVkCommandBufferSubmitInfo* info);

#endif
