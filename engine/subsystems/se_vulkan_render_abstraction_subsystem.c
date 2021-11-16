
#include "se_vulkan_render_abstraction_subsystem.h"
#include "vulkan/se_vulkan_render_subsystem_base.h"
#include "vulkan/se_vulkan_render_subsystem_utils.h"
#include "vulkan/se_vulkan_render_subsystem_memory.h"
#include "vulkan/se_vulkan_render_subsystem_device.h"
#include "vulkan/se_vulkan_render_subsystem_texture.h"
#include "vulkan/se_vulkan_render_subsystem_command_buffer.h"
#include "vulkan/se_vulkan_render_subsystem_render_pass.h"
#include "engine/engine.h"

#ifdef _WIN32
#   define SE_VULKAN_IFACE_FUNC __declspec(dllexport)
#else
#   error Unsupported platform
#endif

static SeRenderAbstractionSubsystemInterface g_Iface;
static SabrinaEngine* g_Engine;
static SeWindowSubsystemInterface* g_windowIface;

SE_VULKAN_IFACE_FUNC void se_load(SabrinaEngine* engine)
{
    g_Engine = engine;
    g_Iface = (SeRenderAbstractionSubsystemInterface)
    {
        .device_create                          = se_vk_device_create,
        .device_wait                            = se_vk_device_wait,
        .get_swap_chain_textures_num            = se_vk_device_get_swap_chain_textures_num,
        .get_swap_chain_texture                 = se_vk_device_get_swap_chain_texture,
        .get_active_swap_chain_texture_index    = NULL,
        .begin_frame                            = NULL,
        .end_frame                              = NULL,
        .program_create                         = NULL,
        .texture_create                         = se_vk_texture_create,
        .render_pass_create                     = se_vk_render_pass_create,
        .render_pipeline_graphics_create        = NULL,
        .framebuffer_create                     = NULL,
        .command_buffer_request                 = NULL,
        .command_buffer_submit                  = NULL,
        .command_bind_pipeline                  = NULL,
        .command_draw                           = NULL,
    };
}

SE_VULKAN_IFACE_FUNC void se_init(SabrinaEngine* engine)
{
    se_vk_check(volkInitialize());
    g_windowIface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
}

SE_VULKAN_IFACE_FUNC void* se_get_interface(SabrinaEngine* engine)
{
    return &g_Iface;
}

#define VOLK_IMPLEMENTATION
#include "engine/libs/volk/volk.h"

#include "vulkan/se_vulkan_render_subsystem_utils.c"
#include "vulkan/se_vulkan_render_subsystem_memory.c"
#include "vulkan/se_vulkan_render_subsystem_device.c"
#include "vulkan/se_vulkan_render_subsystem_texture.c"
#include "vulkan/se_vulkan_render_subsystem_command_buffer.c"
#include "vulkan/se_vulkan_render_subsystem_render_pass.c"

#define SE_DEBUG_IMPL
#include "engine/debug.h"

#define SE_CONTAINERS_IMPL
#include "engine/containers.h"
