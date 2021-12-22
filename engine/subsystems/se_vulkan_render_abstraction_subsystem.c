
#include "se_vulkan_render_abstraction_subsystem.h"
#include "vulkan/se_vulkan_render_subsystem_base.h"
#include "vulkan/se_vulkan_render_subsystem_utils.h"
#include "vulkan/se_vulkan_render_subsystem_memory.h"
#include "vulkan/se_vulkan_render_subsystem_in_flight_manager.h"
#include "vulkan/se_vulkan_render_subsystem_device.h"
#include "vulkan/se_vulkan_render_subsystem_texture.h"
#include "vulkan/se_vulkan_render_subsystem_command_buffer.h"
#include "vulkan/se_vulkan_render_subsystem_render_pass.h"
#include "vulkan/se_vulkan_render_subsystem_render_program.h"
#include "vulkan/se_vulkan_render_subsystem_render_pipeline.h"
#include "vulkan/se_vulkan_render_subsystem_framebuffer.h"
#include "vulkan/se_vulkan_render_subsystem_memory_buffer.h"
#include "vulkan/se_vulkan_render_subsystem_resource_set.h"
#include "engine/engine.h"

static SeRenderAbstractionSubsystemInterface g_Iface;
static SeWindowSubsystemInterface* g_windowIface;

static void se_vk_perspective_projection_matrix(SeFloat4x4* result, float fovDeg, float aspect, float nearPlane, float farPlane)
{
    //
    // https://vincent-p.github.io/posts/vulkan_perspective_matrix/
    // @NOTE : This matrix works for reverse depth (near plane = 1, far plane = 0).
    //         If you want to change this, then render pass clear values must be reevaluated
    //         as well as render pipeline depthCompareOp.
    // @NOTE : This matrix works for coordinate system used in se_math.h (forward == positive z)
    //
    const float focalLength = 1.0f / tan(se_to_radians(fovDeg) / 2.0f);
    const float x  =  focalLength / aspect;
    const float y  = -focalLength;
    const float A  = nearPlane / (nearPlane - farPlane);
    const float B  = A * -farPlane;
    *result = (SeFloat4x4)
    {
        x,    0.0f,  0.0f, 0.0f,
        0.0f,    y,  0.0f, 0.0f,
        0.0f, 0.0f,     A,    B,
        0.0f, 0.0f,  1.0f, 0.0f,
    };
}

SE_DLL_EXPORT void se_load(SabrinaEngine* engine)
{
    g_Iface = (SeRenderAbstractionSubsystemInterface)
    {
        .device_create                          = se_vk_device_create,
        .device_wait                            = se_vk_device_wait,
        .get_swap_chain_textures_num            = se_vk_device_get_swap_chain_textures_num,
        .get_swap_chain_texture                 = se_vk_device_get_swap_chain_texture,
        .get_active_swap_chain_texture_index    = se_vk_device_get_active_swap_chain_texture_index,
        .begin_frame                            = se_vk_device_begin_frame,
        .end_frame                              = se_vk_device_end_frame,
        .program_create                         = se_vk_render_program_create,
        .texture_create                         = se_vk_texture_create,
        .texture_get_width                      = se_vk_texture_get_width,
        .texture_get_height                     = se_vk_texture_get_height,
        .render_pass_create                     = se_vk_render_pass_create,
        .render_pipeline_graphics_create        = se_vk_render_pipeline_graphics_create,
        .framebuffer_create                     = se_vk_framebuffer_create,
        .resource_set_create                    = se_vk_resource_set_create,
        .memory_buffer_create                   = se_vk_memory_buffer_create,
        .memory_buffer_get_mapped_address       = se_vk_memory_buffer_get_mapped_address,
        .command_buffer_request                 = se_vk_command_buffer_request,
        .command_buffer_submit                  = se_vk_command_buffer_submit,
        .command_bind_pipeline                  = se_vk_command_buffer_bind_pipeline,
        .command_draw                           = se_vk_command_buffer_draw,
        .command_bind_resource_set              = se_vk_command_bind_resource_set,
        .perspective_projection_matrix          = se_vk_perspective_projection_matrix,
    };
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_vk_check(volkInitialize());
    g_windowIface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_Iface;
}

#define VOLK_IMPLEMENTATION
#include "engine/libs/volk/volk.h"

#define SSR_DIRTY_ALLOCATOR
#define SSR_IMPL
#include "engine/libs/ssr/simple_spirv_reflection.h"

#include "vulkan/se_vulkan_render_subsystem_utils.c"
#include "vulkan/se_vulkan_render_subsystem_memory.c"
#include "vulkan/se_vulkan_render_subsystem_in_flight_manager.c"
#include "vulkan/se_vulkan_render_subsystem_device.c"
#include "vulkan/se_vulkan_render_subsystem_texture.c"
#include "vulkan/se_vulkan_render_subsystem_command_buffer.c"
#include "vulkan/se_vulkan_render_subsystem_render_pass.c"
#include "vulkan/se_vulkan_render_subsystem_render_program.c"
#include "vulkan/se_vulkan_render_subsystem_render_pipeline.c"
#include "vulkan/se_vulkan_render_subsystem_framebuffer.c"
#include "vulkan/se_vulkan_render_subsystem_memory_buffer.c"
#include "vulkan/se_vulkan_render_subsystem_resource_set.c"
