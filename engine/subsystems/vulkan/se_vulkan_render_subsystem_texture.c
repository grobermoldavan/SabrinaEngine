
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_device.h"
#include "se_vulkan_render_subsystem_memory.h"
#include "se_vulkan_render_subsystem_utils.h"
#include "engine/render_abstraction_interface.h"
#include "engine/allocator_bindings.h"

enum SeVkTextureFlags
{
    SE_VK_TEXTURE_FROM_EXTERNAL_RESOURCES = 0x00000001,
};

struct SeVkRenderDevice;

typedef struct SeVkTexture
{
    SeRenderObject object;
    SeRenderObject* device;
    VkExtent3D extent;
    VkFormat format;
    VkImageSubresourceRange fullSubresourceRange;
    VkImageLayout currentLayout;
    VkImage handle;
    SeVkMemory memory;
    VkImageView imageView;
    VkSampler defaultSampler;
    uint64_t flags;
} SeVkTexture;

SeRenderObject* se_vk_texture_create(SeTextureCreateInfo* createInfo)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    SeVkTexture* texture = allocator->alloc(allocator->allocator, sizeof(SeVkTexture), se_default_alignment, se_alloc_tag);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(createInfo->device);
    //
    // Basic info
    //
    texture->object.handleType = SE_RENDER_TEXTURE;
    texture->object.destroy = se_vk_texture_destroy;
    texture->device = createInfo->device;
    texture->extent = (VkExtent3D){ .width = createInfo->width, .height = createInfo->height, .depth = 1, };
    texture->format = createInfo->format == SE_TEXTURE_FORMAT_DEPTH_STENCIL ? se_vk_device_get_depth_stencil_format(createInfo->device) : se_vk_utils_to_vk_format(createInfo->format);
    const VkImageAspectFlags aspect = createInfo->format == SE_TEXTURE_FORMAT_DEPTH_STENCIL 
        ? (se_vk_device_is_stencil_supported(createInfo->device) ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT)
        : VK_IMAGE_ASPECT_COLOR_BIT;
    texture->fullSubresourceRange = (VkImageSubresourceRange)
    {
        .aspectMask     = aspect,
        .baseMipLevel   = 0,
        .levelCount     = VK_REMAINING_MIP_LEVELS,
        .baseArrayLayer = 0,
        .layerCount     = VK_REMAINING_ARRAY_LAYERS,
    };
    texture->currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    //
    // Create VkImage
    //
    VkImageUsageFlags usage = 0;
    if (createInfo->usage & SE_TEXTURE_USAGE_RENDER_PASS_ATTACHMENT)
    {
        usage |= createInfo->format == SE_TEXTURE_FORMAT_DEPTH_STENCIL 
            ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
            : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    }
    if (createInfo->usage & SE_TEXTURE_USAGE_TEXTURE)
    {
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    uint32_t queueFamilyIndices[SE_VK_MAX_UNIQUE_COMMAND_QUEUES] = {0};
    VkSharingMode sharingMode = 0;
    uint32_t queueFamilyIndexCount = 0;
    {
        uint32_t graphicsQueueFamilyIndex = se_vk_device_get_command_queue_family_index(createInfo->device, SE_VK_CMD_QUEUE_GRAPHICS);
        uint32_t transferQueueFamilyIndex = se_vk_device_get_command_queue_family_index(createInfo->device, SE_VK_CMD_QUEUE_TRANSFER);
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
    se_vk_check(vkCreateImage(logicalHandle, &imageCreateInfo, callbacks, &texture->handle));
    //
    // Allocate and bind memory
    //
    VkMemoryRequirements memRequirements = {0};
    vkGetImageMemoryRequirements(logicalHandle, texture->handle, &memRequirements);
    SeVkGpuAllocationRequest request = (SeVkGpuAllocationRequest)
    {
        .sizeBytes          = memRequirements.size,
        .alignment          = memRequirements.alignment,
        .memoryTypeBits     = memRequirements.memoryTypeBits,
        .properties         = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    };
    texture->memory = se_vk_gpu_allocate(memoryManager, request);
    vkBindImageMemory(logicalHandle, texture->handle, texture->memory.memory, texture->memory.offsetBytes);
    //
    // Create VkImageView
    //
    VkImageViewCreateInfo viewCreateInfo = (VkImageViewCreateInfo)
    {
        .sType              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext              = NULL,
        .flags              = 0,
        .image              = texture->handle,
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
    se_vk_check(vkCreateImageView(logicalHandle, &viewCreateInfo, callbacks, &texture->imageView));
    //
    // Create VkSampler (if image is sampled)
    //
    if (createInfo->usage & SE_TEXTURE_USAGE_TEXTURE)
    {
        // @TODO : many of this create info parameters should be in the texture settings (SeTextureCreateInfo)...
        VkSamplerCreateInfo samplerCreateInfo = (VkSamplerCreateInfo)
        {
            .sType                      = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext                      = NULL,
            .flags                      = 0,
            .magFilter                  = VK_FILTER_LINEAR,
            .minFilter                  = VK_FILTER_LINEAR,
            .mipmapMode                 = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .addressModeU               = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeV               = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .addressModeW               = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipLodBias                 = 0.0f,
            .anisotropyEnable           = VK_FALSE, // todo
            .maxAnisotropy              = 0.0f,     // todo
            .compareEnable              = VK_FALSE,
            .compareOp                  = VK_COMPARE_OP_ALWAYS,
            .minLod                     = 0.0f,
            .maxLod                     = 0.0f,
            .borderColor                = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates    = VK_FALSE,
        };
        se_vk_check(vkCreateSampler(logicalHandle, &samplerCreateInfo, callbacks, &texture->defaultSampler));
    }
    else
    {
        texture->defaultSampler = VK_NULL_HANDLE;
    }
    return (SeRenderObject*)texture;
}

SeRenderObject* se_vk_texture_create_from_external_resources(SeRenderObject* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    SeVkTexture* texture = allocator->alloc(allocator->allocator, sizeof(SeVkTexture), se_default_alignment, se_alloc_tag);
    *texture = (SeVkTexture)
    {
        .object             = (SeRenderObject)
        {
            .handleType     = SE_RENDER_TEXTURE,
            .destroy        = se_vk_texture_destroy,
        },
        .device             = device,
        .fullSubresourceRange = (VkImageSubresourceRange)
        {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        },
        .extent             = (VkExtent3D) { extent->width, extent->height, 1 },
        .handle             = image,
        .imageView          = view,
        .format             = format,
        .currentLayout      = VK_IMAGE_LAYOUT_UNDEFINED,
        .flags              = SE_VK_TEXTURE_FROM_EXTERNAL_RESOURCES,
    };
    return (SeRenderObject*)texture;
}

void se_vk_texture_destroy(SeRenderObject* _texture)
{
    SeVkTexture* texture = (SeVkTexture*)_texture;
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(texture->device);
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(texture->device);
    if (!(texture->flags & SE_VK_TEXTURE_FROM_EXTERNAL_RESOURCES))
    {
        if (texture->defaultSampler) vkDestroySampler(logicalHandle, texture->defaultSampler, callbacks);
        vkDestroyImageView(logicalHandle, texture->imageView, callbacks);
        vkDestroyImage(logicalHandle, texture->handle, callbacks);
        se_vk_gpu_deallocate(memoryManager, texture->memory);
    }
    SeAllocatorBindings* allocator = memoryManager->cpu_persistentAllocator;
    allocator->dealloc(allocator->allocator, texture, sizeof(SeVkTexture));
}

uint32_t se_vk_texture_get_width(SeRenderObject* _texture)
{
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->extent.width;
}

uint32_t se_vk_texture_get_height(SeRenderObject* _texture)
{
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->extent.height;
}

VkFormat se_vk_texture_get_format(SeRenderObject* _texture)
{
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->format;
}

VkImageView se_vk_texture_get_view(SeRenderObject* _texture)
{
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->imageView;
}

VkExtent3D se_vk_texture_get_extent(SeRenderObject* _texture)
{
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->extent;
}

void se_vk_texture_transition_image_layout(SeRenderObject* _texture, VkCommandBuffer commandBuffer, VkImageLayout targetLayout)
{
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
        .image                  = texture->handle,
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
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->currentLayout;
}

VkSampler se_vk_texture_get_default_sampler(SeRenderObject* _texture)
{
    SeVkTexture* texture = (SeVkTexture*)_texture;
    return texture->defaultSampler;
}
