
#include "engine/engine.hpp"
#include "engine/engine.cpp"

DataProvider g_fontDataEnglish;
DataProvider g_fontDataRussian;

SeFolderHandle g_folder = {};

void init()
{
    g_fontDataEnglish = data_provider::from_file("shahd serif.ttf");
    g_fontDataRussian = data_provider::from_file("a_antiquetrady regular.ttf");
}

void terminate()
{
    
}

void update(const SeUpdateInfo& info)
{
    if (win::is_close_button_pressed()) engine::stop();
    if (render::begin_frame())
    {
        if (ui::begin({ render::swap_chain_texture(), SeRenderTargetLoadOp::CLEAR, { 0.0f, 0.0f, 0.0f, 1.0f } }))
        {
            ui::set_font_group({ g_fontDataEnglish, g_fontDataRussian });

            ui::set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::BOTTOM_LEFT });
            ui::set_param(SeUiParam::PIVOT_TYPE_Y, { .pivot = SeUiPivotType::BOTTOM_LEFT });
            ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 0.0f });
            ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 0.0f });
            ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = 20.0f });
            ui::set_param(SeUiParam::FONT_LINE_STEP, { .dim = 22.0f });

            if (ui::begin_window
            ({
                .uid    = "Main window",
                .width  = win::get_width<float>(),
                .height = win::get_height<float>(),
                .flags  = 0,
            }))
            {
                if (ui::button
                ({
                    .uid        = "back",
                    .utf8text   = "back",
                    .width      = 0,
                    .height     = 0,
                    .mode       = SeUiButtonMode::CLICK,
                }))
                {
                    if (g_folder) g_folder = fs::parent_folder(g_folder);
                }

                if (!g_folder)
                {
                    if (ui::button
                    ({
                        .uid        = fs::full_path(SE_APPLICATION_FOLDER),
                        .utf8text   = fs::full_path(SE_APPLICATION_FOLDER),
                        .width      = 0,
                        .height     = 0,
                        .mode       = SeUiButtonMode::CLICK,
                    }))
                    {
                        g_folder = SE_APPLICATION_FOLDER;
                    }
                    if (fs::full_path(SE_USER_DATA_FOLDER))
                    {
                        if (ui::button
                        ({
                            .uid        = fs::full_path(SE_USER_DATA_FOLDER),
                            .utf8text   = fs::full_path(SE_USER_DATA_FOLDER),
                            .width      = 0,
                            .height     = 0,
                            .mode       = SeUiButtonMode::CLICK,
                        }))
                        {
                            g_folder = SE_USER_DATA_FOLDER;
                        }
                    }
                }
                else
                {
                    ui::text({ .utf8text = fs::full_path(g_folder) });
                    for (SeFileHandle file : SeFileIterator{ g_folder })
                    {
                        ui::text({ .utf8text = fs::full_path(file) });
                    }
                    for (SeFolderHandle folder : SeFolderIterator{ g_folder, true })
                    {
                        const char* fullPath = fs::full_path(folder);
                        if (ui::button
                        ({
                            .uid        = fullPath,
                            .utf8text   = fullPath,
                            .width      = 0,
                            .height     = 0,
                            .mode       = SeUiButtonMode::CLICK,
                        }))
                        {
                            g_folder = folder;
                        }
                        for (SeFileHandle file : SeFileIterator{ folder })
                        {
                            ui::text({ .utf8text = fs::full_path(file) });
                        }
                    }
                }
                ui::end_window();
            }

            ui::end(0);
        }
        render::end_frame();
    }

}

int main(int argc, char* argv[])
{
    // This application will create a new folder in "C:\Users\*USER_NAME*\AppData\Local\" (or other default app data folder)
    // Folder name will be "Sabrina engine - file system api"
    // Folder creation is controlled by SeSettings::createUserDataFolder parameter
    const SeSettings settings
    {
        .applicationName        = "Sabrina engine - file system api",
        .isFullscreenWindow     = false,
        .isResizableWindow      = true,
        .windowWidth            = 640,
        .windowHeight           = 480,
        .createUserDataFolder   = true,
    };
    engine::run(settings, init, update, terminate);
    return 0;
}
