
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

namespace render
{
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

    SePassDependencies begin_pass(const SeBeginPassInfo& info)
    {
        return se_vk_graph_begin_pass(&g_vulkanDevice->graph, info);
    }

    void end_pass()
    {
        se_vk_graph_end_pass(&g_vulkanDevice->graph);
    }

    SeRenderRef program(const SeProgramInfo& info)
    {
        return se_vk_graph_program(&g_vulkanDevice->graph, info);
    }

    SeRenderRef texture(const SeTextureInfo& info)
    {
        return se_vk_graph_texture(&g_vulkanDevice->graph, info);
    }

    SeRenderRef swap_chain_texture()
    {
        return se_vk_graph_swap_chain_texture(&g_vulkanDevice->graph);
    }

    SeTextureSize texture_size(SeRenderRef texture)
    {
        return se_vk_grap_texture_size(&g_vulkanDevice->graph, texture);
    }

    SeRenderRef graphics_pipeline(const SeGraphicsPipelineInfo& info)
    {
        return se_vk_graph_graphics_pipeline(&g_vulkanDevice->graph, info);
    }

    SeRenderRef compute_pipeline(const SeComputePipelineInfo& info)
    {
        return se_vk_graph_compute_pipeline(&g_vulkanDevice->graph, info);
    }

    SeRenderRef memory_buffer(const SeMemoryBufferInfo& info)
    {
        return se_vk_graph_memory_buffer(&g_vulkanDevice->graph, info);
    }

    SeRenderRef sampler(const SeSamplerInfo& info)
    {
        return se_vk_graph_sampler(&g_vulkanDevice->graph, info);
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

    SeComputeWorkgroupSize workgroup_size(SeRenderRef ref)
    {
        const ObjectPool<SeVkProgram>& programPool = se_vk_memory_manager_get_pool<SeVkProgram>(&g_vulkanDevice->memoryManager);
        const SeVkProgram* program = object_pool::access(programPool, se_vk_ref_index(ref));
        return
        {
            program->reflection.computeWorkGroupSizeX,
            program->reflection.computeWorkGroupSizeY,
            program->reflection.computeWorkGroupSizeZ,
        };
    }

    SePassDependencies dependencies_for_texture(SeRenderRef texture)
    {
        return se_vk_graph_dependencies_for_texture(&g_vulkanDevice->graph, texture);
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
