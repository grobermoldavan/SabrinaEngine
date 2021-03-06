
#include "ui_test_subsystem.hpp"
#include "engine/engine.hpp"

const SeRenderAbstractionSubsystemInterface*    g_render;
SeWindowHandle                                  g_window;
SeDeviceHandle                                  g_device;
DataProvider                                    g_presentVsData;
DataProvider                                    g_presentFsData;
DataProvider                                    g_fontDataEnglish;
DataProvider                                    g_fontDataRussian;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_init_global_subsystem_pointers(engine);
    g_render = se_get_subsystem_interface<SeRenderAbstractionSubsystemInterface>(engine);
    g_window = win::create
    ({
        .name           = "Sabrina engine - ui example",
        .isFullscreen   = false,
        .isResizable    = true,
        .width          = 640,
        .height         = 480,
    });
    g_device = g_render->device_create({ .window = g_window });
    g_presentVsData = data_provider::from_file("assets/default/shaders/present.vert.spv");
    g_presentFsData = data_provider::from_file("assets/default/shaders/present.frag.spv");
    g_fontDataEnglish = data_provider::from_file("assets/default/fonts/shahd serif.ttf");
    g_fontDataRussian = data_provider::from_file("assets/default/fonts/a_antiquetrady regular.ttf");
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    g_render->device_destroy(g_device);
    win::destroy(g_window);
    data_provider::destroy(g_presentVsData);
    data_provider::destroy(g_presentFsData);
    data_provider::destroy(g_fontDataEnglish);
    data_provider::destroy(g_fontDataRussian);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const UpdateInfo* info)
{
    const SeWindowSubsystemInput* input = win::get_input(g_window);
    if (input->isCloseButtonPressed || win::is_keyboard_button_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    static size_t frameIndex = 0;
    frameIndex += 1;

    g_render->begin_frame(g_device);
    {
        if (ui::begin({
            "main",
            g_render,
            g_device,
            { g_render->swap_chain_texture(g_device), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR },
            g_window,
        }))
        {
            //
            // Set font (in this case we combine two fonts into one atlas)
            //
            ui::set_font_group({ g_fontDataEnglish, g_fontDataRussian });
            ui::set_style_param(SeUiStyleParam::FONT_HEIGHT, { .dim = ui_dim::pix(20) });
            ui::set_style_param(SeUiStyleParam::FONT_LINE_STEP, { .dim = ui_dim::pix(25) });

            //
            // Windows
            //
            if (ui::begin_window({
                .uid            = "first_window",
                .bottomLeftX    = ui_dim::pix(100),
                .bottomLeftY    = ui_dim::pix(100),
                .topRightX      = ui_dim::pix(400),
                .topRightY      = ui_dim::pix(400),
                .flags          = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
            }))
            {
                ui::window_text({ "TEXT IN WINDOW, text in window yyyiii, TEXT IN WINDOW, TEXT IN WINDOW" });
                ui::window_text({ "AMOGUS AMOGUS AMOGUS" });
                ui::end_window();
            }

            ui::set_style_param(SeUiStyleParam::PRIMARY_COLOR, { .color = col::pack({ 0.73f, 0.11f, 0.14f, 1.0f }) });
            ui::set_style_param(SeUiStyleParam::SECONDARY_COLOR, { .color = col::pack({ 0.09f, 0.01f, 0.01f, 0.7f }) });
            ui::set_style_param(SeUiStyleParam::ACCENT_COLOR, { .color = col::pack({ 0.88f, 0.73f, 0.73f, 1.0f }) });
            ui::set_style_param(SeUiStyleParam::WINDOW_TOP_PANEL_THICKNESS, { .dim = ui_dim::pix(10.0f) });
            ui::set_style_param(SeUiStyleParam::WINDOW_BORDER_THICKNESS, { .dim = ui_dim::pix(3.0f) });

            if (ui::begin_window({
                .uid            = "second_window",
                .bottomLeftX    = ui_dim::pix(250),
                .bottomLeftY    = ui_dim::pix(250),
                .topRightX      = ui_dim::pix(600),
                .topRightY      = ui_dim::pix(600),
                .flags          = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
            }))
            {
                ui::window_text({ "ТЕКСТ В ОКНЕ, текст в окне уууй, ТЕКСТ В ОКНЕ, ТЕКСТ В ОКНЕ" });
                ui::end_window();
            }

            //
            // Text lines
            //
            SeString str;
            {
                SeStringBuilder builder = string_builder::begin();
                string_builder::append(builder, "Frame (кадр) : ");
                string_builder::append(builder, string::cast(frameIndex));
                str = string_builder::end(builder);
            }
            ui::set_style_param(SeUiStyleParam::FONT_COLOR, { .color = col::pack({ 0.0f, 1.0f, 0.0f, 1.0f }) });
            ui::text_line({ string::cstr(str), ui_dim::pix(100), ui_dim::pix(100) });

            ui::set_style_param(SeUiStyleParam::FONT_COLOR, { .color = col::pack({ 1.0f, 1.0f, 0.0f, 1.0f }) });
            ui::text_line({ string::cstr(str), ui_dim::pix(100), ui_dim::pix(200) });

            ui::set_style_param(SeUiStyleParam::FONT_COLOR, { .color = col::pack({ 1.0f, 0.0f, 0.0f, 1.0f }) });
            ui::text_line({ string::cstr(str), ui_dim::pix(100), ui_dim::pix(300) });

            ui::end();
        }
    }
    g_render->end_frame(g_device);
}
