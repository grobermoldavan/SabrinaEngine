
#include "engine/engine.hpp"
#include "engine/engine.cpp"

DataProvider g_fontDataEnglish;
DataProvider g_fontDataRussian;

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
    ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = 40 });
    ui::set_param(SeUiParam::FONT_COLOR, { .color = col::pack({ 1.0f, 1.0f, 1.0f, 1.0f }) });
    ui::set_param(SeUiParam::PRIMARY_COLOR, { .color = col::pack({ 0.2f, 0.2f, 0.2f, 1.0f }) });
    ui::set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    ui::set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 10.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 10.0f });
    if (ui::button({ "lang_selection", get_local(LocalizedString::CURRENT_LANGUAGE), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_currentLanguage = g_currentLanguage == Language::ENGLISH ? Language::RUSSIAN : Language::ENGLISH;
    }
}

void update_main_menu()
{
    if (win::is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) engine::stop();

    constexpr float FONT_HEIGHT = 80.0f;

    ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = FONT_HEIGHT });
    ui::set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::CENTER });
    ui::set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::CENTER });
    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 2.0f });

    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() / 2.0f + FONT_HEIGHT * 2.0f });
    if (ui::button({ "mm_to_text", get_local(LocalizedString::MM_TEXT_EXAMPLES), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_menuState = MenuState::TEXT_EXAMPLE;
    }

    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() / 2.0f });
    if (ui::button({ "mm_to_window", get_local(LocalizedString::MM_WINDOW_EXAMPLES), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_menuState = MenuState::WINDOW_EXAMPLE;
    }

    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() / 2.0f - FONT_HEIGHT * 2.0f });
    if (ui::button({ "mm_to_button", get_local(LocalizedString::MM_BUTTON_EXAMPLES), 0, 0, SeUiButtonMode::CLICK }))
    {
        g_menuState = MenuState::BUTTON_EXAMPLE;
    }
}

void update_text_example(float dt)
{
    using TimeMs = uint64_t;

    if (win::is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) g_menuState = MenuState::MAIN;

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

    ui::set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    ui::set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::BOTTOM_LEFT });
    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 50.0f });

    float positionY = 40.0f;
    float fontHeight = BASE_FONT_HEIGHT;
    float colorDt = dt;
    for (size_t it = 0; it < NUM_LINES; it++)
    {
        ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = positionY });
        ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = fontHeight });

        static TimeMs colorTimes[NUM_LINES] = { };
        colorTimes[it] += TimeMs(colorDt * 1000.0f);

        // Lerp colors for fun
        const TimeMs        clampedTime = colorTimes[it] % MAX_COLOR_TIME_MS;
        const size_t        colorIndex  = clampedTime / COLOR_INTERVAL_MS;
        const float         lerpValue   = float(clampedTime % COLOR_INTERVAL_MS) / float(COLOR_INTERVAL_MS);
        const SeColorUnpacked colorFrom   = COLORS[colorIndex];
        const SeColorUnpacked colorTo     = COLORS[(colorIndex + 1) % NUM_COLORS];
        const SeColorUnpacked resultColor = se_lerp(colorFrom, colorTo, lerpValue);

        ui::set_param(SeUiParam::FONT_COLOR, { .color = col::pack(resultColor) });
        ui::text({ string::cstr(string::create_fmt(SeStringLifetime::TEMPORARY, "{} : {}", get_local(LocalizedString::LINE_OF_TEXT), clampedTime)) });

        positionY += fontHeight + ADDITIONAL_STEP;
        fontHeight *= FONT_HEIGHT_STEP;
        colorDt *= COLOR_TIME_SCALE;
    }
}

