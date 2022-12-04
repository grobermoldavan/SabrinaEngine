
#include "vulkan/se_vulkan_base.hpp"
#include "vulkan/se_vulkan_device.hpp"
#include "vulkan/se_vulkan_frame_manager.hpp"
#include "vulkan/se_vulkan_framebuffer.hpp"
#include "vulkan/se_vulkan_graph.hpp"
#include "vulkan/se_vulkan_memory_buffer.hpp"
#include "vulkan/se_vulkan_memory.hpp"
#include "vulkan/se_vulkan_pipeline.hpp"
#include "vulkan/se_vulkan_program.hpp"
#include "vulkan/se_vulkan_render_pass.hpp"
#include "vulkan/se_vulkan_sampler.hpp"
#include "vulkan/se_vulkan_texture.hpp"
#include "vulkan/se_vulkan_command_buffer.hpp"
#include "vulkan/se_vulkan_utils.hpp"
#include "engine/engine.hpp"

SeVkDevice* g_vulkanDevice;

template<typename Ref>
typename SeVkRefToResource<Ref>::Res* se_vk_unref(Ref ref)
{
    auto& pool = se_vk_memory_manager_get_pool<typename SeVkRefToResource<Ref>::Res>(&g_vulkanDevice->memoryManager);
    auto* const result = object_pool::access(pool, ref.index, ref.generation);
    se_assert_msg(result, "Ref is either invalid or expired");
    se_assert_msg((result->object.flags & SeVkObject::Flags::IN_GRAVEYARD) == 0, "Trying to unref already deleted objct");
    return result;
}

template<typename Ref>
typename SeVkRefToResource<Ref>::Res* se_vk_unref_graveyard(Ref ref)
{
    auto& pool = se_vk_memory_manager_get_pool<typename SeVkRefToResource<Ref>::Res>(&g_vulkanDevice->memoryManager);
    auto* const result = object_pool::access(pool, ref.index, ref.generation);
    se_assert_msg(result, "Ref is either invalid or expired");
    se_assert_msg(result->object.flags & SeVkObject::Flags::IN_GRAVEYARD, "Trying to unref not deleted object");
    return result;
}

template<typename Ref>
ObjectPoolEntryRef<typename SeVkRefToResource<Ref>::Res> se_vk_to_pool_ref(Ref ref)
{
    return
    {
        &se_vk_memory_manager_get_pool<typename SeVkRefToResource<Ref>::Res>(&g_vulkanDevice->memoryManager),
        ref.index,
        ref.generation
    };
}

namespace render
{
    namespace impl
    {
        void write(SeVkMemoryBuffer* buffer, void* sourcePtr, size_t sourceSize, size_t offset)
        {
            if (buffer->memory.mappedMemory)
            {
                memcpy(buffer->memory.mappedMemory, sourcePtr, sourceSize);
            }
            else
            {
                ObjectPool<SeVkCommandBuffer>& cmdPool = se_vk_memory_manager_get_pool<SeVkCommandBuffer>(&g_vulkanDevice->memoryManager);
                SeVkCommandBuffer* const cmd = object_pool::take(cmdPool);
                SeVkCommandBufferInfo cmdInfo =
                {
                    .device = g_vulkanDevice,
                    .usage  = SE_VK_COMMAND_BUFFER_USAGE_TRANSFER,
                };
                SeVkMemoryBuffer* const stagingBuffer = se_vk_memory_manager_get_staging_buffer(&g_vulkanDevice->memoryManager);
                const size_t stagingBufferSize = stagingBuffer->memory.size;
                void* const stagingMemory = stagingBuffer->memory.mappedMemory;
                se_assert(stagingMemory);
                const size_t numCopies = (sourceSize / stagingBufferSize) + (sourceSize % stagingBufferSize ? 1 : 0);
                for (size_t it = 0; it < numCopies; it++)
                {
                    const size_t dataLeft = sourceSize - (it * stagingBufferSize);
                    const size_t copySize = se_min(dataLeft, stagingBufferSize);
                    memcpy(stagingMemory, ((char*)sourcePtr) + it * stagingBufferSize, copySize);
                    se_vk_command_buffer_construct(cmd, &cmdInfo);
                    const VkBufferCopy copy
                    {
                        .srcOffset  = 0,
                        .dstOffset  = offset + it * stagingBufferSize,
                        .size       = copySize,
                    };
                    vkCmdCopyBuffer(cmd->handle, stagingBuffer->handle, buffer->handle, 1, &copy);
                    SeVkCommandBufferSubmitInfo submit = { };
                    se_vk_command_buffer_submit(cmd, &submit);
                    vkWaitForFences(se_vk_device_get_logical_handle(g_vulkanDevice), 1, &cmd->fence, VK_TRUE, UINT64_MAX);    
                    se_vk_command_buffer_destroy(cmd);
                }
                object_pool::release(cmdPool, cmd);
            }
        }
    }

