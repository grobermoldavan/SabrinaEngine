
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "se_vulkan_render_subsystem_in_flight_manager.h"
#include "engine/allocator_bindings.h"

SeRenderObject* se_vk_texture_create(SeTextureCreateInfo* createInfo)
{
    se_vk_expect_handle(createInfo->device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create texture");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    //
    //
    SeVkTexture* texture = se_object_pool_take(SeVkTexture, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_TEXTURE));
    memset(texture, 0, sizeof(SeVkTexture));
    texture->object = se_vk_render_object(SE_RENDER_HANDLE_TYPE_TEXTURE, se_vk_texture_submit_for_deffered_destruction);
    texture->createInfo = *createInfo;
    //
    //
    //
    se_vk_texture_recreate_inplace((SeRenderObject*)texture);
    //
    //
    //
    return (SeRenderObject*)texture;
}

SeRenderObject* se_vk_texture_create_from_swap_chain(SeRenderObject* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format)
{
    se_vk_expect_handle(device, SE_RENDER_HANDLE_TYPE_DEVICE, "Can't create texture from external resources");
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    //
    //
    //
    SeVkTexture* texture = se_object_pool_take(SeVkTexture, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_TEXTURE));
    memset(texture, 0, sizeof(SeVkTexture));
    texture->object = se_vk_render_object(SE_RENDER_HANDLE_TYPE_TEXTURE, se_vk_texture_submit_for_deffered_destruction);
    texture->createInfo.device = device;
    //
    //
    //
    se_vk_texture_recreate_inplace_from_swap_chain((SeRenderObject*)texture, extent, image, view, format);
    //
    //
    //
    return (SeRenderObject*)texture;
}

