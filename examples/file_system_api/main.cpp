
#include "engine/engine.hpp"
#include "engine/engine.cpp"

DataProvider g_fontDataEnglish;
DataProvider g_fontDataRussian;

const size_t FOLDER_STACK_SIZE = 32;
SeFolderHandle g_folderStack[FOLDER_STACK_SIZE];
size_t g_folderStackTop = 0;

void folder_stack_push(SeFolderHandle folder)
{
    g_folderStack[g_folderStackTop++] = folder;
}

void folder_stack_pop()
{
    if (!g_folderStackTop) return;
    g_folderStackTop -= 1;
}

SeFolderHandle folder_stack_top()
{
    se_assert(g_folderStackTop);
    return g_folderStack[g_folderStackTop - 1];
}

void init()
{
    g_fontDataEnglish = data_provider::from_file("shahd serif.ttf");
    g_fontDataRussian = data_provider::from_file("a_antiquetrady regular.ttf");
    folder_stack_push(SE_APPLICATION_FOLDER);
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
                    folder_stack_pop();
                }

                if (g_folderStackTop == 0)
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
                        folder_stack_push(SE_APPLICATION_FOLDER);
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
                            folder_stack_push(SE_USER_DATA_FOLDER);
                        }
                    }
                }
                else
                {
                    const SeFolderHandle currentFolder = folder_stack_top();
                    ui::text({ .utf8text = fs::full_path(currentFolder) });
                    for (SeFileHandle file : SeFileIterator{ currentFolder })
                    {
                        ui::text({ .utf8text = fs::full_path(file) });
                    }
                    for (SeFolderHandle folder : SeFolderIterator{ currentFolder, true })
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
                            folder_stack_push(folder);
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
