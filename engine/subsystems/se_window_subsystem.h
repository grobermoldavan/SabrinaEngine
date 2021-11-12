#ifndef _SE_WINDOW_SUBSYSTEM_H_
#define _SE_WINDOW_SUBSYSTEM_H_

#include <inttypes.h>
#include <stdbool.h>

#define SE_WINDOW_SUBSYSTEM_NAME "se_window_subsystem"

#define se_is_keyboard_button_pressed(inputPtr, keyFlag) (input->keyboardButtons[keyFlag / 64] & (1ull << (keyFlag - (keyFlag / 64) * 64)))

typedef enum SeMouseInput
{
    SE_LMB,
    SE_RMB,
    SE_MMB,
} SeMouseInput;

typedef enum SeKeyboardInput
{
    SE_NONE,
    SE_Q, SE_W, SE_E, SE_R, SE_T, SE_Y, SE_U, SE_I, SE_O, SE_P, SE_A, SE_S, SE_D,
    SE_F, SE_G, SE_H, SE_J, SE_K, SE_L, SE_Z, SE_X, SE_C, SE_V, SE_B, SE_N, SE_M,
    SE_L_BRACKET, SE_R_BRACKET, SE_SEMICOLON, SE_APOSTROPHE, SE_BACKSLASH, SE_COMMA, SE_PERIOD, SE_SLASH,
    SE_TILDA, SE_NUM_1, SE_NUM_2, SE_NUM_3, SE_NUM_4, SE_NUM_5, SE_NUM_6, SE_NUM_7, SE_NUM_8, SE_NUM_9, SE_NUM_0, SE_MINUS, SE_EQUALS,
    SE_TAB, SE_CAPS_LOCK, SE_ENTER, SE_BACKSPACE, SE_SPACE,
    SE_L_SHIFT, SE_R_SHIFT, SE_SHIFT,
    SE_L_CTRL, SE_R_CTRL, SE_CTRL,
    SE_L_ALT, SE_R_ALT, SE_ALT,
    SE_ESCAPE, SE_F1, SE_F2, SE_F3, SE_F4, SE_F5, SE_F6, SE_F7, SE_F8, SE_F9, SE_F10, SE_F11, SE_F12,
    SE_ARROW_UP, SE_ARROW_DOWN, SE_ARROW_LEFT, SE_ARROW_RIGHT
} SeKeyboardInput;

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

typedef struct SeWindowSubsystemInput
{
    uint64_t keyboardButtons[2];    // flags
    uint32_t mouseButtons;          // flags
    uint32_t mouseX;                // position
    uint32_t mouseY;                // position
    uint32_t mouseWheel;            // wheel input
    bool isCloseButtonPressed;
} SeWindowSubsystemInput;

typedef struct SeWindowSubsystemCreateInfo
{
    const char* name;
    bool isFullscreen;
    bool isResizable;
    uint32_t width;
    uint32_t height;
} SeWindowSubsystemCreateInfo;

typedef struct SeWindowHandle
{
    void* ptr;
} SeWindowHandle;

typedef struct SeWindowSubsystemInterface
{
    SeWindowHandle                  (*create)(SeWindowSubsystemCreateInfo* createInfo);
    void                            (*destroy)(SeWindowHandle handle);
    const SeWindowSubsystemInput*   (*get_input)(SeWindowHandle handle);
    uint32_t                        (*get_width)(SeWindowHandle handle);
    uint32_t                        (*get_height)(SeWindowHandle handle);
    void*                           (*get_native_handle)(SeWindowHandle handle);
} SeWindowSubsystemInterface;

#endif
