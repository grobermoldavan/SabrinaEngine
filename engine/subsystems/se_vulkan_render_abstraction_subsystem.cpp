
#include "se_vulkan_render_abstraction_subsystem.hpp"
#include "vulkan/se_vulkan_render_subsystem_base.hpp"
#include "vulkan/se_vulkan_render_subsystem_device.hpp"
#include "vulkan/se_vulkan_render_subsystem_frame_manager.hpp"
#include "vulkan/se_vulkan_render_subsystem_framebuffer.hpp"
#include "vulkan/se_vulkan_render_subsystem_graph.hpp"
#include "vulkan/se_vulkan_render_subsystem_memory_buffer.hpp"
#include "vulkan/se_vulkan_render_subsystem_memory.hpp"
#include "vulkan/se_vulkan_render_subsystem_pipeline.hpp"
#include "vulkan/se_vulkan_render_subsystem_program.hpp"
#include "vulkan/se_vulkan_render_subsystem_render_pass.hpp"
#include "vulkan/se_vulkan_render_subsystem_sampler.hpp"
#include "vulkan/se_vulkan_render_subsystem_texture.hpp"
#include "vulkan/se_vulkan_render_subsystem_command_buffer.hpp"
#include "vulkan/se_vulkan_render_subsystem_utils.hpp"
#include "engine/engine.hpp"

SeRenderAbstractionSubsystemInterface g_iface;
SeVkDevice* g_device;

SeFloat4x4 se_vk_perspective_projection_matrix(float fovDeg, float aspect, float nearPlane, float farPlane)
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

SeFloat4x4 se_vk_orthographic_projection_matrix(float left, float right, float bottom, float top, float nearPlane, float farPlane)
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

bool se_vk_begin_frame_call()
{
    const uint32_t width = win::get_width();
    const uint32_t height = win::get_height();
    if (width == 0 && height == 0)
    {
        return false;
    }
    se_vk_device_begin_frame(g_device, { width, height });
    return true;
}

void se_vk_end_frame_call()
{
    se_vk_device_end_frame(g_device);
}

void se_vk_begin_pass_call(const SeBeginPassInfo& info)
{
    se_vk_graph_begin_pass(&g_device->graph, info);
}

void se_vk_end_pass_call()
{
    se_vk_graph_end_pass(&g_device->graph);
}

SeRenderRef se_vk_program_call(const SeProgramInfo& info)
{
    return se_vk_graph_program(&g_device->graph, info);
}

SeRenderRef se_vk_texture_call(const SeTextureInfo& info)
{
    return se_vk_graph_texture(&g_device->graph, info);
}

SeRenderRef se_vk_swap_chain_texture_call()
{
    return se_vk_graph_swap_chain_texture(&g_device->graph);
}

SeTextureSize se_vk_texture_size_call(SeRenderRef texture)
{
    return se_vk_grap_texture_size(&g_device->graph, texture);
}

SeRenderRef se_vk_graphics_pipeline_call(const SeGraphicsPipelineInfo& info)
{
    return se_vk_graph_graphics_pipeline(&g_device->graph, info);
}

SeRenderRef se_vk_graphics_pipeline_call(const SeComputePipelineInfo& info)
{
    return se_vk_graph_compute_pipeline(&g_device->graph, info);
}

SeRenderRef se_vk_memory_buffer_call(const SeMemoryBufferInfo& info)
{
    return se_vk_graph_memory_buffer(&g_device->graph, info);
}

SeRenderRef se_vk_sampler_call(const SeSamplerInfo& info)
{
    return se_vk_graph_sampler(&g_device->graph, info);
}

void se_vk_command_bind_call(const SeCommandBindInfo& info)
{
    se_vk_graph_command_bind(&g_device->graph, info);
}

void se_vk_command_draw_call(const SeCommandDrawInfo& info)
{
    se_vk_graph_command_draw(&g_device->graph, info);
}

void se_vk_command_dispatch_call(const SeCommandDispatchInfo& info)
{
    se_vk_graph_command_dispatch(&g_device->graph, info);
}

SeComputeWorkgroupSize se_vk_compute_workgroup_size_call(SeRenderRef ref)
{
    const ObjectPool<SeVkProgram>& programPool = se_vk_memory_manager_get_pool<SeVkProgram>(&g_device->memoryManager);
    const SeVkProgram* program = object_pool::access(programPool, se_vk_ref_index(ref));
    return
    {
        program->reflection.computeWorkGroupSizeX,
        program->reflection.computeWorkGroupSizeY,
        program->reflection.computeWorkGroupSizeZ,
    };
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .begin_frame        = se_vk_begin_frame_call,
        .end_frame          = se_vk_end_frame_call,
        .begin_pass         = se_vk_begin_pass_call,
        .end_pass           = se_vk_end_pass_call,
        .program            = se_vk_program_call,
        .texture            = se_vk_texture_call,
        .swap_chain_texture = se_vk_swap_chain_texture_call,
        .texture_size       = se_vk_texture_size_call,
        .graphics_pipeline  = se_vk_graphics_pipeline_call,
        .compute_pipeline   = se_vk_graphics_pipeline_call,
        .memory_buffer      = se_vk_memory_buffer_call,
        .sampler            = se_vk_sampler_call,
        .bind               = se_vk_command_bind_call,
        .draw               = se_vk_command_draw_call,
        .dispatch           = se_vk_command_dispatch_call,
        .perspective        = se_vk_perspective_projection_matrix,
        .orthographic       = se_vk_orthographic_projection_matrix,
        .workgroup_size     = se_vk_compute_workgroup_size_call,
    };
    se_init_global_subsystem_pointers(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine, const SeEngineSettings* settings)
{
    se_vk_check(volkInitialize());
    g_device = se_vk_device_create(win::get_native_handle());
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    se_vk_device_destroy(g_device);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

#define VOLK_IMPLEMENTATION
#include "engine/libs/volk/volk.h"

#define SSR_IMPL
#include "engine/libs/ssr/simple_spirv_reflection.h"

#include "vulkan/se_vulkan_render_subsystem_device.cpp"
#include "vulkan/se_vulkan_render_subsystem_frame_manager.cpp"
#include "vulkan/se_vulkan_render_subsystem_framebuffer.cpp"
#include "vulkan/se_vulkan_render_subsystem_graph.cpp"
#include "vulkan/se_vulkan_render_subsystem_memory_buffer.cpp"
#include "vulkan/se_vulkan_render_subsystem_memory.cpp"
#include "vulkan/se_vulkan_render_subsystem_pipeline.cpp"
#include "vulkan/se_vulkan_render_subsystem_program.cpp"
#include "vulkan/se_vulkan_render_subsystem_render_pass.cpp"
#include "vulkan/se_vulkan_render_subsystem_sampler.cpp"
#include "vulkan/se_vulkan_render_subsystem_texture.cpp"
#include "vulkan/se_vulkan_render_subsystem_command_buffer.cpp"
#include "vulkan/se_vulkan_render_subsystem_utils.cpp"
