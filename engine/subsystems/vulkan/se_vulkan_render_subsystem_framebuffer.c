
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "engine/render_abstraction_interface.h"
#include "engine/allocator_bindings.h"

#define SE_VK_FRAMEBUFFER_MAX_TEXTURES 32

typedef struct SeVkFramebuffer
{
    SeRenderObject object;
    SeRenderObject* device;
    SeRenderObject* renderPass;
    VkFramebuffer handle;
    SeRenderObject* textures[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    size_t numTextures;
} SeVkFramebuffer;

SeRenderObject* se_vk_framebuffer_create(SeFramebufferCreateInfo* createInfo)
{
    SeRenderObject* device = createInfo->device;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);
    //
    // Initial setup
    //
    se_vk_render_pass_validate_framebuffer_textures(createInfo->renderPass, createInfo->attachmentsPtr, createInfo->numAttachments);
    SeVkFramebuffer* framebuffer = allocator->alloc(allocator->allocator, sizeof(SeVkFramebuffer), se_default_alignment);
    framebuffer->object.handleType = SE_RENDER_FRAMEBUFFER;
    framebuffer->object.destroy = se_vk_framebuffer_destroy;
    framebuffer->device = createInfo->device;
    framebuffer->renderPass = createInfo->renderPass;
    framebuffer->numTextures = createInfo->numAttachments;
    //
    // Temp data
    //
    VkImageView attachmentViews[SE_VK_FRAMEBUFFER_MAX_TEXTURES] = {0};
    for (size_t it = 0; it < createInfo->numAttachments; it++)
    {
        SeRenderObject* texture = createInfo->attachmentsPtr[it];
        attachmentViews[it] = se_vk_texture_get_view(texture);
    }
    //
    // Create framebuffer
    //
    // @NOTE : currently we assuming, that all framebuffer attachments have the same extent
    VkExtent3D extent = se_vk_texture_get_extent(createInfo->attachmentsPtr[0]);
    VkFramebufferCreateInfo framebufferCreateInfo = (VkFramebufferCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext              = NULL,
        .flags              = 0,
        .renderPass         = se_vk_render_pass_get_handle(createInfo->renderPass),
        .attachmentCount    = (uint32_t)createInfo->numAttachments, // @TODO : safe cast
        .pAttachments       = attachmentViews,
        .width              = extent.width,
        .height             = extent.height,
        .layers             = 1,
    };
    se_vk_check(vkCreateFramebuffer(logicalHandle, &framebufferCreateInfo, callbacks, &framebuffer->handle));
    for (size_t it = 0; it < createInfo->numAttachments; it++)
    {
        framebuffer->textures[it] = createInfo->attachmentsPtr[it];
    }
    return (SeRenderObject*)framebuffer;
}

void se_vk_framebuffer_destroy(SeRenderObject* _framebuffer)
{
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    SeRenderObject* device = framebuffer->device;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(device);

    vkDestroyFramebuffer(logicalHandle, framebuffer->handle, callbacks);
    allocator->dealloc(allocator->allocator, framebuffer, sizeof(SeVkFramebuffer));
}

VkFramebuffer se_vk_framebuffer_get_handle(SeRenderObject* _framebuffer)
{
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    return framebuffer->handle;
}

void se_vk_framebuffer_prepare(SeRenderObject* _framebuffer)
{
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    SeRenderObject* commandBuffer = NULL;
    for (size_t it = 0; it < framebuffer->numTextures; it++)
    {
        SeRenderObject* texture = framebuffer->textures[it];
        const VkImageLayout currentLayout = se_vk_texture_get_current_layout(texture);
        const VkImageLayout initialLayout = se_vk_render_pass_get_initial_attachment_layout(framebuffer->renderPass, it);
        if (currentLayout != initialLayout)
        {
            if (!commandBuffer)
            {
                SeCommandBufferRequestInfo requestInfo = (SeCommandBufferRequestInfo)
                {
                    framebuffer->device,
                    SE_USAGE_TRANSFER
                };
                commandBuffer = se_vk_command_buffer_request(&requestInfo);
            }
            se_vk_command_buffer_transition_image_layout(commandBuffer, texture, initialLayout);
        }
    }
    if (commandBuffer)
    {
        se_vk_command_buffer_submit(commandBuffer);
    }
}
