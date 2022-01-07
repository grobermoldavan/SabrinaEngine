
#include "tetris_subsystem_impl/tetris_controller.h"
#include "tetris_subsystem_impl/tetris_render.h"
#include "engine/engine.h"

SeWindowSubsystemInterface* windowInterface;

SeWindowHandle windowHandle;
TetrisState tetrisState = {0};
TetrisRenderState tetrisRenderState = {0};

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    windowInterface = (SeWindowSubsystemInterface*)engine->find_subsystem_interface(engine, SE_WINDOW_SUBSYSTEM_NAME);
    SeWindowSubsystemCreateInfo createInfo = (SeWindowSubsystemCreateInfo)
    {
        .name           = "Sabrina engine - tetris example",
        .isFullscreen   = false,
        .isResizable    = true,
        .width          = 640,
        .height         = 480,
    };
    windowHandle = windowInterface->create(&createInfo);
    tetris_render_init(&((TetrisRenderInitInfo){ &tetrisRenderState, &windowHandle, engine }));
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    tetris_render_terminate(&tetrisRenderState);
    windowInterface->destroy(windowHandle);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = windowInterface->get_input(windowHandle);
    if (input->isCloseButtonPressed || se_is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    tetris_controller_update(&tetrisState, input, info->dt);
    tetris_render_update(&tetrisRenderState, &tetrisState, input, info->dt);
}

#include "tetris_subsystem_impl/tetris_controller.c"
#include "tetris_subsystem_impl/tetris_render.c"
