
#include "engine/se_engine.hpp"
#include "engine/se_engine.cpp"

SeDataProvider g_fontDataEnglish;
SeDataProvider g_fontDataRussian;

SeFolderHandle g_folder = {};

void init()
{
    g_fontDataEnglish = se_data_provider_from_file("shahd serif.ttf");
    g_fontDataRussian = se_data_provider_from_file("a_antiquetrady regular.ttf");
}

void terminate()
{
    
}

void update(const SeUpdateInfo& info)
{
    if (se_win_is_close_button_pressed()) se_engine_stop();
    if (se_render_begin_frame())
    {
        if (se_ui_begin({ se_render_swap_chain_texture(), SeRenderTargetLoadOp::CLEAR, { 0.0f, 0.0f, 0.0f, 1.0f } }))
        {
            se_ui_set_font_group({ g_fontDataEnglish, g_fontDataRussian });

            se_ui_set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
            se_ui_set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
            se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 0.0f });
            se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 0.0f });
            se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = 20.0f });
            se_ui_set_param(SeUiParam::FONT_LINE_GAP, { .dim = 2.0f });

            if (se_ui_begin_window
            ({
                .uid    = "Main window",
                .width  = se_win_get_width<float>(),
                .height = se_win_get_height<float>(),
                .flags  = 0,
            }))
            {
                if (se_ui_button
                ({
                    .uid        = "back",
                    .utf8text   = "back",
                    .width      = 0,
                    .height     = 0,
                    .mode       = SeUiButtonMode::CLICK,
                }))
                {
                    if (g_folder) g_folder = se_fs_parent_folder(g_folder);
                }

                if (!g_folder)
                {
                    if (se_ui_button
                    ({
                        .uid        = se_fs_full_path(SE_APPLICATION_FOLDER),
                        .utf8text   = se_fs_full_path(SE_APPLICATION_FOLDER),
                        .width      = 0,
                        .height     = 0,
                        .mode       = SeUiButtonMode::CLICK,
                    }))
                    {
                        g_folder = SE_APPLICATION_FOLDER;
                    }
                    if (se_fs_full_path(SE_USER_DATA_FOLDER))
                    {
                        if (se_ui_button
                        ({
                            .uid        = se_fs_full_path(SE_USER_DATA_FOLDER),
                            .utf8text   = se_fs_full_path(SE_USER_DATA_FOLDER),
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
                    se_ui_text({ .utf8text = se_fs_full_path(g_folder) });
                    for (SeFileHandle file : SeFileIterator{ g_folder })
                    {
                        se_ui_text({ .utf8text = se_fs_full_path(file) });
                    }
                    for (SeFolderHandle folder : SeFolderIterator{ g_folder, true })
                    {
                        const char* fullPath = se_fs_full_path(folder);
                        if (se_ui_button
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
                            se_ui_text({ .utf8text = se_fs_full_path(file) });
                        }
                    }
                }
                se_ui_end_window();
            }

            se_ui_end(0);
        }
        se_render_end_frame();
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
    se_engine_run(settings, init, update, terminate);
    return 0;
}
