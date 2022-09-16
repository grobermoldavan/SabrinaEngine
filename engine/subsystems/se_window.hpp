#ifndef _SE_WINDOW_SUBSYSTEM_H_
#define _SE_WINDOW_SUBSYSTEM_H_

#include "engine/se_common_includes.hpp"

struct SeMouse
{
    using Type = uint32_t;
    enum : Type
    {
        LMB,
        RMB,
        MMB,
    };
};

struct SeKeyboard
{
    using Type = uint32_t;
    enum : Type
    {
        NONE,
        Q, W, E, R, T, Y, U, I, O, P, A, S, D,
        F, G, H, J, K, L, Z, X, C, V, B, N, M,
        L_BRACKET, R_BRACKET, SEMICOLON, APOSTROPHE, BACKSLASH, COMMA, PERIOD, SLASH,
        TILDA, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9, NUM_0, MINUS, EQUALS,
        TAB, CAPS_LOCK, ENTER, BACKSPACE, SPACE,
        L_SHIFT, R_SHIFT, SHIFT,
        L_CTRL, R_CTRL, CTRL,
        L_ALT, R_ALT, ALT,
        ESCAPE, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
        ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT
    };
};

const char* SE_KEYBOARD_INPUT_TO_STRING[] =
{
    "NONE",
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "A", "S", "D", "F", "G", "H", "J", "K", "L", "Z", "X", "C", "V", "B", "N", "M",
    "L_BRACKET", "R_BRACKET", "SEMICOLON", "APOSTROPHE", "BACKSLASH", "COMMA", "PERIOD", "SLASH",
    "TILDA", "NUM_1", "NUM_2", "NUM_3", "NUM_4", "NUM_5", "NUM_6", "NUM_7", "NUM_8", "NUM_9", "NUM_0", "MINUS", "EQUALS",
    "TAB", "CAPS_LOCK", "ENTER", "BACKSPACE", "SPACE",
    "L_SHIFT", "R_SHIFT", "SHIFT",
    "L_CTRL", "R_CTRL", "CTRL",
    "L_ALT", "R_ALT", "ALT",
    "ESCAPE", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
    "ARROW_UP", "ARROW_DOWN", "ARROW_LEFT", "ARROW_RIGHT"
};

const char* SE_MOUSE_INPUT_TO_STRING[] =
{
    "LMB", "RMB", "MMB"
};

struct SeMousePos
{
    int64_t x;
    int64_t y;
};

namespace win
{
    template<typename T = uint32_t> T   get_width();
    template<typename T = uint32_t> T   get_height();
    void*                               get_native_handle();

    SeMousePos  get_mouse_pos();
    int32_t     get_mouse_wheel();
    bool        is_close_button_pressed();

    bool        is_keyboard_button_pressed(SeKeyboard::Type keyFlag);
    bool        is_keyboard_button_just_pressed(SeKeyboard::Type keyFlag);
    bool        is_mouse_button_pressed(SeMouse::Type keyFlag);
    bool        is_mouse_button_just_pressed(SeMouse::Type keyFlag);

    namespace engine
    {
        void init(const SeSettings* settings);
        void update();
        void terminate();
    }
}

#endif
