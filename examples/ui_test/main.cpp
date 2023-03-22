
#include "engine/se_engine.hpp"
#include "engine/se_engine.cpp"

SeDataProvider g_fontDataEnglish;
SeDataProvider g_fontDataRussian;

enum struct MenuState
{
    MAIN,
    TEXT_EXAMPLE,
    WINDOW_EXAMPLE,
    BUTTON_EXAMPLE,
} g_menuState = MenuState::MAIN;

enum Language
{
    ENGLISH,
    RUSSIAN,
    __LANGUAGE_COUNT,
} g_currentLanguage = Language::ENGLISH;

enum LocalizedString
{
    CURRENT_LANGUAGE,
    MM_TEXT_EXAMPLES,
    MM_WINDOW_EXAMPLES,
    MM_BUTTON_EXAMPLES,
    LINE_OF_TEXT,
    TEXT_INSIDE_WINDOW,
    EVERY_TEXT_COMMAND_STARTS_FROM_THE_NEW_LINE,
    WINDOW_CAN_BE_MOVED_AND_RESIZED,
    WINDOW_CAN_BE_SCROLLED,
    WINDOW_CAN_NOT_BE_SCROLLED,
    BUTTON_MODES,
    TOGGLE_BUTTON,
    YOU_VE_TOGGLED_THE_BUTTON,
    HOLD_BUTTON,
    YOU_RE_HOLDING_THE_BUTTON,
    THIS_IS_A_TEXT_LINE,
    __LOCALIZED_STRING_COUNT,
};

using LocalizationSet = const char*[LocalizedString::__LOCALIZED_STRING_COUNT];
LocalizationSet LOCALIZATION_SETS[Language::__LANGUAGE_COUNT] =
{
    {
        /* CURRENT_LANGUAGE */                              "English",
        /* MM_TEXT_EXAMPLES */                              "Text examples",
        /* MM_WINDOW_EXAMPLES */                            "Window examples",
        /* MM_BUTTON_EXAMPLES */                            "Button examples",
        /* LINE_OF_TEXT */                                  "Line of text",
        /* TEXT_INSIDE_WINDOW */                            "Text inside window",
        /* EVERY_TEXT_COMMAND_STARTS_FROM_THE_NEW_LINE */   "Every \"text\" command starts from the new line",
        /* WINDOW_CAN_BE_MOVED_AND_RESIZED */               "This window can be moved and resized",
        /* WINDOW_CAN_BE_SCROLLED */                        "This window can be scrolled",
        /* WINDOW_CAN_NOT_BE_SCROLLED */                    "This window can't be scrolled",
        /* BUTTON_MODES */                                  "Buttons have three modes - TOGGLE, HOLD and CLICK",
        /* TOGGLE_BUTTON */                                 "Toggle button",
        /* YOU_VE_TOGGLED_THE_BUTTON */                     "You've toggled the button",
        /* HOLD_BUTTON */                                   "Hold button",
        /* YOU_RE_HOLDING_THE_BUTTON */                     "You're holding the button",
        /* THIS_IS_A_TEXT_LINE */                           "This is a text input line",
    },
    {
        /* CURRENT_LANGUAGE */                              "Русский",
        /* MM_TEXT_EXAMPLES */                              "Примеры текста",
        /* MM_WINDOW_EXAMPLES */                            "Примеры окон",
        /* MM_BUTTON_EXAMPLES */                            "Примеры кнопок",
        /* LINE_OF_TEXT */                                  "Линия текста",
        /* TEXT_INSIDE_WINDOW */                            "Текст внутри окна",
        /* EVERY_TEXT_COMMAND_STARTS_FROM_THE_NEW_LINE */   "Каждая комманда \"text\" начинается с новой строки",
        /* WINDOW_CAN_BE_MOVED_AND_RESIZED */               "Это окно можно двигать и менять ему размер",
        /* WINDOW_CAN_BE_SCROLLED */                        "Это окно можно прокручивать",
        /* WINDOW_CAN_NOT_BE_SCROLLED */                    "Это окно нельзя прокручивать",
        /* BUTTON_MODES */                                  "У кнопок есть три режима - TOGGLE, HOLD and CLICK",
        /* TOGGLE_BUTTON */                                 "TOGGLE кнопка",
        /* YOU_VE_TOGGLED_THE_BUTTON */                     "Вы переключили кнопку",
        /* HOLD_BUTTON */                                   "HOLD кнопка",
        /* YOU_RE_HOLDING_THE_BUTTON */                     "Вы удерживаете кнопку",
        /* THIS_IS_A_TEXT_LINE */                           "Строка для текстового инпута",
    },
};

inline const char* get_local(LocalizedString str)
{
    return LOCALIZATION_SETS[g_currentLanguage][str];
}

