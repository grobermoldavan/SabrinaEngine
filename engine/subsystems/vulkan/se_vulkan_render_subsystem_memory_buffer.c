
#include "se_vulkan_render_subsystem_memory_buffer.h"
#include "se_vulkan_render_subsystem_device.h"

static size_t g_memoryBufferIndex = 0;

void se_vk_memory_buffer_construct(SeVkMemoryBuffer* buffer, SeVkMemoryBufferInfo* info)
{
    SeVkDevice* device = (SeVkDevice*)info->device;
    SeVkMemoryManager* memoryManager = &device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    //
    // Initial setup
    //
    *buffer = (SeVkMemoryBuffer)
    {
        .object = (SeVkObject){ SE_VK_TYPE_MEMORY_BUFFER, g_memoryBufferIndex++ },
        .device = device,
        .handle = VK_NULL_HANDLE,
        .memory = {0},
    };
    //
    // Create buffer handle
    //
    uint32_t queueFamilyIndices[SE_VK_MAX_UNIQUE_COMMAND_QUEUES];
    VkBufferCreateInfo bufferCreateInfo = (VkBufferCreateInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .size                   = info->size,
        .usage                  = info->usage,
        .queueFamilyIndexCount  = 0,
        .pQueueFamilyIndices    = queueFamilyIndices,
    };
    se_vk_device_fill_sharing_mode
    (
        device,
        SE_VK_CMD_QUEUE_GRAPHICS | SE_VK_CMD_QUEUE_TRANSFER | SE_VK_CMD_QUEUE_COMPUTE,
        &bufferCreateInfo.queueFamilyIndexCount,
        queueFamilyIndices,
        &bufferCreateInfo.sharingMode
    );
    se_vk_check(vkCreateBuffer(logicalHandle, &bufferCreateInfo, callbacks, &buffer->handle));
    //
    // Allocate memory
    //
    VkMemoryRequirements requirements = {0};
    vkGetBufferMemoryRequirements(logicalHandle, buffer->handle, &requirements);
    SeVkGpuAllocationRequest allocationRequest = (SeVkGpuAllocationRequest)
    {
        .size           = requirements.size,
        .alignment      = requirements.alignment,
        .memoryTypeBits = requirements.memoryTypeBits,
        .properties     = info->visibility,
    };
    buffer->memory = se_vk_memory_manager_allocate(memoryManager, allocationRequest);
    //
    // Bind memory
    //
    vkBindBufferMemory(logicalHandle, buffer->handle, buffer->memory.memory, buffer->memory.offset);
}

void se_vk_memory_buffer_destroy(SeVkMemoryBuffer* buffer)
{
    SeVkMemoryManager* memoryManager = &buffer->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(buffer->device);

    vkDestroyBuffer(logicalHandle, buffer->handle, callbacks);
    se_vk_memory_manager_deallocate(memoryManager, buffer->memory);
}
