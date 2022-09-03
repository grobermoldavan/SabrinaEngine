
#include "ui_test_subsystem.hpp"
#include "engine/engine.hpp"

const SeRenderAbstractionSubsystemInterface*    g_render;
const SeUiSubsystemInterface*                   g_ui;

DataProvider                                    g_fontDataEnglish;
DataProvider                                    g_fontDataRussian;

enum struct MenuState
{
    MAIN,
    TEXT_EXAMPLE,
    WINDOW_EXAMPLE,
    BUTTON_EXAMPLE,
} g_menuState = MenuState::MAIN;

void update_main_menu(SabrinaEngine* engine)
{
    const SeWindowSubsystemInput* input = win::get_input();
    if (win::is_keyboard_button_just_pressed(input, SE_ESCAPE)) engine->shouldRun = false;

    constexpr float FONT_HEIGHT = 80.0f;

    g_ui->set_param(SeUiParam::FONT_HEIGHT, { .dim = FONT_HEIGHT });
    g_ui->set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::CENTER });
    g_ui->set_param(SeUiParam::PIVOT_TYPE_Y, { .pivot = SeUiPivotType::CENTER });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 2.0f });

    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() / 2.0f + FONT_HEIGHT * 2.0f });
    if (g_ui->button({ "mm_to_text", "Text examples", 0, 0, SeUiButtonMode::HOLD }))
    {
        g_menuState = MenuState::TEXT_EXAMPLE;
    }

    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() / 2.0f });
    if (g_ui->button({ "mm_to_window", "Window examples", 0, 0, SeUiButtonMode::HOLD }))
    {
        g_menuState = MenuState::WINDOW_EXAMPLE;
    }

    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() / 2.0f - FONT_HEIGHT * 2.0f });
    if (g_ui->button({ "mm_to_button", "Button examples", 0, 0, SeUiButtonMode::HOLD }))
    {
        g_menuState = MenuState::BUTTON_EXAMPLE;
    }
}

void update_text_example(float dt)
{
    using TimeMs = uint64_t;

    const SeWindowSubsystemInput* input = win::get_input();
    if (win::is_keyboard_button_just_pressed(input, SE_ESCAPE)) g_menuState = MenuState::MAIN;

    constexpr size_t    NUM_LINES           = 8;
    constexpr float     BASE_FONT_HEIGHT    = 80.0f;
    constexpr float     FONT_HEIGHT_STEP    = 0.8f;
    constexpr float     ADDITIONAL_STEP     = 5.0f;
    constexpr TimeMs    COLOR_INTERVAL_MS   = 2000;
    constexpr float     COLOR_TIME_SCALE    = 1.2f;
    constexpr ColorUnpacked COLORS[] =
    {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f, 1.0f },
        { 0.2f, 1.0f, 1.0f, 1.0f },
    };
    constexpr size_t NUM_COLORS = se_array_size(COLORS);
    constexpr TimeMs MAX_COLOR_TIME_MS = NUM_COLORS * COLOR_INTERVAL_MS;

    g_ui->set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::BOTTOM_LEFT });
    g_ui->set_param(SeUiParam::PIVOT_TYPE_Y, { .pivot = SeUiPivotType::BOTTOM_LEFT });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 50.0f });

    float positionY = 40.0f;
    float fontHeight = BASE_FONT_HEIGHT;
    float colorDt = dt;
    for (size_t it = 0; it < NUM_LINES; it++)
    {
        g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = positionY });
        g_ui->set_param(SeUiParam::FONT_HEIGHT, { .dim = fontHeight });

        static TimeMs colorTimes[NUM_LINES] = { };
        colorTimes[it] += TimeMs(colorDt * 1000.0f);

        // Lerp colors for fun
        const TimeMs        clampedTime = colorTimes[it] % MAX_COLOR_TIME_MS;
        const size_t        colorIndex  = clampedTime / COLOR_INTERVAL_MS;
        const float         lerpValue   = float(clampedTime % COLOR_INTERVAL_MS) / float(COLOR_INTERVAL_MS);
        const ColorUnpacked colorFrom   = COLORS[colorIndex];
        const ColorUnpacked colorTo     = COLORS[(colorIndex + 1) % NUM_COLORS];
        const ColorUnpacked resultColor = se_lerp(colorFrom, colorTo, lerpValue);

        g_ui->set_param(SeUiParam::FONT_COLOR, { .color = col::pack(resultColor) });
        g_ui->text({ string::cstr(string::create_fmt(SeStringLifetime::Temporary, "Line of text. Линия текста. Color time ms: {}", clampedTime)) });

        positionY += fontHeight + ADDITIONAL_STEP;
        fontHeight *= FONT_HEIGHT_STEP;
        colorDt *= COLOR_TIME_SCALE;
    }
}