void draw_language_selection()
{
    se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = 40 });
    se_ui_set_param(SeUiParam::FONT_COLOR, { .color = se_color_pack({ 1.0f, 1.0f, 1.0f, 1.0f }) });
    se_ui_set_param(SeUiParam::PRIMARY_COLOR, { .color = se_color_pack({ 0.2f, 0.2f, 0.2f, 1.0f }) });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 10.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 10.0f });
    if (se_ui_button({ "lang_selection", get_local(LocalizedString::CURRENT_LANGUAGE), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_currentLanguage = g_currentLanguage == Language::ENGLISH ? Language::RUSSIAN : Language::ENGLISH;
    }
}

void update_main_menu()
{
    if (se_win_is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) se_engine_stop();

    constexpr float FONT_HEIGHT = 80.0f;

    se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = FONT_HEIGHT });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::CENTER });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::CENTER });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = se_win_get_width<float>() / 2.0f });

    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() / 2.0f + FONT_HEIGHT * 2.0f });
    if (se_ui_button({ "mm_to_text", get_local(LocalizedString::MM_TEXT_EXAMPLES), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_menuState = MenuState::TEXT_EXAMPLE;
    }

    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() / 2.0f });
    if (se_ui_button({ "mm_to_window", get_local(LocalizedString::MM_WINDOW_EXAMPLES), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_menuState = MenuState::WINDOW_EXAMPLE;
    }

    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() / 2.0f - FONT_HEIGHT * 2.0f });
    if (se_ui_button({ "mm_to_button", get_local(LocalizedString::MM_BUTTON_EXAMPLES), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_menuState = MenuState::BUTTON_EXAMPLE;
    }
}

void update_text_example(float dt)
{
    using TimeMs = uint64_t;

    if (se_win_is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) g_menuState = MenuState::MAIN;

    constexpr size_t    NUM_LINES           = 8;
    constexpr float     BASE_FONT_HEIGHT    = 80.0f;
    constexpr float     FONT_HEIGHT_STEP    = 0.8f;
    constexpr float     ADDITIONAL_STEP     = 5.0f;
    constexpr TimeMs    COLOR_INTERVAL_MS   = 2000;
    constexpr float     COLOR_TIME_SCALE    = 1.2f;
    constexpr SeColorUnpacked COLORS[] =
    {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 1.0f, 1.0f },
        { 0.2f, 1.0f, 1.0f, 1.0f },
    };
    constexpr size_t NUM_COLORS = se_array_size(COLORS);
    constexpr TimeMs MAX_COLOR_TIME_MS = NUM_COLORS * COLOR_INTERVAL_MS;

    se_ui_set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 50.0f });

    float positionY = 40.0f;
    float fontHeight = BASE_FONT_HEIGHT;
    float colorDt = dt;
    for (size_t it = 0; it < NUM_LINES; it++)
    {
        se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = positionY });
        se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = fontHeight });

        static TimeMs colorTimes[NUM_LINES] = { };
        colorTimes[it] += TimeMs(colorDt * 1000.0f);

        // Lerp colors for fun
        const TimeMs        clampedTime = colorTimes[it] % MAX_COLOR_TIME_MS;
        const size_t        colorIndex  = clampedTime / COLOR_INTERVAL_MS;
        const float         lerpValue   = float(clampedTime % COLOR_INTERVAL_MS) / float(COLOR_INTERVAL_MS);
        const SeColorUnpacked colorFrom   = COLORS[colorIndex];
        const SeColorUnpacked colorTo     = COLORS[(colorIndex + 1) % NUM_COLORS];
        const SeColorUnpacked resultColor = se_lerp(colorFrom, colorTo, lerpValue);

        se_ui_set_param(SeUiParam::FONT_COLOR, { .color = se_color_pack(resultColor) });
        se_ui_text({ se_string_cstr(se_string_create_fmt(SeStringLifetime::TEMPORARY, "{} : {}", get_local(LocalizedString::LINE_OF_TEXT), clampedTime)) });

        positionY += fontHeight + ADDITIONAL_STEP;
        fontHeight *= FONT_HEIGHT_STEP;
        colorDt *= COLOR_TIME_SCALE;
    }
}

