
#include "se_vulkan_render_subsystem_base.hpp"
#include "se_vulkan_render_subsystem_texture.hpp"
#include "se_vulkan_render_subsystem_device.hpp"
#include "se_vulkan_render_subsystem_command_buffer.hpp"
#include "se_vulkan_render_subsystem_utils.hpp"
#include "engine/allocator_bindings.hpp"

constexpr size_t MAX_STBI_ALLOCATIONS = 64;
struct
{
    void* ptr;
    size_t size;
} g_stbiAllocations[MAX_STBI_ALLOCATIONS];
size_t g_numStbiAllocations = 0;

#define STBI_MALLOC(sz) se_vk_texture_stbi_alloc(sz)
void* se_vk_texture_stbi_alloc(size_t size)
{
    AllocatorBindings allocator = app_allocators::persistent();
    void* ptr = allocator.alloc(allocator.allocator, size, se_default_alignment, se_alloc_tag);
    se_assert(g_numStbiAllocations < MAX_STBI_ALLOCATIONS);
    g_stbiAllocations[g_numStbiAllocations++] = { ptr, size };
    return ptr;
}

#define STBI_REALLOC(p,newsz) se_vk_texture_stbi_realloc(p, newsz)
void* se_vk_texture_stbi_realloc(void* oldPtr, size_t newSize)
{
    AllocatorBindings allocator = app_allocators::persistent();
    void* newPtr = se_vk_texture_stbi_alloc(newSize);
    for (size_t it = 0; it < g_numStbiAllocations; it++)
    {
        if (g_stbiAllocations[it].ptr == oldPtr)
        {
            memcpy(newPtr, oldPtr, g_stbiAllocations[it].size);
            allocator.dealloc(allocator.allocator, oldPtr, g_stbiAllocations[it].size);
            g_stbiAllocations[it] = g_stbiAllocations[g_numStbiAllocations - 1];
            g_numStbiAllocations -= 1;
            break;
        }
    }
    return newPtr;
}

#define STBI_FREE(p) se_vk_texture_stbi_free(p)
void se_vk_texture_stbi_free(void* ptr)
{
    AllocatorBindings allocator = app_allocators::persistent();
    for (size_t it = 0; it < g_numStbiAllocations; it++)
    {
        if (g_stbiAllocations[it].ptr == ptr)
        {
            allocator.dealloc(allocator.allocator, ptr, g_stbiAllocations[it].size);
            g_stbiAllocations[it] = g_stbiAllocations[g_numStbiAllocations - 1];
            g_numStbiAllocations -= 1;
            break;
        }
    }
}

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_ASSERT se_assert
#include "engine/libs/stb/stb_image.h"

size_t g_textureIndex = 0;

