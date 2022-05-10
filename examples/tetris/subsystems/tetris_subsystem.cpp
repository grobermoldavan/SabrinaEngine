
#include "tetris_subsystem_impl/tetris_controller.hpp"
#include "tetris_subsystem_impl/tetris_render.hpp"
#include "engine/engine.hpp"

SeWindowHandle g_window;
const SeRenderAbstractionSubsystemInterface* g_render;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    SE_PLATFORM_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SePlatformSubsystemInterface>(engine);
    SE_APPLICATION_ALLOCATORS_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeApplicationAllocatorsSubsystemInterface>(engine);
    SE_WINDOW_SUBSYSTEM_GLOBAL_NAME = se_get_subsystem_interface<SeWindowSubsystemInterface>(engine);
    g_render = se_get_subsystem_interface<SeRenderAbstractionSubsystemInterface>(engine);

    SeWindowSubsystemCreateInfo createInfo
    {
        .name           = "Sabrina engine - tetris example",
        .isFullscreen   = false,
        .isResizable    = true,
        .width          = 640,
        .height         = 480,
    };
    g_window = win::create(createInfo);
    tetris_render_init();
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    tetris_render_terminate();
    win::destroy(g_window);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = win::get_input(g_window);
    if (input->isCloseButtonPressed || win::is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    tetris_controller_update(input, info->dt);
    tetris_render_update(input, info->dt);
}

#include "tetris_subsystem_impl/tetris_controller.cpp"
#include "tetris_subsystem_impl/tetris_render.cpp"