void update_window_example()
{
    if (se_win_is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) g_menuState = MenuState::MAIN;

    se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = 40.0f });
    se_ui_set_param(SeUiParam::FONT_LINE_GAP, { .dim = 10.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 100.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 100.0f });
    if (se_ui_begin_window({
        .uid    = "first_window",
        .width  = 600.0f,
        .height = 600.0f,
        .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y | SeUiFlags::SCROLLABLE_X | SeUiFlags::SCROLLABLE_Y,
    }))
    {
        se_ui_text({ get_local(LocalizedString::TEXT_INSIDE_WINDOW) });
        se_ui_text({ get_local(LocalizedString::EVERY_TEXT_COMMAND_STARTS_FROM_THE_NEW_LINE) });
        se_ui_text({ get_local(LocalizedString::WINDOW_CAN_BE_MOVED_AND_RESIZED) });
        se_ui_input_text_line({ "text_input_line", get_local(LocalizedString::THIS_IS_A_TEXT_LINE) });
        for (size_t it = 0; it < 10; it++)
            se_ui_text({ get_local(LocalizedString::WINDOW_CAN_BE_SCROLLED) });
        se_ui_end_window();
    }

    // This window will have different colors
    se_ui_set_param(SeUiParam::PRIMARY_COLOR, { .color = se_color_pack({ 0.87f, 0.81f, 0.83f, 1.0f }) });
    se_ui_set_param(SeUiParam::SECONDARY_COLOR, { .color = se_color_pack({ 0.36f, 0.45f, 0.58f, 0.8f }) });
    se_ui_set_param(SeUiParam::ACCENT_COLOR, { .color = se_color_pack({ 0.95f, 0.75f, 0.79f, 1.0f }) });
    se_ui_set_param(SeUiParam::WINDOW_TOP_PANEL_THICKNESS, { .dim = 10.0f });
    se_ui_set_param(SeUiParam::WINDOW_BORDER_THICKNESS, { .dim = 3.0f });

    // This window will be centered around pivot position
    se_ui_set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::CENTER });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::CENTER });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 1000.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 400.0f });

    if (se_ui_begin_window({
        .uid    = "second_window",
        .width  = 600.0f,
        .height = 600.0f,
        .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
    }))
    {
        se_ui_text({ get_local(LocalizedString::TEXT_INSIDE_WINDOW) });
        se_ui_text({ get_local(LocalizedString::EVERY_TEXT_COMMAND_STARTS_FROM_THE_NEW_LINE) });
        se_ui_text({ get_local(LocalizedString::WINDOW_CAN_BE_MOVED_AND_RESIZED) });
        if (se_ui_button({ "win_button", get_local(LocalizedString::HOLD_BUTTON), 0, 0, SeUiButtonMode::HOLD }))
        {
            se_ui_text({ get_local(LocalizedString::YOU_RE_HOLDING_THE_BUTTON) });
        }
        se_ui_text({ get_local(LocalizedString::WINDOW_CAN_NOT_BE_SCROLLED) });

        se_ui_end_window();
    }
}

void update_button_example()
{
    if (se_win_is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) g_menuState = MenuState::MAIN;

    se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = 50.0f });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::CENTER });
    se_ui_set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::CENTER });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = se_win_get_width<float>() / 2.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() - 100.0f });
    se_ui_text({ get_local(LocalizedString::BUTTON_MODES) });
    
    se_ui_set_param(SeUiParam::FONT_HEIGHT, { .dim = 50.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = se_win_get_width<float>() / 4.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() - 200.0f });
    
    // You can specify button size, but this size will be expanded if button text is too big
    if (se_ui_button({ "button", get_local(LocalizedString::TOGGLE_BUTTON), 0, 0, SeUiButtonMode::TOGGLE }))
    {
        se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() - 270.0f });
        se_ui_text({ get_local(LocalizedString::YOU_VE_TOGGLED_THE_BUTTON) });
    }

    se_ui_set_param(SeUiParam::PIVOT_POSITION_X, { .dim = se_win_get_width<float>() / 4.0f * 3.0f });
    se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() - 200.0f });
    if (se_ui_button({ "button2", get_local(LocalizedString::HOLD_BUTTON), 0, 0, SeUiButtonMode::HOLD }))
    {
        se_ui_set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = se_win_get_height<float>() - 270.0f });
        se_ui_text({ get_local(LocalizedString::YOU_RE_HOLDING_THE_BUTTON) });
    }
}

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
        if (se_ui_begin({ se_render_swap_chain_texture(), SeRenderTargetLoadOp::CLEAR, { 0.5f, 0.5f, 0.1f, 1.0f } }))
        {
            se_ui_set_font_group({ g_fontDataEnglish, g_fontDataRussian });
            switch (g_menuState)
            {
                case MenuState::MAIN:           update_main_menu();             break;
                case MenuState::TEXT_EXAMPLE:   update_text_example(info.dt);   break;
                case MenuState::WINDOW_EXAMPLE: update_window_example();        break;
                case MenuState::BUTTON_EXAMPLE: update_button_example();        break;
            }
            draw_language_selection();
            se_ui_end(0);
        }
        se_render_end_frame();
    }
}

int main(int argc, char* argv[])
{
    const SeSettings settings
    {
        .applicationName        = "Sabrina engine - ui example",
        .isFullscreenWindow     = false,
        .isResizableWindow      = true,
        .windowWidth            = 640,
        .windowHeight           = 480,
        .createUserDataFolder   = false,
    };
    se_engine_run(settings, init, update, terminate);
    return 0;
}
