
#include "se_vulkan_base.hpp"
#include "se_vulkan_texture.hpp"
#include "se_vulkan_device.hpp"
#include "se_vulkan_command_buffer.hpp"
#include "se_vulkan_utils.hpp"

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
    const SeAllocatorBindings allocator = se_allocator_persistent();
    void* const ptr = allocator.alloc(allocator.allocator, size, se_default_alignment, se_alloc_tag);
    se_assert(g_numStbiAllocations < MAX_STBI_ALLOCATIONS);
    g_stbiAllocations[g_numStbiAllocations++] = { ptr, size };
    return ptr;
}

#define STBI_REALLOC(p,newsz) se_vk_texture_stbi_realloc(p, newsz)
void* se_vk_texture_stbi_realloc(void* oldPtr, size_t newSize)
{
    SeAllocatorBindings allocator = se_allocator_persistent();
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
    SeAllocatorBindings allocator = se_allocator_persistent();
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

void se_vk_texture_construct(SeVkTexture* texture, SeVkTextureInfo* info)
{
    SeVkMemoryManager* const memoryManager = &info->device->memoryManager;
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(info->device);
    //
    // Read data and process if it is an image file
    //
    VkExtent3D textureExtent = info->extent;
    void* loadedTextureData = nullptr;
    size_t loadedTextureDataSize = 0;
    if (se_data_provider_is_valid(info->data))
    {
        auto [sourcePtr, sourceSize] = se_data_provider_get(info->data);
        if (info->data.type == SeDataProvider::FROM_FILE)
        {
            const SeVkFormatInfo formatInfo = se_vk_utils_get_format_info(info->format);
            int dimX = 0;
            int dimY = 0;
            int channels = 0;
            void* loadedImagePtr = nullptr;
            if (formatInfo.componentsSizeBits == 8 && formatInfo.componentType == SeVkFormatInfo::Type::UINT)
            {
                loadedImagePtr = stbi_load_from_memory((const stbi_uc*)sourcePtr, (int)sourceSize, &dimX, &dimY, &channels, int(formatInfo.numComponents));
            }
            else if (formatInfo.componentsSizeBits == 16 && formatInfo.componentType == SeVkFormatInfo::Type::UINT)
            {
                loadedImagePtr = stbi_load_16_from_memory((const stbi_uc*)sourcePtr, (int)sourceSize, &dimX, &dimY, &channels, int(formatInfo.numComponents));
            }
            else if (formatInfo.componentsSizeBits == 32 && formatInfo.componentType == SeVkFormatInfo::Type::FLOAT)
            {
                loadedImagePtr = stbi_loadf_from_memory((const stbi_uc*)sourcePtr, (int)sourceSize, &dimX, &dimY, &channels, int(formatInfo.numComponents));
            }
            else
            {
                se_assert_msg(false, "Unsupported texture file format");
            }
            se_assert(loadedImagePtr);
            loadedTextureData = loadedImagePtr;
            loadedTextureDataSize = dimX * dimY * formatInfo.numComponents * formatInfo.componentsSizeBits / 8;
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
            .object                 = { SeVkObject::Type::TEXTURE, 0, g_textureIndex++ },
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
        uint32_t queueFamilyIndices[SeVkConfig::MAX_UNIQUE_COMMAND_QUEUES];
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
            if (info->data.type == SeDataProvider::FROM_FILE)
            {
                stbi_image_free(loadedTextureData);
            }
            //
            // Transition image layout and copy buffer
            //
            SeObjectPool<SeVkCommandBuffer>& cmdPool = se_vk_memory_manager_get_pool<SeVkCommandBuffer>(memoryManager);
            SeVkCommandBuffer* cmd = se_object_pool_take(cmdPool);
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
            se_object_pool_release(cmdPool, cmd);
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
        .object                 = { SeVkObject::Type::TEXTURE, 0, g_textureIndex++ },
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
    SeVkMemoryManager* const memoryManager = &texture->device->memoryManager;
    const VkAllocationCallbacks* const callbacks = se_vk_memory_manager_get_callbacks(memoryManager);
    const VkDevice logicalHandle = se_vk_device_get_logical_handle(texture->device);
    if (!(texture->flags & SE_VK_TEXTURE_FROM_SWAP_CHAIN))
    {
        vkDestroyImageView(logicalHandle, texture->view, callbacks);
        vkDestroyImage(logicalHandle, texture->image, callbacks);
        se_vk_memory_manager_deallocate(memoryManager, texture->memory);
    }
}