    bool begin_frame()
    {
        const uint32_t width = win::get_width();
        const uint32_t height = win::get_height();
        if (width == 0 && height == 0)
        {
            return false;
        }
        se_vk_device_begin_frame(g_vulkanDevice, { width, height });
        return true;
    }

    void end_frame()
    {
        se_vk_device_end_frame(g_vulkanDevice);
    }

    SePassDependencies begin_graphics_pass(const SeGraphicsPassInfo& info)
    {
        return se_vk_graph_begin_graphics_pass(&g_vulkanDevice->graph, info);
    }

    SePassDependencies begin_compute_pass(const SeComputePassInfo& info)
    {
        return se_vk_graph_begin_compute_pass(&g_vulkanDevice->graph, info);
    }
    
    void end_pass()
    {
        se_vk_graph_end_pass(&g_vulkanDevice->graph);
    }

    SeProgramRef program(const SeProgramInfo& info)
    {
        se_assert(data_provider::is_valid(info.data));

        SeVkMemoryManager* const memoryManager = &g_vulkanDevice->memoryManager;
        ObjectPool<SeVkProgram>& pool = se_vk_memory_manager_get_pool<SeVkProgram>(memoryManager);

        SeVkProgramInfo vkInfo
        {
            .device = g_vulkanDevice,
            .data   = info.data,
        };
        SeVkProgram* const result = object_pool::take(pool);
        se_vk_program_construct(result, &vkInfo);

        const auto ref = object_pool::to_ref(pool, result);
        return { ref.index, ref.generation };
    }

    SeTextureRef texture(const SeTextureInfo& info)
    {
        ObjectPool<SeVkTexture>& pool = se_vk_memory_manager_get_pool<SeVkTexture>(&g_vulkanDevice->memoryManager);
        SeVkTexture* const result = object_pool::take(pool);

        VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        if (data_provider::is_valid(info.data)) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (info.format == SeTextureFormat::DEPTH_STENCIL) usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        SeVkTextureInfo vkInfo
        {
            .device     = g_vulkanDevice,
            .format     = info.format == SeTextureFormat::DEPTH_STENCIL ? se_vk_device_get_depth_stencil_format(g_vulkanDevice) : se_vk_utils_to_vk_format(info.format),
            .extent     = { info.width, info.height, 1 },
            .usage      = usage,
            .sampling   = VK_SAMPLE_COUNT_1_BIT, // @TODO : support multisampling
            .data       = info.data,
        };
        se_vk_texture_construct(result, &vkInfo);

        const auto ref = object_pool::to_ref(pool, result);
        return { ref.index, ref.generation, 0 };
    }

    SeTextureRef swap_chain_texture()
    {
        return { 0, 0, 1 };
    }

