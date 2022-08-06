
#include "tetris_subsystem_impl/tetris_controller.hpp"
#include "tetris_subsystem_impl/tetris_render.hpp"
#include "engine/engine.hpp"

const SeRenderAbstractionSubsystemInterface* g_render;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_init_global_subsystem_pointers(engine);
    g_render = se_get_subsystem_interface<SeRenderAbstractionSubsystemInterface>(engine);
    tetris_render_init();
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    tetris_render_terminate();
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = win::get_input();
    if (input->isCloseButtonPressed || win::is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;
    tetris_controller_update(input, info->dt);
    tetris_render_update(input, info->dt);
}

#include "tetris_subsystem_impl/tetris_controller.cpp"
#include "tetris_subsystem_impl/tetris_render.cpp"
