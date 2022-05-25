
#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "engine/allocator_bindings.hpp"

static size_t g_textureIndex = 0;

void se_vk_texture_construct(SeVkTexture* texture, SeVkTextureInfo* info)
{
    SeVkMemoryManager* memoryManager = &info->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    {
        const bool isDepthFormat = se_vk_utils_is_depth_format(info->format);
        const bool isStencilFormat = se_vk_utils_is_stencil_format(info->format);
        const VkImageAspectFlags aspect =
            (isDepthFormat                          ? VK_IMAGE_ASPECT_DEPTH_BIT     : 0) |
            (isStencilFormat                        ? VK_IMAGE_ASPECT_STENCIL_BIT   : 0) |
            (!(isDepthFormat || isStencilFormat)    ? VK_IMAGE_ASPECT_COLOR_BIT     : 0) ;
        *texture =
        {
            .object                 = { SE_VK_TYPE_TEXTURE, g_textureIndex++ },
            .device                 = info->device,
            .extent                 = info->extent,
            .format                 = info->format,
            .currentLayout          = VK_IMAGE_LAYOUT_UNDEFINED,
            .image                  = VK_NULL_HANDLE,
            .memory                 = { },
            .view                   = VK_NULL_HANDLE,
            .fullSubresourceRange   = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS },
            .flags                  = 0,
        };
    }
    {
        uint32_t queueFamilyIndices[SE_VK_MAX_UNIQUE_COMMAND_QUEUES];
        VkImageCreateInfo imageCreateInfo =
        {
            .sType                  = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .imageType              = texture->extent.depth > 1 ? VK_IMAGE_TYPE_3D : (texture->extent.height > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D),
            .format                 = texture->format,
            .extent                 = texture->extent,
            .mipLevels              = 1,
            .arrayLayers            = 1,
            .samples                = info->sampling,
            .tiling                 = VK_IMAGE_TILING_OPTIMAL,
            .usage                  = info->usage,
            .sharingMode            = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount  = 0,
            .pQueueFamilyIndices    = queueFamilyIndices,
            .initialLayout          = texture->currentLayout,
        };
        se_vk_device_fill_sharing_mode
        (
            info->device,
            SE_VK_CMD_QUEUE_GRAPHICS | SE_VK_CMD_QUEUE_TRANSFER | SE_VK_CMD_QUEUE_COMPUTE,
            &imageCreateInfo.queueFamilyIndexCount,
            queueFamilyIndices,
            &imageCreateInfo.sharingMode
        );
        se_vk_check(vkCreateImage(logicalHandle, &imageCreateInfo, callbacks, &texture->image));
    }
    {
        VkMemoryRequirements memRequirements = { };
        vkGetImageMemoryRequirements(logicalHandle, texture->image, &memRequirements);
        SeVkGpuAllocationRequest request =
        {
            .size               = memRequirements.size,
            .alignment          = memRequirements.alignment,
            .memoryTypeBits     = memRequirements.memoryTypeBits,
            .properties         = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };
        texture->memory = se_vk_memory_manager_allocate(memoryManager, request);
        vkBindImageMemory(logicalHandle, texture->image, texture->memory.memory, texture->memory.offset);
    }
    {
        VkImageViewCreateInfo viewCreateInfo =
        {
            .sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext              = nullptr,
            .flags              = 0,
            .image              = texture->image,
            .viewType           = texture->extent.depth > 1 ? VK_IMAGE_VIEW_TYPE_3D : (texture->extent.height > 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_1D),
            .format             = texture->format,
            .components         =
            {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange   = texture->fullSubresourceRange,
        };
        se_vk_check(vkCreateImageView(logicalHandle, &viewCreateInfo, callbacks, &texture->view));
    }
}

void se_vk_texture_construct_from_swap_chain(SeVkTexture* texture, SeVkDevice* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format)
{
    *texture =
    {
        .object                 = { SE_VK_TYPE_TEXTURE, g_textureIndex++ },
        .device                 = device,
        .extent                 = { extent->width, extent->height, 1 },
        .format                 = format,
        .currentLayout          = VK_IMAGE_LAYOUT_UNDEFINED,
        .image                  = image,
        .memory                 = {0},
        .view                   = view,
        .fullSubresourceRange   = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS },
        .flags                  = SE_VK_TEXTURE_FROM_SWAP_CHAIN,
    };
}

void se_vk_texture_destroy(SeVkTexture* texture)
{
    SeVkMemoryManager* memoryManager = &texture->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(texture->device);
    if (!(texture->flags & SE_VK_TEXTURE_FROM_SWAP_CHAIN))
    {
        vkDestroyImageView(logicalHandle, texture->view, callbacks);
        vkDestroyImage(logicalHandle, texture->image, callbacks);
        se_vk_memory_manager_deallocate(memoryManager, texture->memory);
    }
}