struct { int numComponents; int componentsSize; bool isFloat; } se_vk_texture_get_format_info(VkFormat format)
{
    switch (format)
    {
        case VK_FORMAT_R8_SNORM:                    return { 1, 8, false };
        case VK_FORMAT_R8_UNORM:                    return { 1, 8, false };
        case VK_FORMAT_R8_USCALED:                  return { 1, 8, false };
        case VK_FORMAT_R8_SSCALED:                  return { 1, 8, false };
        case VK_FORMAT_R8_UINT:                     return { 1, 8, false };
        case VK_FORMAT_R8_SINT:                     return { 1, 8, false };
        case VK_FORMAT_R8_SRGB:                     return { 1, 8, false };
        case VK_FORMAT_R8G8_UNORM:                  return { 2, 8, false };
        case VK_FORMAT_R8G8_SNORM:                  return { 2, 8, false };
        case VK_FORMAT_R8G8_USCALED:                return { 2, 8, false };
        case VK_FORMAT_R8G8_SSCALED:                return { 2, 8, false };
        case VK_FORMAT_R8G8_UINT:                   return { 2, 8, false };
        case VK_FORMAT_R8G8_SINT:                   return { 2, 8, false };
        case VK_FORMAT_R8G8_SRGB:                   return { 2, 8, false };
        case VK_FORMAT_R8G8B8_UNORM:                return { 3, 8, false };
        case VK_FORMAT_R8G8B8_SNORM:                return { 3, 8, false };
        case VK_FORMAT_R8G8B8_USCALED:              return { 3, 8, false };
        case VK_FORMAT_R8G8B8_SSCALED:              return { 3, 8, false };
        case VK_FORMAT_R8G8B8_UINT:                 return { 3, 8, false };
        case VK_FORMAT_R8G8B8_SINT:                 return { 3, 8, false };
        case VK_FORMAT_R8G8B8_SRGB:                 return { 3, 8, false };
        case VK_FORMAT_B8G8R8_UNORM:                return { 3, 8, false };
        case VK_FORMAT_B8G8R8_SNORM:                return { 3, 8, false };
        case VK_FORMAT_B8G8R8_USCALED:              return { 3, 8, false };
        case VK_FORMAT_B8G8R8_SSCALED:              return { 3, 8, false };
        case VK_FORMAT_B8G8R8_UINT:                 return { 3, 8, false };
        case VK_FORMAT_B8G8R8_SINT:                 return { 3, 8, false };
        case VK_FORMAT_B8G8R8_SRGB:                 return { 3, 8, false };
        case VK_FORMAT_R8G8B8A8_UNORM:              return { 4, 8, false };
        case VK_FORMAT_R8G8B8A8_SNORM:              return { 4, 8, false };
        case VK_FORMAT_R8G8B8A8_USCALED:            return { 4, 8, false };
        case VK_FORMAT_R8G8B8A8_SSCALED:            return { 4, 8, false };
        case VK_FORMAT_R8G8B8A8_UINT:               return { 4, 8, false };
        case VK_FORMAT_R8G8B8A8_SINT:               return { 4, 8, false };
        case VK_FORMAT_R8G8B8A8_SRGB:               return { 4, 8, false };
        case VK_FORMAT_B8G8R8A8_UNORM:              return { 4, 8, false };
        case VK_FORMAT_B8G8R8A8_SNORM:              return { 4, 8, false };
        case VK_FORMAT_B8G8R8A8_USCALED:            return { 4, 8, false };
        case VK_FORMAT_B8G8R8A8_SSCALED:            return { 4, 8, false };
        case VK_FORMAT_B8G8R8A8_UINT:               return { 4, 8, false };
        case VK_FORMAT_B8G8R8A8_SINT:               return { 4, 8, false };
        case VK_FORMAT_B8G8R8A8_SRGB:               return { 4, 8, false };
        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:       return { 4, 8, false };
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:       return { 4, 8, false };
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:     return { 4, 8, false };
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:     return { 4, 8, false };
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:        return { 4, 8, false };
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:        return { 4, 8, false };
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:        return { 4, 8, false };
        case VK_FORMAT_R16_UNORM:                   return { 1, 16, false };
        case VK_FORMAT_R16_SNORM:                   return { 1, 16, false };
        case VK_FORMAT_R16_USCALED:                 return { 1, 16, false };
        case VK_FORMAT_R16_SSCALED:                 return { 1, 16, false };
        case VK_FORMAT_R16_UINT:                    return { 1, 16, false };
        case VK_FORMAT_R16_SINT:                    return { 1, 16, false };
        case VK_FORMAT_R16_SFLOAT:                  return { 1, 16, true  };
        case VK_FORMAT_R16G16_UNORM:                return { 2, 16, false };
        case VK_FORMAT_R16G16_SNORM:                return { 2, 16, false };
        case VK_FORMAT_R16G16_USCALED:              return { 2, 16, false };
        case VK_FORMAT_R16G16_SSCALED:              return { 2, 16, false };
        case VK_FORMAT_R16G16_UINT:                 return { 2, 16, false };
        case VK_FORMAT_R16G16_SINT:                 return { 2, 16, false };
        case VK_FORMAT_R16G16_SFLOAT:               return { 2, 16, true  };
        case VK_FORMAT_R16G16B16_UNORM:             return { 3, 16, false };
        case VK_FORMAT_R16G16B16_SNORM:             return { 3, 16, false };
        case VK_FORMAT_R16G16B16_USCALED:           return { 3, 16, false };
        case VK_FORMAT_R16G16B16_SSCALED:           return { 3, 16, false };
        case VK_FORMAT_R16G16B16_UINT:              return { 3, 16, false };
        case VK_FORMAT_R16G16B16_SINT:              return { 3, 16, false };
        case VK_FORMAT_R16G16B16_SFLOAT:            return { 3, 16, true  };
        case VK_FORMAT_R16G16B16A16_UNORM:          return { 4, 16, false };
        case VK_FORMAT_R16G16B16A16_SNORM:          return { 4, 16, false };
        case VK_FORMAT_R16G16B16A16_USCALED:        return { 4, 16, false };
        case VK_FORMAT_R16G16B16A16_SSCALED:        return { 4, 16, false };
        case VK_FORMAT_R16G16B16A16_UINT:           return { 4, 16, false };
        case VK_FORMAT_R16G16B16A16_SINT:           return { 4, 16, false };
        case VK_FORMAT_R16G16B16A16_SFLOAT:         return { 4, 16, true  };
        case VK_FORMAT_R32_UINT:                    return { 1, 32, false };
        case VK_FORMAT_R32_SINT:                    return { 1, 32, false };
        case VK_FORMAT_R32_SFLOAT:                  return { 1, 32, true  };
        case VK_FORMAT_R32G32_UINT:                 return { 2, 32, false };
        case VK_FORMAT_R32G32_SINT:                 return { 2, 32, false };
        case VK_FORMAT_R32G32_SFLOAT:               return { 2, 32, true  };
        case VK_FORMAT_R32G32B32_UINT:              return { 3, 32, false };
        case VK_FORMAT_R32G32B32_SINT:              return { 3, 32, false };
        case VK_FORMAT_R32G32B32_SFLOAT:            return { 3, 32, true  };
        case VK_FORMAT_R32G32B32A32_UINT:           return { 4, 32, false };
        case VK_FORMAT_R32G32B32A32_SINT:           return { 4, 32, false };
        case VK_FORMAT_R32G32B32A32_SFLOAT:         return { 4, 32, true  };
        case VK_FORMAT_R64_UINT:                    return { 1, 64, false };
        case VK_FORMAT_R64_SINT:                    return { 1, 64, false };
        case VK_FORMAT_R64_SFLOAT:                  return { 1, 64, true  };
        case VK_FORMAT_R64G64_UINT:                 return { 2, 64, false };
        case VK_FORMAT_R64G64_SINT:                 return { 2, 64, false };
        case VK_FORMAT_R64G64_SFLOAT:               return { 2, 64, true  };
        case VK_FORMAT_R64G64B64_UINT:              return { 3, 64, false };
        case VK_FORMAT_R64G64B64_SINT:              return { 3, 64, false };
        case VK_FORMAT_R64G64B64_SFLOAT:            return { 3, 64, true  };
        case VK_FORMAT_R64G64B64A64_UINT:           return { 4, 64, false };
        case VK_FORMAT_R64G64B64A64_SINT:           return { 4, 64, false };
        case VK_FORMAT_R64G64B64A64_SFLOAT:         return { 4, 64, true  };
        case VK_FORMAT_D16_UNORM:                   return { 1, 16, false };
        case VK_FORMAT_D32_SFLOAT:                  return { 1, 32, true  };
        case VK_FORMAT_S8_UINT:                     return { 1, 8, false };
        default:                                    { se_assert(false); }
    }
    return { };
}

