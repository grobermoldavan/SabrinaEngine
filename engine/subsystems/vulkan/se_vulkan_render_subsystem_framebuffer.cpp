
#include "se_vulkan_render_subsystem_framebuffer.hpp"
#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_memory.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"
#include "se_vulkan_render_subsystem_render_pass.hpp"
#include "engine/allocator_bindings.hpp"

size_t g_framebufferIndex = 0;

void se_vk_framebuffer_construct(SeVkFramebuffer* framebuffer, SeVkFramebufferInfo* info)
{
    SeVkMemoryManager* memoryManager = &info->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    
    // @NOTE : currently we assuming, that all framebuffer attachments have the same extent
    // @TODO : support 3D textures
    SeVkTexture* tex = *info->textures[0];
    *framebuffer =
    {
        .object         = { SE_VK_TYPE_FRAMEBUFFER, g_framebufferIndex++ },
        .device         = info->device,
        .pass           = info->pass,
        .textures       = { },
        .numTextures    = info->numTextures,
        .handle         = VK_NULL_HANDLE,
        .extent         = { tex->extent.width, tex->extent.height },
    };

    VkImageView attachmentViews[SE_VK_FRAMEBUFFER_MAX_TEXTURES];
    for (uint32_t it = 0; it < info->numTextures; it++)
    {
        attachmentViews[it] = info->textures[it]->view;
        framebuffer->textures[it] = info->textures[it];
    }

    VkFramebufferCreateInfo framebufferCreateInfo
    {
        .sType              = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext              = nullptr,
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
