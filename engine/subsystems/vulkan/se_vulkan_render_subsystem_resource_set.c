
#include "se_vulkan_render_subsystem_resource_set.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "se_vulkan_render_subsystem_memory_buffer.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "engine/allocator_bindings.h"
#include "engine/render_abstraction_interface.h"

typedef struct SeVkResourceSet
{
    SeRenderObject object;
    SeRenderObject* device;
    SeRenderObject* pipeline;
    VkDescriptorSet handle;
    uint32_t set;
    SeRenderObject* boundObjects[SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET];
    uint32_t numBindings;
} SeVkResourceSet;

SeRenderObject* se_vk_resource_set_create(SeResourceSetCreateInfo* createInfo)
{
    SeRenderObject* device = createInfo->device;
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(device);
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* persistentAllocator = memoryManager->cpu_persistentAllocator;
    SeAllocatorBindings* frameAllocator = memoryManager->cpu_frameAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    //
    // Base info
    //
    SeVkResourceSet* resourceSet = persistentAllocator->alloc(persistentAllocator->allocator, sizeof(SeVkResourceSet), se_default_alignment, se_alloc_tag);
    resourceSet->object.handleType = SE_RENDER_RESOURCE_SET;
    resourceSet->object.destroy = se_vk_resource_set_destroy;
    resourceSet->device = device;
    resourceSet->pipeline = createInfo->pipeline;
    resourceSet->handle = se_vk_in_flight_manager_create_descriptor_set(se_vk_device_get_in_flight_manager(device), createInfo->pipeline, createInfo->set);
    resourceSet->set = (uint32_t)createInfo->set; // @TODO : safe cast
    resourceSet->numBindings = (uint32_t)createInfo->numBindings; // @TODO : safe cast
    //
    // Write descriptor sets
    //
    se_assert(createInfo->numBindings == se_vk_render_pipeline_get_biggest_binding_index(createInfo->pipeline, createInfo->set));
    VkDescriptorImageInfo imageInfos[SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET] = {0};
    VkDescriptorBufferInfo bufferInfos[SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET] = {0};
    VkWriteDescriptorSet writes[SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET] = {0};
    size_t imageInfosIt = 0;
    size_t bufferInfosIt = 0;
    for (size_t it = 0; it < createInfo->numBindings; it++)
    {
        SeVkDescriptorSetBindingInfo bindingInfo = se_vk_render_pipeline_get_binding_info(createInfo->pipeline, createInfo->set, it);
        SeRenderObject* boundObject = createInfo->bindings[it];
        resourceSet->boundObjects[it] = boundObject;
        VkWriteDescriptorSet write = (VkWriteDescriptorSet)
        {
            .sType              = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext              = NULL,
            .dstSet             = resourceSet->handle,
            .dstBinding         = (uint32_t)it, // @TODO : safe cast
            .dstArrayElement    = 0,
            .descriptorCount    = 1,
            .descriptorType     = bindingInfo.descriptorType,
            .pImageInfo         = NULL,
            .pBufferInfo        = NULL,
            .pTexelBufferView   = NULL,
        };
        if (boundObject->handleType == SE_RENDER_MEMORY_BUFFER)
        {
            VkDescriptorBufferInfo* bufferInfo = &bufferInfos[bufferInfosIt++];
            *bufferInfo = (VkDescriptorBufferInfo)
            {
                .buffer = se_vk_memory_buffer_get_handle(boundObject),
                .offset = 0,
                .range  = VK_WHOLE_SIZE,
            };
            write.pBufferInfo = bufferInfo;
        }
        else if (boundObject->handleType == SE_RENDER_TEXTURE)
        {
            VkDescriptorImageInfo* imageInfo = &imageInfos[imageInfosIt++];
            *imageInfo = (VkDescriptorImageInfo)
            {
                .sampler        = se_vk_texture_get_default_sampler(boundObject),
                .imageView      = se_vk_texture_get_view(boundObject),
                .imageLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // ?????
            };
            write.pImageInfo = imageInfo;
        }
        else se_assert(!"Unsupported binding type");
        writes[it] = write;
    }
    vkUpdateDescriptorSets(logicalHandle, (uint32_t)createInfo->numBindings, writes, 0, NULL);
    return (SeRenderObject*)resourceSet;
}

void se_vk_resource_set_destroy(SeRenderObject* _set)
{
    SeVkResourceSet* set = (SeVkResourceSet*)_set;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(set->device);
    SeAllocatorBindings* persistentAllocator = memoryManager->cpu_persistentAllocator;
    //
    //
    //
    persistentAllocator->dealloc(persistentAllocator->allocator, set, sizeof(SeVkResourceSet));
    // @NOTE : VkDescriptorSet is handled by SeVkInFlightManager
}

SeRenderObject* se_vk_resource_set_get_pipeline(SeRenderObject* _set)
{
    return ((SeVkResourceSet*)_set)->pipeline;
}

VkDescriptorSet se_vk_resource_set_get_descriptor_set(SeRenderObject* _set)
{
    return ((SeVkResourceSet*)_set)->handle;
}

uint32_t se_vk_resource_set_get_set_index(SeRenderObject* _set)
{
    return ((SeVkResourceSet*)_set)->set;
}

void se_vk_resource_set_prepare(SeRenderObject* _set)
{
    SeVkResourceSet* set = (SeVkResourceSet*)_set;
    SeRenderObject* cmd = NULL;
    for (size_t it = 0; it < set->numBindings; it++)
    {
        SeRenderObject* obj = set->boundObjects[it];
        if (obj->handleType == SE_RENDER_TEXTURE && se_vk_texture_get_current_layout(obj) != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            if (!cmd) cmd = se_vk_command_buffer_request(&((SeCommandBufferRequestInfo) { .device = set->device, .usage = SE_USAGE_TRANSFER }));
            se_vk_command_buffer_transition_image_layout(cmd, obj, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }
    if (cmd) se_vk_command_buffer_submit(cmd);
}