void se_vk_texture_construct(SeVkTexture* texture, SeVkTextureInfo* info)
{
    SeVkMemoryManager* memoryManager = &info->device->memoryManager;
    VkAllocationCallbacks* callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    //
    // Read data and process if it is an image file
    //
    VkExtent3D textureExtent = info->extent;
    void* loadedTextureData = nullptr;
    size_t loadedTextureDataSize = 0;
    if (data_provider::is_valid(info->data))
    {
        auto [sourcePtr, sourceSize] = data_provider::get(info->data);
        if (info->data.type == DataProvider::FROM_FILE)
        {
            const auto formatInfo = se_vk_texture_get_format_info(info->format);
            int dimX = 0;
            int dimY = 0;
            int channels = 0;
            void* loadedImagePtr = nullptr;
            if (formatInfo.componentsSize == 8 && !formatInfo.isFloat)
            {
                loadedImagePtr = stbi_load_from_memory((const stbi_uc*)sourcePtr, (int)sourceSize, &dimX, &dimY, &channels, formatInfo.numComponents);
            }
            else if (formatInfo.componentsSize == 16 && !formatInfo.isFloat)
            {
                loadedImagePtr = stbi_load_16_from_memory((const stbi_uc*)sourcePtr, (int)sourceSize, &dimX, &dimY, &channels, formatInfo.numComponents);
            }
            else if (formatInfo.componentsSize == 32 && formatInfo.isFloat)
            {
                loadedImagePtr = stbi_loadf_from_memory((const stbi_uc*)sourcePtr, (int)sourceSize, &dimX, &dimY, &channels, formatInfo.numComponents);
            }
            else
            {
                se_assert_msg(false, "Unsupported file format for loading");
            }
            se_assert(loadedImagePtr);
            loadedTextureData = loadedImagePtr;
            loadedTextureDataSize = dimX * dimY * channels * formatInfo.componentsSize / 8;
            textureExtent = { (uint32_t)dimX, (uint32_t)dimY, 1 };
        }
        else
        {
            loadedTextureData = sourcePtr;
            loadedTextureDataSize = sourceSize;
        }
    }
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
            .extent                 = textureExtent,
            .format                 = info->format,
            .currentLayout          = VK_IMAGE_LAYOUT_UNDEFINED,
            .image                  = VK_NULL_HANDLE,
            .memory                 = { },
            .view                   = VK_NULL_HANDLE,
            .fullSubresourceRange   = { aspect, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS },
            .flags                  = 0,
        };
        //
        // Create image
        //
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
        //
        // Copy data from provider
        //
        if (loadedTextureData)
        {
            //
            // Copy to staging memory
            //
            SeVkMemoryBuffer* stagingBuffer = se_vk_memory_manager_get_staging_buffer(memoryManager);
            const size_t stagingBufferSize = stagingBuffer->memory.size;
            void* const stagingMemory = stagingBuffer->memory.mappedMemory;
            se_assert(stagingBufferSize >= loadedTextureDataSize);
            memcpy(stagingMemory, loadedTextureData, loadedTextureDataSize);
            if (info->data.type == DataProvider::FROM_FILE)
            {
                stbi_image_free(loadedTextureData);
            }
            //
            // Transition image layout and copy buffer
            //
            ObjectPool<SeVkCommandBuffer>& cmdPool = se_vk_memory_manager_get_pool<SeVkCommandBuffer>(memoryManager);
            SeVkCommandBuffer* cmd = object_pool::take(cmdPool);
            SeVkCommandBufferInfo cmdInfo =
            {
                .device = info->device,
                .usage  = SE_VK_COMMAND_BUFFER_USAGE_TRANSFER,
            };
            se_vk_command_buffer_construct(cmd, &cmdInfo);
            //
            // Transition
            //
            const VkImageLayout newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            VkImageMemoryBarrier imageBarrier
            {
                .sType                  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .pNext                  = nullptr,
                .srcAccessMask          = se_vk_utils_image_layout_to_access_flags(texture->currentLayout),
                .dstAccessMask          = se_vk_utils_image_layout_to_access_flags(newLayout),
                .oldLayout              = texture->currentLayout,
                .newLayout              = newLayout,
                .srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED,
                .image                  = texture->image,
                .subresourceRange       = texture->fullSubresourceRange,
            };
            vkCmdPipelineBarrier
            (
                cmd->handle,
                se_vk_utils_image_layout_to_pipeline_stage_flags(texture->currentLayout),
                se_vk_utils_image_layout_to_pipeline_stage_flags(newLayout),
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &imageBarrier
            );
            texture->currentLayout = newLayout;
            //
            // Copy
            //
            VkBufferImageCopy copy
            {
                .bufferOffset       = 0,
                .bufferRowLength    = 0,
                .bufferImageHeight  = 0,
                .imageSubresource   =
                {
                    VK_IMAGE_ASPECT_COLOR_BIT,
                    0,
                    0,
                    1,
                },
                .imageOffset        = { 0, 0, 0 },
                .imageExtent        = texture->extent,
            };
            vkCmdCopyBufferToImage(cmd->handle, stagingBuffer->handle, texture->image, texture->currentLayout, 1, &copy);
            //
            // Submit
            //
            SeVkCommandBufferSubmitInfo submit = { };
            se_vk_command_buffer_submit(cmd, &submit);
            vkWaitForFences(logicalHandle, 1, &cmd->fence, VK_TRUE, UINT64_MAX);    
            se_vk_command_buffer_destroy(cmd);
            object_pool::release(cmdPool, cmd);
        }
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
        .memory                 = { },
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
