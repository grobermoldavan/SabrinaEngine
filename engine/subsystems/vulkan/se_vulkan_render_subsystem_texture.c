
#include "se_vulkan_render_subsystem_base.h"
#include "se_vulkan_render_subsystem_texture.h"
#include "se_vulkan_render_subsystem_device.h"
#include "engine/render_abstraction_interface.h"
#include "engine/allocator_bindings.h"

enum SeVkTextureFlags
{
    SE_VK_TEXTURE_FROM_EXTERNAL_RESOURCES = 0x00000001,
};

struct SeVkRenderDevice;

typedef struct SeVkTexture
{
    SeRenderObject handle;
    SeRenderObject* device;
    VkImageSubresourceRange subresourceRange;
    VkExtent3D extent;
    VkImage image;
    VkImageView imageView;
    VkFormat format;
    VkImageLayout currentLayout;
    VkSampler defaultSampler; // todo
    uint64_t flags;
} SeVkTexture;

SeRenderObject* se_vk_texture_create(SeTextureCreateInfo* createInfo)
{
    se_assert(!"todo");

    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(createInfo->device);
    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    SeVkTexture* texture = allocator->alloc(allocator->allocator, sizeof(SeVkTexture), se_default_alignment);
    texture->handle.handleType = SE_RENDER_TEXTURE;
    texture->handle.destroy = se_vk_texture_destroy;
    texture->device = createInfo->device;
    // @TODO
    return (SeRenderObject*)texture;
}

SeRenderObject* se_vk_texture_create_from_external_resources(SeRenderObject* device, VkExtent2D* extent, VkImage image, VkImageView view, VkFormat format)
{
    SeVkMemoryManager* memoryManager = se_vk_device_get_memory_manager(device);
    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    SeVkTexture* texture = allocator->alloc(allocator->allocator, sizeof(SeVkTexture), se_default_alignment);
    *texture = (SeVkTexture)
    {
        .handle             = (SeRenderObject)
        {
            .handleType     = SE_RENDER_TEXTURE,
            .destroy        = se_vk_texture_destroy,
        },
        .device             = device,
        .subresourceRange   = (VkImageSubresourceRange)
        {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        },
        .extent             = (VkExtent3D) { extent->width, extent->height, 1 },
        .image              = image,
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

    se_assert(texture->flags & SE_VK_TEXTURE_FROM_EXTERNAL_RESOURCES && "todo");

    SeAllocatorBindings* allocator = memoryManager->cpu_allocator;
    allocator->dealloc(allocator->allocator, texture, sizeof(SeVkTexture));
}
