
#include "ui_test_subsystem.hpp"
#include "engine/engine.hpp"

const SeRenderAbstractionSubsystemInterface*    g_render;
const SeUiSubsystemInterface*                   g_ui;

DataProvider                                    g_presentVsData;
DataProvider                                    g_presentFsData;
DataProvider                                    g_fontDataEnglish;
DataProvider                                    g_fontDataRussian;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_init_global_subsystem_pointers(engine);
    g_render = se_get_subsystem_interface<SeRenderAbstractionSubsystemInterface>(engine);
    g_ui = se_get_subsystem_interface<SeUiSubsystemInterface>(engine);

    g_presentVsData = data_provider::from_file("assets/default/shaders/present.vert.spv");
    g_presentFsData = data_provider::from_file("assets/default/shaders/present.frag.spv");
    g_fontDataEnglish = data_provider::from_file("assets/default/fonts/shahd serif.ttf");
    g_fontDataRussian = data_provider::from_file("assets/default/fonts/a_antiquetrady regular.ttf");
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    data_provider::destroy(g_presentVsData);
    data_provider::destroy(g_presentFsData);
    data_provider::destroy(g_fontDataEnglish);
    data_provider::destroy(g_fontDataRussian);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = win::get_input();
    if (input->isCloseButtonPressed || win::is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    static size_t frameIndex = 0;
    frameIndex += 1;

    if (g_render->begin_frame())
    {
        if (g_ui->begin_ui({ g_render, { g_render->swap_chain_texture(), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } }))
        {
            //
            // Set font (in this case we combine two fonts into one atlas)
            //
            g_ui->set_font_group({ g_fontDataEnglish, g_fontDataRussian });
            g_ui->set_param(SeUiParam::FONT_HEIGHT, { .dim = 20.0f });
            g_ui->set_param(SeUiParam::FONT_LINE_STEP, { .dim = 25.0f });

            //
            // Windows
            //
            g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 100.0f });
            g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 100.0f });
            if (g_ui->begin_window({
                .uid    = "first_window",
                .width  = 300.0f,
                .height = 300.0f,
                .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
            }))
            {
                g_ui->text({ "TEXT IN WINDOW, text in window yyyiii, TEXT IN WINDOW, TEXT IN WINDOW" });
                g_ui->text({ "AMOGUS AMOGUS AMOGUS" });
                g_ui->end_window();
            }

            g_ui->set_param(SeUiParam::PRIMARY_COLOR, { .color = col::pack({ 0.73f, 0.11f, 0.14f, 1.0f }) });
            g_ui->set_param(SeUiParam::SECONDARY_COLOR, { .color = col::pack({ 0.09f, 0.01f, 0.01f, 0.7f }) });
            g_ui->set_param(SeUiParam::ACCENT_COLOR, { .color = col::pack({ 0.88f, 0.73f, 0.73f, 1.0f }) });
            g_ui->set_param(SeUiParam::WINDOW_TOP_PANEL_THICKNESS, { .dim = 10.0f });
            g_ui->set_param(SeUiParam::WINDOW_BORDER_THICKNESS, { .dim = 3.0f });
            // This window will be centered around pivot position
            g_ui->set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::CENTER });
            g_ui->set_param(SeUiParam::PIVOT_TYPE_Y, { .pivot = SeUiPivotType::CENTER });
            g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 400.0f });
            g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 400.0f });

            if (g_ui->begin_window({
                .uid    = "second_window",
                .width  = 300.0f,
                .height = 300.0f,
                .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
            }))
            {
                g_ui->text({ "ТЕКСТ В ОКНЕ, текст в окне уууй, ТЕКСТ В ОКНЕ, ТЕКСТ В ОКНЕ" });
                g_ui->end_window();
            }

            //
            // Text lines
            //
            SeString str = string::create_fmt(SeStringLifetime::Temporary, "Frame (кадр) : {}", frameIndex);

            g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 100.0f });
            g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 100.0f });
            g_ui->set_param(SeUiParam::FONT_COLOR, { .color = col::pack({ 0.0f, 1.0f, 0.0f, 1.0f }) });
            g_ui->text({ string::cstr(str) });

            g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 100.0f });
            g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 200.0f });
            g_ui->set_param(SeUiParam::FONT_COLOR, { .color = col::pack({ 1.0f, 1.0f, 0.0f, 1.0f }) });
            g_ui->text({ string::cstr(str) });

            g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 100.0f });
            g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 300.0f });
            g_ui->set_param(SeUiParam::FONT_COLOR, { .color = col::pack({ 1.0f, 0.0f, 0.0f, 1.0f }) });
            g_ui->text({ string::cstr(str) });

            g_ui->end_ui(0);
        }
        g_render->end_frame();
    }
}
