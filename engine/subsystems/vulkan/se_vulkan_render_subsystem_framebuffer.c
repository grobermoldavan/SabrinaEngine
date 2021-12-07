
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
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
    VkExtent2D extent;
} SeVkFramebuffer;

SeRenderObject* se_vk_framebuffer_create(SeFramebufferCreateInfo* createInfo)
{
    se_vk_expect_handle(createInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create framebuffer");
    se_vk_expect_handle(createInfo->renderPass, SE_RENDER_HANDLE_TYPE_PASS, "Can't create framebuffer");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    // Initial setup
    //
    se_vk_render_pass_validate_framebuffer_textures(createInfo->renderPass, createInfo->attachmentsPtr, createInfo->numAttachments);
    SeVkFramebuffer* framebuffer = allocator->alloc(allocator->allocator, sizeof(SeVkFramebuffer), se_default_alignment, se_alloc_tag);
    framebuffer->object.handleType = SE_RENDER_HANDLE_TYPE_FRAMEBUFFER;
    framebuffer->object.destroy = se_vk_framebuffer_submit_for_deffered_destruction;
    framebuffer->device = createInfo->device;
    framebuffer->renderPass = createInfo->renderPass;
    framebuffer->numTextures = createInfo->numAttachments;
    //
    // Temp data
    //
    VkImageView attachmentViews[SE_VK_FRAMEBUFFER_MAX_TEXTURES] = {0};
    for (size_t it = 0; it < createInfo->numAttachments; it++)
    {
        se_vk_expect_handle(createInfo->attachmentsPtr[it], SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't create framebuffer");
        SeRenderObject* texture = createInfo->attachmentsPtr[it];
        attachmentViews[it] = se_vk_texture_get_view(texture);
    }
    //
    // Create framebuffer
    //
    // @NOTE : currently we assuming, that all framebuffer attachments have the same extent
    VkExtent3D extent = se_vk_texture_get_extent(createInfo->attachmentsPtr[0]);
    framebuffer->extent = (VkExtent2D) { .width = extent.width, .height = extent.height };
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

void se_vk_framebuffer_submit_for_deffered_destruction(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't submit framebuffer for deffered destruction");
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(((SeVkCommandBuffer*)_framebuffer)->device);
    se_vk_in_flight_manager_submit_deffered_destruction(inFlightManager, (SeVkDefferedDestruction) { _framebuffer, se_vk_framebuffer_destroy });
}

void se_vk_framebuffer_destroy(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't destroy framebuffer");
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(framebuffer->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkDevice logicalHandle = se_vk_device_get_logical_handle(framebuffer->device);
    //
    //
    //
    vkDestroyFramebuffer(logicalHandle, framebuffer->handle, callbacks);
    allocator->dealloc(allocator->allocator, framebuffer, sizeof(SeVkFramebuffer));
}

VkFramebuffer se_vk_framebuffer_get_handle(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't get framebuffer handle");
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    return framebuffer->handle;
}

void se_vk_framebuffer_prepare(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't prepare framebuffer");
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
                    SE_COMMAND_BUFFER_USAGE_TRANSFER
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

VkExtent2D se_vk_framebuffer_get_extent(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't get framebuffer extent");
    return ((SeVkFramebuffer*)_framebuffer)->extent;
}
