
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "engine/allocator_bindings.h"

static size_t g_framebufferIndex = 0;

void se_vk_framebuffer_construct(SeVkFramebuffer* framebuffer, SeVkFramebufferInfo* info)
{
    SeVkMemoryManager* memoryManager = &info->device->memoryManager;
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    
    // @NOTE : currently we assuming, that all framebuffer attachments have the same extent
    // @TODO : support 3D textures
    *framebuffer = (SeVkFramebuffer)
    {
        .object         = (SeVkObject){ SE_VK_TYPE_FRAMEBUFFER, g_framebufferIndex++ },
        .device         = info->device,
        .pass           = info->pass,
        .textures       = {0},
        .numTextures    = info->numTextures,
        .handle         = VK_NULL_HANDLE,
        .extent         = { info->textures[0]->extent.width, info->textures[0]->extent.height },
    };

    VkImageView attachmentViews[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    for (uint32_t it = 0; it < info->numTextures; it++)
    {
        attachmentViews[it] = info->textures[it]->view;
        framebuffer->textures[it] = info->textures[it];
    }

    VkFramebufferCreateInfo framebufferCreateInfo = (VkFramebufferCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext              = NULL,
        .flags              = 0,
        .renderPass         = info->pass->handle,
        .attachmentCount    = info->numTextures,
        .pAttachments       = attachmentViews,
        .width              = framebuffer->extent.width,
        .height             = framebuffer->extent.height,
        .layers             = 1,
    };
    se_vk_check(vkCreateFramebuffer(logicalHandle, &framebufferCreateInfo, callbacks, &framebuffer->handle));
}

void se_vk_framebuffer_destroy(SeVkFramebuffer* framebuffer)
{
    SeVkMemoryManager* memoryManager = &framebuffer->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(framebuffer->device);
    vkDestroyFramebuffer(logicalHandle, framebuffer->handle, callbacks);
}

void se_vk_framebuffer_prepare(SeVkFramebuffer* framebuffer)
{
    se_assert(!"todo");
    // se_vk_expect_handle(_framebuffer, SE_VK_TYPE_FRAMEBUFFER, "Can't prepare framebuffer");
    // SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    // SeRenderObject* commandBuffer = NULL;
    // for (size_t it = 0; it < framebuffer->numTextures; it++)
    // {
    //     SeRenderObject* texture = framebuffer->textures[it];
    //     const VkImageLayout currentLayout = se_vk_texture_get_current_layout(texture);
    //     const VkImageLayout initialLayout = se_vk_render_pass_get_initial_attachment_layout(framebuffer->info.renderPass, it);
    //     if (currentLayout != initialLayout)
    //     {
    //         if (!commandBuffer)
    //         {
    //             SeCommandBufferRequestInfo requestInfo = (SeCommandBufferRequestInfo)
    //             {
    //                 framebuffer->info.device,
    //                 SE_COMMAND_BUFFER_USAGE_TRANSFER
    //             };
    //             commandBuffer = se_vk_command_buffer_request(&requestInfo);
    //         }
    //         se_vk_command_buffer_transition_image_layout(commandBuffer, texture, initialLayout);
    //     }
    // }
    // if (commandBuffer)
    // {
    //     se_vk_command_buffer_submit(commandBuffer);
    // }
}
