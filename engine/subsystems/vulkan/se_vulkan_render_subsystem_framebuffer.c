
#include "se_vulkan_render_subsystem_framebuffer.h"
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_render_pass.h"
#include "se_vulkan_render_subsystem_command_buffer.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "engine/allocator_bindings.h"

typedef enum SeVkFramebufferFlags
{
    SE_VK_FRAMEBUFFER_DEPENDS_ON_SWAP_CHAIN = 0x00000001,
} SeVkFramebufferFlags;

SeRenderObject* se_vk_framebuffer_create(SeFramebufferCreateInfo* createInfo)
{
    se_vk_expect_handle(createInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create framebuffer");
    se_vk_expect_handle(createInfo->renderPass, SE_RENDER_HANDLE_TYPE_PASS, "Can't create framebuffer");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    // Initial setup
    //
    SeVkFramebuffer* framebuffer = se_object_pool_take(SeVkFramebuffer, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER));
    framebuffer->object = se_vk_render_object(SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, se_vk_framebuffer_submit_for_deffered_destruction);
    framebuffer->createInfo = *createInfo;
    framebuffer->createInfo.attachmentsPtr = allocator->alloc(allocator->allocator, createInfo->numAttachments * sizeof(SeRenderObject*), se_default_alignment, se_alloc_tag);
    memcpy(framebuffer->createInfo.attachmentsPtr, createInfo->attachmentsPtr, createInfo->numAttachments * sizeof(SeRenderObject*));
    //
    //
    //
    se_vk_framebuffer_recreate_inplace((SeRenderObject*)framebuffer);
    //
    //
    //
    return (SeRenderObject*)framebuffer;
}

void se_vk_framebuffer_submit_for_deffered_destruction(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't submit framebuffer for deffered destruction");
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(((SeVkFramebuffer*)_framebuffer)->createInfo.device);
    se_vk_in_flight_manager_submit_deffered_destruction(inFlightManager, (SeVkDefferedDestruction) { _framebuffer, se_vk_framebuffer_destroy });
}

void se_vk_framebuffer_destroy(SeRenderObject* _framebuffer)
{
    se_vk_framebuffer_destroy_inplace(_framebuffer);
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(framebuffer->createInfo.device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    //
    //
    //
    allocator->dealloc(allocator->allocator, framebuffer->createInfo.attachmentsPtr, framebuffer->createInfo.numAttachments * sizeof(SeRenderObject*));
    se_object_pool_return(SeVkFramebuffer, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER), framebuffer);
}

void se_vk_framebuffer_destroy_inplace(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't destroy framebuffer");
    se_vk_check_external_resource_dependencies(_framebuffer);
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(framebuffer->createInfo.device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(framebuffer->createInfo.device);
    //
    // Destroy vk handles
    //
    vkDestroyFramebuffer(logicalHandle, framebuffer->handle, callbacks);
    //
    // Remove external dependencies
    //
    se_vk_remove_external_resource_dependency(framebuffer->createInfo.device);
    se_vk_remove_external_resource_dependency(framebuffer->createInfo.renderPass);
}

void se_vk_framebuffer_recreate_inplace(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't recreate framebuffer");
    SeVkFramebuffer* framebuffer = (SeVkFramebuffer*)_framebuffer;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(framebuffer->createInfo.device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(framebuffer->createInfo.device);
    //
    //
    //
    se_assert(framebuffer->createInfo.numAttachments <= SE_VK_FRAMEBUFFER_MAX_TEXTURES);
    se_vk_render_pass_validate_framebuffer_textures(framebuffer->createInfo.renderPass, framebuffer->createInfo.attachmentsPtr, framebuffer->createInfo.numAttachments);
    // @NOTE : currently we assuming, that all framebuffer attachments have the same extent
    const VkExtent3D extent = se_vk_texture_get_extent(framebuffer->createInfo.attachmentsPtr[0]);
    memset(__SE_VK_FRAMEBUFFER_RECREATE_ZEROING_OFFSET(framebuffer), 0, __SE_VK_FRAMEBUFFER_RECREATE_ZEROING_SIZE);
    framebuffer->numTextures = framebuffer->createInfo.numAttachments,
    framebuffer->extent = (VkExtent2D) { .width = extent.width, .height = extent.height };
    //
    // Temp data
    //
    VkImageView attachmentViews[SE_VK_FRAMEBUFFER_MAX_TEXTURES] = {0};
    for (size_t it = 0; it < framebuffer->createInfo.numAttachments; it++)
    {
        se_vk_expect_handle(framebuffer->createInfo.attachmentsPtr[it], SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't create framebuffer");
        SeRenderObject* texture = framebuffer->createInfo.attachmentsPtr[it];
        attachmentViews[it] = se_vk_texture_get_view(texture);
    }
    //
    // Create framebuffer
    //
    VkFramebufferCreateInfo framebufferCreateInfo = (VkFramebufferCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext              = NULL,
        .flags              = 0,
        .renderPass         = se_vk_render_pass_get_handle(framebuffer->createInfo.renderPass),
        .attachmentCount    = (uint32_t)framebuffer->createInfo.numAttachments, // @TODO : safe cast
        .pAttachments       = attachmentViews,
        .width              = framebuffer->extent.width,
        .height             = framebuffer->extent.height,
        .layers             = 1,
    };
    se_vk_check(vkCreateFramebuffer(logicalHandle, &framebufferCreateInfo, callbacks, &framebuffer->handle));
    for (size_t it = 0; it < framebuffer->createInfo.numAttachments; it++)
    {
        framebuffer->textures[it] = framebuffer->createInfo.attachmentsPtr[it];
    }
    //
    // Set flag if framebuffer depends on swap chain (must be rebuilt with the swap chain)
    //
    if (se_vk_render_pass_is_dependent_on_swap_chain(framebuffer->createInfo.renderPass))
    {
        framebuffer->flags |= SE_VK_FRAMEBUFFER_DEPENDS_ON_SWAP_CHAIN;
    }
    else
    {
        for (size_t it = 0; it < framebuffer->createInfo.numAttachments; it++)
        {
            if (se_vk_texture_is_dependent_on_swap_chain(framebuffer->textures[it]))
            {
                framebuffer->flags |= SE_VK_FRAMEBUFFER_DEPENDS_ON_SWAP_CHAIN;
                break;
            }
        }
    }
    //
    // Add resource dependencies
    //
    se_vk_add_external_resource_dependency(framebuffer->createInfo.device);
    se_vk_add_external_resource_dependency(framebuffer->createInfo.renderPass);
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
        const VkImageLayout initialLayout = se_vk_render_pass_get_initial_attachment_layout(framebuffer->createInfo.renderPass, it);
        if (currentLayout != initialLayout)
        {
            if (!commandBuffer)
            {
                SeCommandBufferRequestInfo requestInfo = (SeCommandBufferRequestInfo)
                {
                    framebuffer->createInfo.device,
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

bool se_vk_framebuffer_is_dependent_on_swap_chain(SeRenderObject* _framebuffer)
{
    se_vk_expect_handle(_framebuffer, SE_RENDER_HANDLE_TYPE_FRAMEBUFFER, "Can't get framebuffer extent");
    return ((SeVkFramebuffer*)_framebuffer)->flags & SE_VK_FRAMEBUFFER_DEPENDS_ON_SWAP_CHAIN;
}