void se_vk_texture_recreate_inplace(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't recreate texture");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    //
    //
    //
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(texture->createInfo.device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(texture->createInfo.device);
    memset(__SE_VK_TEXTURE_RECREATE_ZEROING_OFFSET(texture), 0, __SE_VK_TEXTURE_RECREATE_ZEROING_SIZE);
    //
    // Set extent and format
    //
    {
        const VkExtent2D swapChainExtent = se_vk_device_get_swap_chain_extent(texture->createInfo.device);
        const VkImageAspectFlags aspect = texture->createInfo.format == SE_TEXTURE_FORMAT_DEPTH_STENCIL
            ? (se_vk_device_is_stencil_supported(texture->createInfo.device) ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT)
            : VK_IMAGE_ASPECT_COLOR_BIT;
        const uint64_t flags =
            (texture->createInfo.width.type == SE_SIZE_PARAMETER_DYNAMIC ? SE_VK_TEXTURE_DEPENDS_ON_SWAP_CHAIN_EXTENT : 0) |
            (texture->createInfo.height.type == SE_SIZE_PARAMETER_DYNAMIC ? SE_VK_TEXTURE_DEPENDS_ON_SWAP_CHAIN_EXTENT : 0) |
            (texture->createInfo.format == SE_TEXTURE_FORMAT_SWAP_CHAIN ? SE_VK_TEXTURE_DEPENDS_ON_SWAP_CHAIN_FROMAT : 0) |
            (texture->createInfo.usage & SE_TEXTURE_USAGE_TEXTURE ? SE_VK_TEXTURE_HAS_SAMPLER : 0);
        texture->extent = (VkExtent3D)
        {
            .width = se_size_parameter_get(&texture->createInfo.width, swapChainExtent.width, swapChainExtent.height),
            .height = se_size_parameter_get(&texture->createInfo.height, swapChainExtent.width, swapChainExtent.height),
            .depth = 1,
        };
        texture->format =
            texture->createInfo.format == SE_TEXTURE_FORMAT_SWAP_CHAIN
            ? se_vk_device_get_swap_chain_format(texture->createInfo.device)
            :   (texture->createInfo.format == SE_TEXTURE_FORMAT_DEPTH_STENCIL
                ? se_vk_device_get_depth_stencil_format(texture->createInfo.device)
                : se_vk_utils_to_vk_format(texture->createInfo.format));
        texture->fullSubresourceRange = (VkImageSubresourceRange)
        {
            .aspectMask     = aspect,
            .baseMipLevel   = 0,
            .levelCount     = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        };
        texture->flags = flags;
    }
    //
    // Create VkImage
    //
    {
        uint32_t queueFamilyIndices[SE_VK_MAX_UNIQUE_COMMAND_QUEUES] = {0};
        VkSharingMode sharingMode = 0;
        uint32_t queueFamilyIndexCount = 0;
        {
            const uint32_t graphicsQueueFamilyIndex = se_vk_device_get_command_queue_family_index(texture->createInfo.device, SE_VK_CMD_QUEUE_GRAPHICS);
            const uint32_t transferQueueFamilyIndex = se_vk_device_get_command_queue_family_index(texture->createInfo.device, SE_VK_CMD_QUEUE_TRANSFER);
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
        VkImageUsageFlags usage = 0;
        if (texture->createInfo.usage & SE_TEXTURE_USAGE_RENDER_PASS_ATTACHMENT)
        {
            usage |= texture->createInfo.format == SE_TEXTURE_FORMAT_DEPTH_STENCIL
                ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
                : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
        if (texture->createInfo.usage & SE_TEXTURE_USAGE_TEXTURE)
        {
            usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        }
        VkImageCreateInfo imageCreateInfo = (VkImageCreateInfo)
        {
            .sType                  = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext                  = NULL,
            .flags                  = 0,
            .imageType              = VK_IMAGE_TYPE_2D, // @TODO : support other image types
            .format                 = texture->format,
            .extent                 = texture->extent,
            .mipLevels              = 1,
            .arrayLayers            = 1,
            .samples                = VK_SAMPLE_COUNT_1_BIT,
            .tiling                 = VK_IMAGE_TILING_OPTIMAL,
            .usage                  = usage,
            .sharingMode            = sharingMode,
            .queueFamilyIndexCount  = queueFamilyIndexCount,
            .pQueueFamilyIndices    = queueFamilyIndices,
            .initialLayout          = texture->currentLayout,
        };
        se_vk_check(vkCreateImage(logicalHandle, &imageCreateInfo, callbacks, &texture->image));
    }
    //
    // Allocate and bind memory
    //
    {
        VkMemoryRequirements memRequirements = {0};
        vkGetImageMemoryRequirements(logicalHandle, texture->image, &memRequirements);
        SeVkGpuAllocationRequest request = (SeVkGpuAllocationRequest)
        {
            .sizeBytes          = memRequirements.size,
            .alignment          = memRequirements.alignment,
            .memoryTypeBits     = memRequirements.memoryTypeBits,
            .properties         = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        };
        texture->memory = se_vk_gpu_allocate(memoryManager, request);
        vkBindImageMemory(logicalHandle, texture->image, texture->memory.memory, texture->memory.offsetBytes);
    }
    //
    // Create VkImageView
    //
    {
        VkImageViewCreateInfo viewCreateInfo = (VkImageViewCreateInfo)
        {
            .sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext              = NULL,
            .flags              = 0,
            .image              = texture->image,
            .viewType           = VK_IMAGE_VIEW_TYPE_2D,
            .format             = texture->format,
            .components         = (VkComponentMapping)
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
    //
    //
    //
    se_vk_add_external_resource_dependency(texture->createInfo.device);
}

void se_vk_texture_recreate_inplace_from_swap_chain(SeRenderObject* _texture, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't recreate texture");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    //
    //
    //
    memset(__SE_VK_TEXTURE_RECREATE_ZEROING_OFFSET(texture), 0, __SE_VK_TEXTURE_RECREATE_ZEROING_SIZE);
    texture->extent = (VkExtent3D) { extent->width, extent->height, 1 };
    texture->image = image;
    texture->view = view;
    texture->format = format;
    texture->fullSubresourceRange = (VkImageSubresourceRange)
    {
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    texture->flags = SE_VK_TEXTURE_FROM_SWAP_CHAIN;
    //
    //
    //
    se_vk_add_external_resource_dependency(texture->createInfo.device);
}

void se_vk_texture_submit_for_deffered_destruction(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't submit texture for deffered destruction");
    SeVkInFlightManager* inFlightManager = se_vk_device_get_in_flight_manager(((SeVkTexture*)_texture)->createInfo.device);
    se_vk_in_flight_manager_submit_deffered_destruction(inFlightManager, (SeVkDefferedDestruction) { _texture, se_vk_texture_destroy });
}

void se_vk_texture_destroy(SeRenderObject* _texture)
{
    se_vk_texture_destroy_inplace(_texture);
    SeVkTexture* texture = (SeVkTexture*)_texture;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(texture->createInfo.device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(texture->createInfo.device);
    //
    //
    //
    se_object_pool_return(SeVkTexture, se_vk_memory_manager_get_pool(memoryManager, SE_RENDER_HANDLE_TYPE_TEXTURE), texture);
}

void se_vk_texture_destroy_inplace(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't destroy texture");
    se_vk_check_external_resource_dependencies(_texture);
    SeVkTexture* texture = (SeVkTexture*)_texture;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(texture->createInfo.device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(texture->createInfo.device);
    //
    //
    //
    if (!(texture->flags & SE_VK_TEXTURE_FROM_SWAP_CHAIN))
    {
        vkDestroyImageView(logicalHandle, texture->view, callbacks);
        vkDestroyImage(logicalHandle, texture->image, callbacks);
        se_vk_gpu_deallocate(memoryManager, texture->memory);
    }
    //
    //
    //
    se_vk_remove_external_resource_dependency(texture->createInfo.device);
}

uint32_t se_vk_texture_get_width(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't get texture width");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->extent.width;
}

uint32_t se_vk_texture_get_height(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't get texture height");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->extent.height;
}

VkFormat se_vk_texture_get_format(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't get texture format");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->format;
}

VkImageView se_vk_texture_get_view(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't get texture view");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->view;
}

VkExtent3D se_vk_texture_get_extent(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't get texture extent");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->extent;
}

void se_vk_texture_transition_image_layout(SeRenderObject* _texture, VkCommandBuffer commandBuffer, VkImageLayout targetLayout)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't transition texture image layout");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    VkImageMemoryBarrier imageBarrier = (VkImageMemoryBarrier)
    {
        .sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext                  = NULL,
        .srcAccessMask          = se_vk_utils_image_layout_to_access_flags(texture->currentLayout),
        .dstAccessMask          = se_vk_utils_image_layout_to_access_flags(targetLayout),
        .oldLayout              = texture->currentLayout,
        .newLayout              = targetLayout,
        .srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
        .image                  = texture->image,
        .subresourceRange       = texture->fullSubresourceRange,
    };
    vkCmdPipelineBarrier
    (
        commandBuffer,
        se_vk_utils_image_layout_to_pipeline_stage_flags(texture->currentLayout),
        se_vk_utils_image_layout_to_pipeline_stage_flags(targetLayout),
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &imageBarrier
    );
    texture->currentLayout = targetLayout;
}

VkImageLayout se_vk_texture_get_current_layout(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't get current texture image layout");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->currentLayout;
}

bool se_vk_texture_is_dependent_on_swap_chain(SeRenderObject* _texture)
{
    se_vk_expect_handle(_texture, SE_RENDER_HANDLE_TYPE_TEXTURE, "Can't check if texture is dependent on swap chain");
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->flags & SE_VK_TEXTURE_DEPENDS_ON_SWAP_CHAIN_FROMAT || texture->flags & SE_VK_TEXTURE_DEPENDS_ON_SWAP_CHAIN_EXTENT;
}
