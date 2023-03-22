
#include "engine/se_engine.hpp"
#include "engine/se_engine.cpp"

#include "impl/tetris_controller.hpp"
#include "impl/tetris_render.hpp"

#include "impl/tetris_controller.cpp"
#include "impl/tetris_render.cpp"

void init()
{
    tetris_render_init();
}

void terminate()
{
    tetris_render_terminate();
}

void update(const SeUpdateInfo& info)
{
    if (se_win_is_close_button_pressed() || se_win_is_keyboard_button_pressed(SeKeyboard::ESCAPE)) se_engine_stop();
    tetris_controller_update(info.dt);
    tetris_render_update(info.dt);
}

int main(int argc, char* argv[])
{
    const SeSettings settings
    {
        .applicationName        = "Sabrina engine - tetris example",
        .isFullscreenWindow     = false,
        .isResizableWindow      = true,
        .windowWidth            = 640,
        .windowHeight           = 480,
        .createUserDataFolder   = false,
    };
    se_engine_run(settings, init, update, terminate);
    return 0;
}
