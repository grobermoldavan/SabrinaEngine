
#include "se_vulkan_render_subsystem_resource_set.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "se_vulkan_render_subsystem_render_pipeline.h"
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

static void se_vk_resource_set_assert_no_manual_destroy_call(SeRenderObject* _)
{
    se_assert_msg(false, "Resource sets must not be destroyed manually - their lifetime is handled by the rendering backed");
}

SeRenderObject* se_vk_resource_set_request(SeResourceSetRequestInfo* requestInfo)
{
    se_vk_expect_handle(requestInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create resource set");
    se_vk_expect_handle(requestInfo->pipeline, SE_RENDER_HANDLE_TYPE_PIPELINE, "Can't create resource set");
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(requestInfo->device);
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(requestInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* persistentAllocator = memoryManager->cpu_persistentAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(requestInfo->device);
    //
    // Base info
    //
    SeVkResourceSet* resourceSet = persistentAllocator->alloc(persistentAllocator->allocator, sizeof(SeVkResourceSet), se_default_alignment, se_alloc_tag);
    resourceSet->object.handleType = SE_RENDER_HANDLE_TYPE_RESOURCE_SET;
    resourceSet->object.destroy = se_vk_resource_set_assert_no_manual_destroy_call;
    resourceSet->device = requestInfo->device;
    resourceSet->pipeline = requestInfo->pipeline;
    resourceSet->handle = se_vk_in_flight_manager_create_descriptor_set(se_vk_device_get_in_flight_manager(requestInfo->device), requestInfo->pipeline, requestInfo->set);
    resourceSet->set = (uint32_t)requestInfo->set; // @TODO : safe cast
    resourceSet->numBindings = (uint32_t)requestInfo->numBindings; // @TODO : safe cast
    //
    // Write descriptor sets
    //
    se_assert(requestInfo->numBindings == se_vk_render_pipeline_get_biggest_binding_index(requestInfo->pipeline, requestInfo->set));
    VkDescriptorImageInfo imageInfos[SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET] = {0};
    VkDescriptorBufferInfo bufferInfos[SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET] = {0};
    VkWriteDescriptorSet writes[SE_VK_RENDER_PIPELINE_MAX_BINDINGS_IN_DESCRIPTOR_SET] = {0};
    size_t imageInfosIt = 0;
    size_t bufferInfosIt = 0;
    for (size_t it = 0; it < requestInfo->numBindings; it++)
    {
        SeVkDescriptorSetBindingInfo bindingInfo = se_vk_render_pipeline_get_binding_info(requestInfo->pipeline, requestInfo->set, it);
        SeRenderObject* boundObject = requestInfo->bindings[it];
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
        if (boundObject->handleType == SE_RENDER_HANDLE_TYPE_MEMORY_BUFFER)
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
        else if (boundObject->handleType == SE_RENDER_HANDLE_TYPE_TEXTURE)
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
    vkUpdateDescriptorSets(logicalHandle, (uint32_t)requestInfo->numBindings, writes, 0, NULL);
    se_vk_in_flight_manager_register_resource_set(inFlightManager, (SeRenderObject*)resourceSet);
    return (SeRenderObject*)resourceSet;
}

void se_vk_resource_set_destroy(SeRenderObject* _set)
{
    se_vk_expect_handle(_set, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't destroy resource set");
    SeVkResourceSet* set = (SeVkResourceSet*)_set;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(set->device);
    SeAllocatorBindings* persistentAllocator = memoryManager->cpu_persistentAllocator;
    persistentAllocator->dealloc(persistentAllocator->allocator, set, sizeof(SeVkResourceSet));
    // @NOTE : VkDescriptorSet is handled by SeVkInFlightManager
}

SeRenderObject* se_vk_resource_set_get_pipeline(SeRenderObject* _set)
{
    se_vk_expect_handle(_set, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't get resource set pipeline");
    return ((SeVkResourceSet*)_set)->pipeline;
}

VkDescriptorSet se_vk_resource_set_get_descriptor_set(SeRenderObject* _set)
{
    se_vk_expect_handle(_set, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't resource descriptor set");
    return ((SeVkResourceSet*)_set)->handle;
}

uint32_t se_vk_resource_set_get_set_index(SeRenderObject* _set)
{
    se_vk_expect_handle(_set, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't get resource set index");
    return ((SeVkResourceSet*)_set)->set;
}

void se_vk_resource_set_prepare(SeRenderObject* _set)
{
    se_vk_expect_handle(_set, SE_RENDER_HANDLE_TYPE_RESOURCE_SET, "Can't prepare resource set");
    SeVkResourceSet* set = (SeVkResourceSet*)_set;
    SeRenderObject* cmd = NULL;
    for (size_t it = 0; it < set->numBindings; it++)
    {
        SeRenderObject* obj = set->boundObjects[it];
        if (obj->handleType == SE_RENDER_HANDLE_TYPE_TEXTURE && se_vk_texture_get_current_layout(obj) != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            if (!cmd) cmd = se_vk_command_buffer_request(&((SeCommandBufferRequestInfo) { .device = set->device, .usage = SE_COMMAND_BUFFER_USAGE_TRANSFER }));
            se_vk_command_buffer_transition_image_layout(cmd, obj, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }
    if (cmd) se_vk_command_buffer_submit(cmd);
}
