#ifndef _SE_WINDOW_SUBSYSTEM_H_
#define _SE_WINDOW_SUBSYSTEM_H_

#include <inttypes.h>
#include <stdbool.h>

enum SeMouseInput
{
    SE_LMB,
    SE_RMB,
    SE_MMB,
};

enum SeWindowResizeCallbackPriority
{
    __SE_WINDOW_RESIZE_CALLBACK_PRIORITY_UNDEFINED,
    SE_WINDOW_RESIZE_CALLBACK_PRIORITY_INTERNAL_SYSTEM,
    SE_WINDOW_RESIZE_CALLBACK_PRIORITY_USER_SYSTEM,
    __SE_WINDOW_RESIZE_CALLBACK_PRIORITY_COUNT,
};

enum SeKeyboardInput
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

struct SeWindowSubsystemInput
{
    uint64_t keyboardButtonsCurrent[2];     // flags
    uint64_t keyboardButtonsPrevious[2];    // flags
    uint32_t mouseButtons;                  // flags
    int64_t mouseX;                         // position
    int64_t mouseY;                         // position
    int32_t mouseWheel;                     // wheel input
    bool isCloseButtonPressed;
};

struct SeWindowSubsystemCreateInfo
{
    const char* name;
    bool isFullscreen;
    bool isResizable;
    uint32_t width;
    uint32_t height;
};

struct SeWindowHandle
{
    void* ptr;
};

struct SeWindowResizeCallbackInfo
{
    void* userData;
    void (*callback)(SeWindowHandle handle, void* userData);
    SeWindowResizeCallbackPriority priority;
};

using SeWindowResizeCallbackHandle = void*;

struct SeWindowSubsystemInterface
{
    static constexpr const char* NAME = "SeWindowSubsystemInterface";

    SeWindowHandle                  (*create)(const SeWindowSubsystemCreateInfo& createInfo);
    void                            (*destroy)(SeWindowHandle handle);
    const SeWindowSubsystemInput*   (*get_input)(SeWindowHandle handle);
    uint32_t                        (*get_width)(SeWindowHandle handle);
    uint32_t                        (*get_height)(SeWindowHandle handle);
    void*                           (*get_native_handle)(SeWindowHandle handle);
    SeWindowResizeCallbackHandle    (*add_resize_callback)(SeWindowHandle handle, const SeWindowResizeCallbackInfo& callbackInfo);
    void                            (*remove_resize_callback)(SeWindowHandle handle, SeWindowResizeCallbackHandle cbHandle);
};

struct SeWindowSubsystem
{
    using Interface = SeWindowSubsystemInterface;
    static constexpr const char* NAME = "se_window_subsystem";
};

#define SE_WINDOW_SUBSYSTEM_GLOBAL_NAME g_windowSubsystemIface
const struct SeWindowSubsystemInterface* SE_WINDOW_SUBSYSTEM_GLOBAL_NAME;

namespace win
{
    inline SeWindowHandle create(const SeWindowSubsystemCreateInfo& createInfo)
    {
        return SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->create(createInfo);
    }

    inline void destroy(SeWindowHandle handle)
    {
        SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->destroy(handle);
    }

    inline const SeWindowSubsystemInput* get_input(SeWindowHandle handle)
    {
        return SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->get_input(handle);
    }

    inline uint32_t get_width(SeWindowHandle handle)
    {
        return SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->get_width(handle);
    }

    inline uint32_t get_height(SeWindowHandle handle)
    {
        return SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->get_height(handle);
    }

    inline void* get_native_handle(SeWindowHandle handle)
    {
        return SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->get_native_handle(handle);
    }

    inline SeWindowResizeCallbackHandle add_resize_callback(SeWindowHandle handle, const SeWindowResizeCallbackInfo& callbackInfo)
    {
        return SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->add_resize_callback(handle, callbackInfo);
    }

    inline void remove_resize_callback(SeWindowHandle handle, SeWindowResizeCallbackHandle cbHandle)
    {
        SE_WINDOW_SUBSYSTEM_GLOBAL_NAME->remove_resize_callback(handle, cbHandle);
    }

    inline bool is_keyboard_button_pressed(const uint64_t* keyboardFlags, SeKeyboardInput keyFlag)
    {
        return keyboardFlags[keyFlag / 64] & (1ull << (keyFlag - (keyFlag / 64) * 64));
    }

    inline bool is_keyboard_button_pressed(const SeWindowSubsystemInput* inputPtr, SeKeyboardInput keyFlag)
    {
        return win::is_keyboard_button_pressed(inputPtr->keyboardButtonsCurrent, keyFlag);
    }

    inline bool is_keyboard_button_just_pressed(const SeWindowSubsystemInput* inputPtr, SeKeyboardInput keyFlag)
    {
        return win::is_keyboard_button_pressed(inputPtr->keyboardButtonsCurrent, keyFlag) &&
              !win::is_keyboard_button_pressed(inputPtr->keyboardButtonsPrevious, keyFlag);
    }

    inline bool is_mouse_button_pressed(const SeWindowSubsystemInput* inputPtr, SeMouseInput keyFlag)
    {
        return inputPtr->mouseButtons & (1ull << keyFlag);
    }
}

#endif