void update_window_example()
{
    const SeWindowSubsystemInput* input = win::get_input();
    if (win::is_keyboard_button_just_pressed(input, SE_ESCAPE)) g_menuState = MenuState::MAIN;

    g_ui->set_param(SeUiParam::FONT_HEIGHT, { .dim = 40.0f });
    g_ui->set_param(SeUiParam::FONT_LINE_STEP, { .dim = 50.0f });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 100.0f });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 100.0f });
    if (g_ui->begin_window({
        .uid    = "first_window",
        .width  = 600.0f,
        .height = 600.0f,
        .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
    }))
    {
        g_ui->text({ "Text inside window." });
        g_ui->text({ "Every \"text\" command starts from the new line."});
        g_ui->text({ "This window can be moved and resized."});
        g_ui->end_window();
    }

    // This window will have different colors
    g_ui->set_param(SeUiParam::PRIMARY_COLOR, { .color = col::pack({ 0.73f, 0.11f, 0.14f, 1.0f }) });
    g_ui->set_param(SeUiParam::SECONDARY_COLOR, { .color = col::pack({ 0.09f, 0.01f, 0.01f, 0.7f }) });
    g_ui->set_param(SeUiParam::ACCENT_COLOR, { .color = col::pack({ 0.88f, 0.73f, 0.73f, 1.0f }) });
    g_ui->set_param(SeUiParam::WINDOW_TOP_PANEL_THICKNESS, { .dim = 10.0f });
    g_ui->set_param(SeUiParam::WINDOW_BORDER_THICKNESS, { .dim = 3.0f });

    // This window will be centered around pivot position
    g_ui->set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::CENTER });
    g_ui->set_param(SeUiParam::PIVOT_TYPE_Y, { .pivot = SeUiPivotType::CENTER });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 1000.0f });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 400.0f });

    if (g_ui->begin_window({
        .uid    = "second_window",
        .width  = 600.0f,
        .height = 600.0f,
        .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
    }))
    {
        g_ui->text({ "Текст в окне." });
        g_ui->text({ "Каждая комманда \"text\" начинается с новой линии."});
        g_ui->text({ "Это окно можно двигать и менять ему размер." });
        g_ui->end_window();
    }
}

void update_button_example()
{
    const SeWindowSubsystemInput* input = win::get_input();
    if (win::is_keyboard_button_just_pressed(input, SE_ESCAPE)) g_menuState = MenuState::MAIN;

    g_ui->set_param(SeUiParam::FONT_HEIGHT, { .dim = 50.0f });
    g_ui->set_param(SeUiParam::PIVOT_TYPE_X, { .pivot = SeUiPivotType::CENTER });
    g_ui->set_param(SeUiParam::PIVOT_TYPE_Y, { .pivot = SeUiPivotType::CENTER });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 2.0f });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 100.0f });
    g_ui->text({ "Buttons have two modes - TOGGLE and HOLD" });
    
    g_ui->set_param(SeUiParam::FONT_HEIGHT, { .dim = 50.0f });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 4.0f });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 200.0f });
    
    // You can specify button size, but this size will be expanded if button text is too big
    if (g_ui->button({ "button", "Toggle button", 0, 0, SeUiButtonMode::TOGGLE }))
    {
        g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 270.0f });
        g_ui->text({ "You've toggled the button" });
    }

    g_ui->set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 4.0f * 3.0f });
    g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 200.0f });
    if (g_ui->button({ "button2", "Hold button", 0, 0, SeUiButtonMode::HOLD }))
    {
        g_ui->set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 270.0f });
        g_ui->text({ "You're holding the button" });
    }
}

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    se_init_global_subsystem_pointers(engine);
    g_render = se_get_subsystem_interface<SeRenderAbstractionSubsystemInterface>(engine);
    g_ui = se_get_subsystem_interface<SeUiSubsystemInterface>(engine);

    g_fontDataEnglish = data_provider::from_file("assets/default/fonts/shahd serif.ttf");
    g_fontDataRussian = data_provider::from_file("assets/default/fonts/a_antiquetrady regular.ttf");
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    data_provider::destroy(g_fontDataEnglish);
    data_provider::destroy(g_fontDataRussian);
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    const SeWindowSubsystemInput* input = win::get_input();
    if (input->isCloseButtonPressed) engine->shouldRun = false;
    if (g_render->begin_frame())
    {
        if (g_ui->begin_ui({ g_render, { g_render->swap_chain_texture(), SE_PASS_RENDER_TARGET_LOAD_OP_CLEAR } }))
        {
            g_ui->set_font_group({ g_fontDataEnglish, g_fontDataRussian });

            switch (g_menuState)
            {
                case MenuState::MAIN:           update_main_menu(engine);       break;
                case MenuState::TEXT_EXAMPLE:   update_text_example(info->dt);  break;
                case MenuState::WINDOW_EXAMPLE: update_window_example();        break;
                case MenuState::BUTTON_EXAMPLE: update_button_example();        break;
            }
            g_ui->end_ui(0);
        }
        g_render->end_frame();
    }
}