    SeBufferRef memory_buffer(const SeMemoryBufferInfo& info)
    {
        se_assert(data_provider::is_valid(info.data));
        const auto [sourcePtr, sourceSize] = data_provider::get(info.data);

        ObjectPool<SeVkMemoryBuffer>& memoryBufferPool = se_vk_memory_manager_get_pool<SeVkMemoryBuffer>(&g_vulkanDevice->memoryManager);
        SeVkMemoryBuffer* const result = object_pool::take(memoryBufferPool);
        SeVkMemoryBufferInfo vkInfo
        {
            .device     = g_vulkanDevice,
            .size       = sourceSize,
            .usage      =   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | 
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT   | 
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT   ,
            .visibility = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        };
        se_vk_memory_buffer_construct(result, &vkInfo);
        
        if (sourcePtr)
        {
            impl::write(result, sourcePtr, sourceSize, 0);
        }

        const auto ref = object_pool::to_ref(memoryBufferPool, result);
        return { ref.index, ref.generation, 0 };
    }

    SeBufferRef scratch_memory_buffer(const SeMemoryBufferInfo& info)
    {
        return
        {
            se_vk_frame_manager_alloc_scratch_buffer(&g_vulkanDevice->frameManager, info.data),
            se_vk_safe_cast<uint32_t>(g_vulkanDevice->frameManager.frameNumber),
            1,
        };
    }

    SeSamplerRef sampler(const SeSamplerInfo& info)
    {
        SeVkMemoryManager* const memoryManager = &g_vulkanDevice->memoryManager;
        ObjectPool<SeVkSampler>& samplerPool = se_vk_memory_manager_get_pool<SeVkSampler>(memoryManager);

        SeVkSamplerInfo vkInfo
        {
            .device             = g_vulkanDevice,
            .magFilter          = (VkFilter)info.magFilter,
            .minFilter          = (VkFilter)info.minFilter,
            .addressModeU       = (VkSamplerAddressMode)info.addressModeU,
            .addressModeV       = (VkSamplerAddressMode)info.addressModeV,
            .addressModeW       = (VkSamplerAddressMode)info.addressModeW,
            .mipmapMode         = (VkSamplerMipmapMode)info.mipmapMode,
            .mipLodBias         = info.mipLodBias,
            .minLod             = info.minLod,
            .maxLod             = info.maxLod,
            .anisotropyEnabled  = info.anisotropyEnable,
            .maxAnisotropy      = info.maxAnisotropy,
            .compareEnabled     = info.compareEnabled,
            .compareOp          = (VkCompareOp)info.compareOp,
        };
        SeVkSampler* const result = object_pool::take(samplerPool);
        se_vk_sampler_construct(result, &vkInfo);

        const auto ref = object_pool::to_ref(samplerPool, result);
        return { ref.index, ref.generation };
    }

    void bind(const SeCommandBindInfo& info)
    {
        se_vk_graph_command_bind(&g_vulkanDevice->graph, info);
    }

    void draw(const SeCommandDrawInfo& info)
    {
        se_vk_graph_command_draw(&g_vulkanDevice->graph, info);
    }

    void dispatch(const SeCommandDispatchInfo& info)
    {
        se_vk_graph_command_dispatch(&g_vulkanDevice->graph, info);
    }

    void write(const SeMemoryBufferWriteInfo& info)
    {
        se_assert(data_provider::is_valid(info.data));
        const auto [sourcePtr, sourceSize] = data_provider::get(info.data);

        SeVkMemoryBuffer* const buffer = se_vk_unref(info.buffer);
        se_assert((info.offset + sourceSize) <= buffer->memory.size);

        impl::write(buffer, sourcePtr, sourceSize, info.offset);
    }

