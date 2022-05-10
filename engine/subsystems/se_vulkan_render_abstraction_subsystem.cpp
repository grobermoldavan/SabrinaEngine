
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
#include "engine/subsystems/se_window_subsystem.hpp"
#include "engine/subsystems/se_application_allocators_subsystem.hpp"
#include "engine/subsystems/se_platform_subsystem.hpp"
#include "engine/subsystems/se_string_subsystem.hpp"
#include "engine/engine.hpp"

static SeRenderAbstractionSubsystemInterface g_iface;

static SeFloat4x4 se_vk_perspective_projection_matrix(float fovDeg, float aspect, float nearPlane, float farPlane)
{
    //
    // https://vincent-p.github.io/posts/vulkan_perspective_matrix/
    // @NOTE : This matrix works for reverse depth (near plane = 1, far plane = 0).
    //         If you want to change this, then render pass clear values must be reevaluated
    //         as well as render pipeline depthCompareOp.
    // @NOTE : This matrix works for coordinate system used in se_math.h (forward == positive z)
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

static void se_vk_begin_pass_call(SeDeviceHandle _device, const SeBeginPassInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    se_vk_graph_begin_pass(&device->graph, info);
}

static void se_vk_end_pass_call(SeDeviceHandle _device)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    se_vk_graph_end_pass(&device->graph);
}

static SeRenderRef se_vk_program_call(SeDeviceHandle _device, const SeProgramInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    return se_vk_graph_program(&device->graph, info);
}

static SeRenderRef se_vk_texture_call(SeDeviceHandle _device, const SeTextureInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    return se_vk_graph_texture(&device->graph, info);
}

static SeRenderRef se_vk_swap_chain_texture_call(SeDeviceHandle _device)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    return se_vk_graph_swap_chain_texture(&device->graph);
}

static SeRenderRef se_vk_graphics_pipeline_call(SeDeviceHandle _device, const SeGraphicsPipelineInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    return se_vk_graph_graphics_pipeline(&device->graph, info);
}

static SeRenderRef se_vk_memory_buffer_call(SeDeviceHandle _device, const SeMemoryBufferInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    return se_vk_graph_memory_buffer(&device->graph, info);
}

static SeRenderRef se_vk_sampler_call(SeDeviceHandle _device, const SeSamplerInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    return se_vk_graph_sampler(&device->graph, info);
}

static void se_vk_command_bind_call(SeDeviceHandle _device, const SeCommandBindInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    se_vk_graph_command_bind(&device->graph, info);
}

static void se_vk_command_draw_call(SeDeviceHandle _device, const SeCommandDrawInfo& info)
{
    SeVkDevice* device = (SeVkDevice*)_device;
    se_vk_graph_command_draw(&device->graph, info);
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_iface =
    {
        .device_create                  = se_vk_device_create,
        .device_destroy                 = se_vk_device_destroy,
        .begin_frame                    = se_vk_device_begin_frame,
        .end_frame                      = se_vk_device_end_frame,
        .begin_pass                     = se_vk_begin_pass_call,
        .end_pass                       = se_vk_end_pass_call,
        .program                        = se_vk_program_call,
        .texture                        = se_vk_texture_call,
        .swap_chain_texture             = se_vk_swap_chain_texture_call,
        .graphics_pipeline              = se_vk_graphics_pipeline_call,
        .compute_pipeline               = nullptr,
        .memory_buffer                  = se_vk_memory_buffer_call,
        .sampler                        = se_vk_sampler_call,
        .bind                           = se_vk_command_bind_call,
        .draw                           = se_vk_command_draw_call,
        .dispatch                       = nullptr,
        .perspective_projection_matrix  = se_vk_perspective_projection_matrix,
    };
    SE_WINDOW_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeWindowSubsystemInterface>(engine);
    SE_APPLICATION_ALLOCATORS_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeApplicationAllocatorsSubsystemInterface>(engine);
    SE_PLATFORM_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SePlatformSubsystemInterface>(engine);
    SE_STRING_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeStringSubsystemInterface>(engine);
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_vk_check(volkInitialize());
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_iface;
}

#define VOLK_IMPLEMENTATION
#include "engine/libs/volk/volk.h"

#define SSR_DIRTY_ALLOCATOR
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