void update_window_example()
{
    if (win::is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) g_menuState = MenuState::MAIN;

    ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = 40.0f });
    ui::set_param(SeUiParam::FONT_LINE_GAP, { .dim = 10.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 100.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 100.0f });
    if (ui::begin_window({
        .uid    = "first_window",
        .width  = 600.0f,
        .height = 600.0f,
        .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y | SeUiFlags::SCROLLABLE_X | SeUiFlags::SCROLLABLE_Y,
    }))
    {
        ui::text({ get_local(LocalizedString::TEXT_INSIDE_WINDOW) });
        ui::text({ get_local(LocalizedString::EVERY_TEXT_COMMAND_STARTS_FROM_THE_NEW_LINE) });
        ui::text({ get_local(LocalizedString::WINDOW_CAN_BE_MOVED_AND_RESIZED) });
        ui::input_text_line({ "text_input_line", get_local(LocalizedString::THIS_IS_A_TEXT_LINE) });
        for (size_t it = 0; it < 10; it++)
            ui::text({ get_local(LocalizedString::WINDOW_CAN_BE_SCROLLED) });
        ui::end_window();
    }

    // This window will have different colors
    ui::set_param(SeUiParam::PRIMARY_COLOR, { .color = col::pack({ 0.87f, 0.81f, 0.83f, 1.0f }) });
    ui::set_param(SeUiParam::SECONDARY_COLOR, { .color = col::pack({ 0.36f, 0.45f, 0.58f, 0.8f }) });
    ui::set_param(SeUiParam::ACCENT_COLOR, { .color = col::pack({ 0.95f, 0.75f, 0.79f, 1.0f }) });
    ui::set_param(SeUiParam::WINDOW_TOP_PANEL_THICKNESS, { .dim = 10.0f });
    ui::set_param(SeUiParam::WINDOW_BORDER_THICKNESS, { .dim = 3.0f });

    // This window will be centered around pivot position
    ui::set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::CENTER });
    ui::set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::CENTER });
    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = 1000.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = 400.0f });

    if (ui::begin_window({
        .uid    = "second_window",
        .width  = 600.0f,
        .height = 600.0f,
        .flags  = SeUiFlags::MOVABLE | SeUiFlags::RESIZABLE_X | SeUiFlags::RESIZABLE_Y,
    }))
    {
        ui::text({ get_local(LocalizedString::TEXT_INSIDE_WINDOW) });
        ui::text({ get_local(LocalizedString::EVERY_TEXT_COMMAND_STARTS_FROM_THE_NEW_LINE) });
        ui::text({ get_local(LocalizedString::WINDOW_CAN_BE_MOVED_AND_RESIZED) });
        if (ui::button({ "win_button", get_local(LocalizedString::HOLD_BUTTON), 0, 0, SeUiButtonMode::HOLD }))
        {
            ui::text({ get_local(LocalizedString::YOU_RE_HOLDING_THE_BUTTON) });
        }
        ui::text({ get_local(LocalizedString::WINDOW_CAN_NOT_BE_SCROLLED) });

        ui::end_window();
    }
}

void update_button_example()
{
    if (win::is_keyboard_button_just_pressed(SeKeyboard::ESCAPE)) g_menuState = MenuState::MAIN;

    ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = 50.0f });
    ui::set_param(SeUiParam::PIVOT_TYPE_X, { .enumeration = SeUiPivotType::CENTER });
    ui::set_param(SeUiParam::PIVOT_TYPE_Y, { .enumeration = SeUiPivotType::CENTER });
    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 2.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 100.0f });
    ui::text({ get_local(LocalizedString::BUTTON_MODES) });
    
    ui::set_param(SeUiParam::FONT_HEIGHT, { .dim = 50.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 4.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 200.0f });
    
    // You can specify button size, but this size will be expanded if button text is too big
    if (ui::button({ "button", get_local(LocalizedString::TOGGLE_BUTTON), 0, 0, SeUiButtonMode::TOGGLE }))
    {
        ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 270.0f });
        ui::text({ get_local(LocalizedString::YOU_VE_TOGGLED_THE_BUTTON) });
    }

    ui::set_param(SeUiParam::PIVOT_POSITION_X, { .dim = win::get_width<float>() / 4.0f * 3.0f });
    ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 200.0f });
    if (ui::button({ "button2", get_local(LocalizedString::HOLD_BUTTON), 0, 0, SeUiButtonMode::HOLD }))
    {
        ui::set_param(SeUiParam::PIVOT_POSITION_Y, { .dim = win::get_height<float>() - 270.0f });
        ui::text({ get_local(LocalizedString::YOU_RE_HOLDING_THE_BUTTON) });
    }
}

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
        if (ui::begin({ render::swap_chain_texture(), SeRenderTargetLoadOp::CLEAR, { 0.5f, 0.5f, 0.1f, 1.0f } }))
        {
            ui::set_font_group({ g_fontDataEnglish, g_fontDataRussian });
            switch (g_menuState)
            {
                case MenuState::MAIN:           update_main_menu();             break;
                case MenuState::TEXT_EXAMPLE:   update_text_example(info.dt);   break;
                case MenuState::WINDOW_EXAMPLE: update_window_example();        break;
                case MenuState::BUTTON_EXAMPLE: update_button_example();        break;
            }
            draw_language_selection();
            ui::end(0);
        }
        render::end_frame();
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
    engine::run(settings, init, update, terminate);
    return 0;
}