    SeFloat4x4 perspective(float fovDeg, float aspect, float nearPlane, float farPlane)
    {
        //
        // https://vincent-p.github.io/posts/vulkan_perspective_matrix/
        // @NOTE : This matrix works for reverse depth (near plane = 1, far plane = 0).
        //         If you want to change this, then render pass clear values must be reevaluated
        //         as well as render pipeline depthCompareOp.
        // @NOTE : This matrix works for coordinate system used in se_math.h (forward == positive z, up == pozitive y)
        //
        const float focalLength = 1.0f / tanf(se_to_radians(fovDeg) / 2.0f);
        const float x =  focalLength / aspect;
        const float y = -focalLength;
        const float A = nearPlane / (nearPlane - farPlane);
        const float B = A * -farPlane;
        return
        {
            x,    0.0f,  0.0f, 0.0f,
            0.0f,    y,  0.0f, 0.0f,
            0.0f, 0.0f,     A,    B,
            0.0f, 0.0f,  1.0f, 0.0f,
        };
    }

    SeFloat4x4 orthographic(float left, float right, float bottom, float top, float nearPlane, float farPlane)
    {
        const float farMNear = farPlane - nearPlane;
        const float rightMLeft = right - left;
        const float bottomMTop = bottom - top;
        return
        {
            2.0f / rightMLeft, 0                , 0              , -(right + left) / rightMLeft,
            0                , 2.0f / bottomMTop, 0              , -(bottom + top) / bottomMTop,
            0                , 0                , 1.0f / farMNear, -nearPlane/farMNear,
            0                , 0                , 0              , 1,
        };
    }

    SeTextureSize texture_size(SeTextureRef ref)
    {
        if (ref.isSwapChain)
        {
            const VkExtent2D extent = se_vk_device_get_swap_chain_extent(g_vulkanDevice);
            return { extent.width, extent.height, 1 };
        }
        else
        {
            const SeVkTexture* const texture = se_vk_unref(ref);
            return { texture->extent.width, texture->extent.height, texture->extent.depth };
        }
    }

    SeComputeWorkgroupSize workgroup_size(SeProgramRef ref)
    {
        const SeVkProgram* const program = se_vk_unref(ref);
        return
        {
            program->reflection.computeWorkGroupSizeX,
            program->reflection.computeWorkGroupSizeY,
            program->reflection.computeWorkGroupSizeZ,
        };
    }

    void destroy(SeProgramRef ref)
    {
        se_vk_device_submit_to_graveyard(g_vulkanDevice, ref);
    }

    void destroy(SeSamplerRef ref)
    {
        se_vk_device_submit_to_graveyard(g_vulkanDevice, ref);
    }

    void destroy(SeBufferRef ref)
    {
        se_assert_msg(!ref.isScratch, "You shouldn't manually destroy scratch buffers");
        se_vk_device_submit_to_graveyard(g_vulkanDevice, ref);
    }

    void destroy(SeTextureRef ref)
    {
        se_vk_device_submit_to_graveyard(g_vulkanDevice, ref);
    }

    void engine::init()
    {
        se_vk_check(volkInitialize());
        g_vulkanDevice = se_vk_device_create(win::get_native_handle());
    }

    void engine::terminate()
    {
        se_vk_device_destroy(g_vulkanDevice);
    }

    void engine::update()
    {
        se_vk_device_update_graveyard(g_vulkanDevice);
    }
}

#define VOLK_IMPLEMENTATION
#include "engine/libs/volk/volk.h"

#define SSR_IMPL
#include "engine/libs/ssr/simple_spirv_reflection.h"

#include "vulkan/se_vulkan_device.cpp"
#include "vulkan/se_vulkan_frame_manager.cpp"
#include "vulkan/se_vulkan_framebuffer.cpp"
#include "vulkan/se_vulkan_graph.cpp"
#include "vulkan/se_vulkan_memory_buffer.cpp"
#include "vulkan/se_vulkan_memory.cpp"
#include "vulkan/se_vulkan_pipeline.cpp"
#include "vulkan/se_vulkan_program.cpp"
#include "vulkan/se_vulkan_render_pass.cpp"
#include "vulkan/se_vulkan_sampler.cpp"
#include "vulkan/se_vulkan_texture.cpp"
#include "vulkan/se_vulkan_command_buffer.cpp"
#include "vulkan/se_vulkan_utils.cpp"
