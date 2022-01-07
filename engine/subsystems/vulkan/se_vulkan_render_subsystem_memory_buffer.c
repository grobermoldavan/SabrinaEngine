
#include "se_vulkan_render_subsystem_memory_buffer.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "engine/allocator_bindings.h"

SeRenderObject* se_vk_memory_buffer_create(SeMemoryBufferCreateInfo* createInfo)
{
    se_vk_expect_handle(createInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create memory buffer");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    // Initial setup
    //
    SeVkMemoryBuffer* buffer = se_object_pool_take(SeVkMemoryBuffer, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER));
    *buffer = (SeVkMemoryBuffer)
    {
        .object = se_vk_render_object(SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER, se_vk_memory_buffer_submit_for_deffered_destruction),
        .device = createInfo->device,
        .handle = VK_NULL_HANDLE,
        .memory = {0},
    };
    //
    // Create buffer handle
    //
    VkBufferUsageFlags bufferUsage = 0;
    if (createInfo->usage & SE_MEMORY_BUFFER_USAGE_TRANSFER_SRC) bufferUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (createInfo->usage & SE_MEMORY_BUFFER_USAGE_TRANSFER_DST) bufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (createInfo->usage & SE_MEMORY_BUFFER_USAGE_UNIFORM_BUFFER) bufferUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (createInfo->usage & SE_MEMORY_BUFFER_USAGE_STORAGE_BUFFER) bufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    uint32_t queueFamilyIndices[SE_VK_MAX_UNIQUE_COMMAND_QUEUES] = {0};
    VkSharingMode sharingMode = 0;
    uint32_t queueFamilyIndexCount = 0;
    {
        const uint32_t graphicsQueueFamilyIndex = se_vk_device_get_command_queue_family_index(createInfo->device, SE_VK_CMD_QUEUE_GRAPHICS);
        const uint32_t transferQueueFamilyIndex = se_vk_device_get_command_queue_family_index(createInfo->device, SE_VK_CMD_QUEUE_TRANSFER);
        if (graphicsQueueFamilyIndex == transferQueueFamilyIndex)
        {
            sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            queueFamilyIndices[0] = graphicsQueueFamilyIndex;
            queueFamilyIndexCount = 1;
        }
        else
        {
            sharingMode = VK_SHARING_MODE_CONCURRENT;
            queueFamilyIndices[0] = graphicsQueueFamilyIndex;
            queueFamilyIndices[1] = transferQueueFamilyIndex;
            queueFamilyIndexCount = 2;
        }
    }
    VkBufferCreateInfo bufferCreateInfo = (VkBufferCreateInfo)
    {
        .sType                  = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext                  = NULL,
        .flags                  = 0,
        .size                   = createInfo->size,
        .usage                  = bufferUsage,
        .sharingMode            = sharingMode,
        .queueFamilyIndexCount  = queueFamilyIndexCount,
        .pQueueFamilyIndices    = queueFamilyIndices,
    };
    se_vk_check(vkCreateBuffer(logicalHandle, &bufferCreateInfo, callbacks, &buffer->handle));
    //
    // Allocate memory
    //
    VkMemoryRequirements requirements = {0};
    vkGetBufferMemoryRequirements(logicalHandle, buffer->handle, &requirements);
    VkMemoryPropertyFlags properties = createInfo->visibility == SE_MEMORY_BUFFER_VISIBILITY_GPU ?
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT :
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    SeVkGpuAllocationRequest allocationRequest = (SeVkGpuAllocationRequest)
    {
        .sizeBytes      = requirements.size,
        .alignment      = requirements.alignment,
        .memoryTypeBits = requirements.memoryTypeBits,
        .properties     = properties,
    };
    buffer->memory = se_vk_gpu_allocate(memoryManager, allocationRequest);
    //
    // Bind memory
    //
    vkBindBufferMemory(logicalHandle, buffer->handle, buffer->memory.memory, buffer->memory.offsetBytes);
    //
    // Add external dependencies
    //
    se_vk_add_external_resource_dependency(buffer->device);
    //
    //
    //
    return (SeRenderObject*)buffer;
}

void se_vk_memory_buffer_submit_for_deffered_destruction(SeRenderObject* _buffer)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER, "Can't submit memory buffer for deffered destruction");
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(((SeVkMemoryBuffer*)_buffer)->device);
    se_vk_in_flight_manager_submit_deffered_destruction(inFlightManager, (SeVkDefferedDestruction) { _buffer, se_vk_memory_buffer_destroy });
}

void se_vk_memory_buffer_destroy(SeRenderObject* _buffer)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER, "Can't destroy memory buffer");
    se_vk_check_external_resource_dependencies(_buffer);
    SeVkMemoryBuffer* buffer = (SeVkMemoryBuffer*)_buffer;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(buffer->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(buffer->device);
    //
    // Destroy vk handles and free gpu memory
    //
    vkDestroyBuffer(logicalHandle, buffer->handle, callbacks);
    se_vk_gpu_deallocate(memoryManager, buffer->memory);
    //
    // Remove external dependencies
    //
    se_vk_remove_external_resource_dependency(buffer->device);
    //
    //
    //
    se_object_pool_return(SeVkMemoryBuffer, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER), buffer);
}

void* se_vk_memory_buffer_get_mapped_address(SeRenderObject* _buffer)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER, "Can't get mapped memory buffer address");
    return ((SeVkMemoryBuffer*)_buffer)->memory.mappedMemory;
}

VkBuffer se_vk_memory_buffer_get_handle(SeRenderObject* _buffer)
{
    se_vk_expect_handle(_buffer, SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER, "Can't get memory buffer handle");
    return ((SeVkMemoryBuffer*)_buffer)->handle;
}
