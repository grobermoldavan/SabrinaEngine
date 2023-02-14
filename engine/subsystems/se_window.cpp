
#include "se_window.hpp"
#include "engine/engine.hpp"

struct SeWindowSubsystemInput
{
    uint64_t keyboardButtonsCurrent[2];     // flags
    uint64_t keyboardButtonsPrevious[2];    // flags
    uint32_t mouseButtonsCurrent;           // flags
    uint32_t mouseButtonsPrevious;          // flags
    int64_t mouseX;                         // position
    int64_t mouseY;                         // position
    int32_t mouseWheel;                     // wheel input
    bool isCloseButtonPressed;
};

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Windowsx.h>

struct SeWindow
{
    SeWindowSubsystemInput          input;
    HWND                            handle;
    uint32_t                        width;
    uint32_t                        height;
    DynamicArray<SeCharacterInput>  characterInput;
} g_window;

namespace win
{
    LRESULT win32_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    bool    win32_window_process_mouse_input(SeWindow* window, UINT message, WPARAM wParam, LPARAM lParam);
    bool    win32_window_process_keyboard_input(SeWindow* window, UINT message, WPARAM wParam, LPARAM lParam);

    template<typename T>
    T get_width()
    {
        return T(g_window.width);
    }

    template<typename T>
    T get_height()
    {
        return T(g_window.height);
    }

    void* get_native_handle()
    {
        return g_window.handle;
    }

    SeMousePos get_mouse_pos()
    {
        return { g_window.input.mouseX, g_window.input.mouseY };
    }

    int32_t get_mouse_wheel()
    {
        return g_window.input.mouseWheel;
    }

    bool is_close_button_pressed()
    {
        return g_window.input.isCloseButtonPressed;
    }

    bool is_keyboard_button_pressed(SeKeyboard::Type keyFlag)
    {
        return g_window.input.keyboardButtonsCurrent[keyFlag / 64] & (1ull << (keyFlag - (keyFlag / 64) * 64));
    }

    bool is_keyboard_button_just_pressed(SeKeyboard::Type keyFlag)
    {
        const bool isPressed = g_window.input.keyboardButtonsCurrent[keyFlag / 64] & (1ull << (keyFlag - (keyFlag / 64) * 64));
        const bool wasPressed = g_window.input.keyboardButtonsPrevious[keyFlag / 64] & (1ull << (keyFlag - (keyFlag / 64) * 64));
        return isPressed && !wasPressed;
    }

    bool is_mouse_button_pressed(SeMouse::Type keyFlag)
    {
        return g_window.input.mouseButtonsCurrent & (1ull << keyFlag);
    }

    bool is_mouse_button_just_pressed(SeMouse::Type keyFlag)
    {
        const bool isPressed = g_window.input.mouseButtonsCurrent & (1ull << keyFlag);
        const bool wasPressed = g_window.input.mouseButtonsPrevious & (1ull << keyFlag);
        return isPressed && !wasPressed;
    }

    const DynamicArray<SeCharacterInput>& get_character_input()
    {
        return g_window.characterInput;
    }

    void engine::init(const SeSettings& settings)
    {
        se_assert_msg(settings.applicationName, "Application must have name");
        const SeStringW applicationNameW = stringw::from_utf8(allocators::frame(), settings.applicationName);

        const HMODULE moduleHandle = GetModuleHandle(nullptr);

        WNDCLASSW wc = { };
        wc.lpfnWndProc      = win32_window_proc;
        wc.hInstance        = moduleHandle;
        wc.lpszClassName    = stringw::cstr(applicationNameW);
        wc.hCursor          = LoadCursor(nullptr, IDC_ARROW);

        const bool isClassRegistered = RegisterClassW(&wc);
        se_assert(isClassRegistered);

        const DWORD windowedStyleNoResize =
            WS_OVERLAPPED   |           // Window has title and borders
            WS_SYSMENU      |           // Add sys menu (close button)
            WS_MINIMIZEBOX  ;           // Add minimize button
        const DWORD windowedStyleResize =
            WS_OVERLAPPED   |           // Window has title and borders
            WS_SYSMENU      |           // Add sys menu (close button)
            WS_MINIMIZEBOX  |           // Add minimize button
            WS_MAXIMIZEBOX  |           // Add maximize button
            WS_SIZEBOX      ;           // Add ability to resize window by dragging it's frame
        const DWORD fullscreenStyle = WS_POPUP;
        const DWORD activeStyle = settings.isFullscreenWindow ? fullscreenStyle : (settings.isResizableWindow ? windowedStyleResize : windowedStyleNoResize);

        // @TODO : safe cast from uint32_t to int
        const int targetWidth = int(settings.windowWidth);
        const int targetHeight = int(settings.windowHeight);

        g_window.handle = CreateWindowExW(
            0,                                  // Optional window styles.
            stringw::cstr(applicationNameW),    // Window class
            stringw::cstr(applicationNameW),    // Window text
            activeStyle,                        // Window styles
            CW_USEDEFAULT,                      // Position X
            CW_USEDEFAULT,                      // Position Y
            targetWidth,                        // Size X
            targetHeight,                       // Size Y
            nullptr,                            // Parent window
            nullptr,                            // Menu
            moduleHandle,                       // Instance handle
            nullptr                             // Additional application data
        );
        se_assert(g_window.handle);

        if (!settings.isFullscreenWindow)
        {
            // Set correct size for a client rect
            // Retrieve currecnt client rect size and calculate difference with target size
            RECT clientRect;
            bool getClientRectResult = GetClientRect(g_window.handle, &clientRect);
            se_assert(getClientRectResult);
            const int widthDiff = targetWidth - (clientRect.right - clientRect.left);
            const int heightDiff = targetHeight - (clientRect.bottom - clientRect.top);
            // Retrieve currecnt window rect size (this includes borders!)
            RECT windowRect;
            const bool getWindowRectResult = GetWindowRect(g_window.handle, &windowRect);
            se_assert(getWindowRectResult);
            const int windowWidth = windowRect.right - windowRect.left;
            const int windowHeight = windowRect.bottom - windowRect.top;
            // Set new window size based on calculated diff
            const bool setWindowPosResult = SetWindowPos(g_window.handle, nullptr, windowRect.left, windowRect.top, windowWidth + widthDiff, windowHeight + heightDiff, SWP_NOMOVE | SWP_NOZORDER);
            se_assert(setWindowPosResult);
            // Retrieve resulting size
            getClientRectResult = GetClientRect(g_window.handle, &clientRect);
            se_assert(getClientRectResult);
            g_window.width = (uint32_t)(clientRect.right - clientRect.left);
            g_window.height = (uint32_t)(clientRect.bottom - clientRect.top);
        }

        dynamic_array::construct(g_window.characterInput, allocators::persistent(), 64);

        SetWindowLongPtr(g_window.handle, GWLP_USERDATA, (LONG_PTR)&g_window);
        ShowWindow(g_window.handle, settings.isFullscreenWindow ? SW_MAXIMIZE : SW_SHOW);
        UpdateWindow(g_window.handle);

        engine::update();
    }

    void engine::update()
    {
        dynamic_array::reset(g_window.characterInput);
        memcpy(g_window.input.keyboardButtonsPrevious, g_window.input.keyboardButtonsCurrent, sizeof(g_window.input.keyboardButtonsPrevious));
        g_window.input.mouseButtonsPrevious = g_window.input.mouseButtonsCurrent;
        const uint32_t prevWidth = g_window.width;
        const uint32_t prevHeight = g_window.height;
        g_window.input.mouseWheel = 0;
        MSG msg = { };
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    void engine::terminate()
    {
        dynamic_array::destroy(g_window.characterInput);
        DestroyWindow(g_window.handle);
        engine::update();
    }

    LRESULT win32_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        SeWindow* const window = (SeWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        if (window)
        {
            if (win32_window_process_mouse_input(window, uMsg, wParam, lParam))
            {
                return 0;
            }
            if (win32_window_process_keyboard_input(window, uMsg, wParam, lParam))
            {
                return 0;
            }
            switch (uMsg)
            {
            case WM_CLOSE:
                g_window.input.isCloseButtonPressed = true;
                return 0;
            case WM_SIZE:
                g_window.width = uint32_t(LOWORD(lParam));
                g_window.height = uint32_t(HIWORD(lParam));
                return 0;
            case WM_CHAR:
                // @TODO : support surrogate pairs
                // https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-char
                // https://learn.microsoft.com/en-us/windows/win32/intl/surrogates-and-supplementary-characters
                const wchar_t w = wchar_t(wParam);
                const SeUtf32Char character = unicode::to_utf32(w);
                // Special characters are processed in win32_window_process_keyboard_input
                if (!unicode::is_special_character(character))
                {
                    dynamic_array::push(g_window.characterInput, { .type = SeCharacterInput::CHARACTER, .character = character  });
                }
                return 0;
            }
        }
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    bool win32_window_process_mouse_input(SeWindow* window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        #define BIT(bit) (1 << bit)
        #define LOGMSG(msg, ...)
        switch (message)
        {
            case WM_LBUTTONDOWN:
            {
                g_window.input.mouseButtonsCurrent |= BIT((uint32_t)(SeMouse::LMB));
                LOGMSG(WM_LBUTTONDOWN);
                SetCapture(g_window.handle);
                break;
            }
            case WM_LBUTTONUP:
            {
                g_window.input.mouseButtonsCurrent &= ~BIT((uint32_t)(SeMouse::LMB));
                LOGMSG(WM_LBUTTONUP);
                ReleaseCapture();
                break;
            }
            case WM_MBUTTONDOWN:
            {
                g_window.input.mouseButtonsCurrent |= BIT((uint32_t)(SeMouse::MMB));
                LOGMSG(WM_MBUTTONDOWN);
                SetCapture(g_window.handle);
                break;
            }
            case WM_MBUTTONUP:
            {
                g_window.input.mouseButtonsCurrent &= ~BIT((uint32_t)(SeMouse::MMB));
                LOGMSG(WM_MBUTTONUP);
                ReleaseCapture();
                break;
            }
            case WM_RBUTTONDOWN:
            {
                g_window.input.mouseButtonsCurrent |= BIT((uint32_t)(SeMouse::RMB));
                LOGMSG(WM_RBUTTONDOWN);
                SetCapture(g_window.handle);
                break;
            }
            case WM_RBUTTONUP:
            {
                g_window.input.mouseButtonsCurrent &= ~BIT((uint32_t)(SeMouse::RMB));
                LOGMSG(WM_RBUTTONUP);
                ReleaseCapture();
                break;
            }
            case WM_XBUTTONDOWN:
            {
                LOGMSG(WM_XBUTTONDOWN);
                SetCapture(g_window.handle);
                break;
            }
            case WM_XBUTTONUP:
            {
                LOGMSG(WM_XBUTTONUP);
                ReleaseCapture();
                break;
            }
            case WM_MOUSEMOVE:
            {
                break;
            }
            case WM_MOUSEWHEEL:
            {
                g_window.input.mouseWheel += GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
                LOGMSG(WM_MOUSEWHEEL, " ", GET_WHEEL_DELTA_WPARAM(wParam))
                break;
            }
            default:
            {
                return false;
            }
        }
        g_window.input.mouseX = GET_X_LPARAM(lParam);
        g_window.input.mouseY = ((int64_t)g_window.height) - GET_Y_LPARAM(lParam);
        return true;
        #undef LOGMSG
        #undef BIT
    }

    bool win32_window_process_keyboard_input(SeWindow* window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        #define BIT(bit) (1ULL << (bit))
        #define k(key) (uint64_t)(key)
        static const uint64_t VK_CODE_TO_KEYBOARD_FLAGS[] =
        {
            k(SeKeyboard::NONE),        // 0x00 - none
            k(SeKeyboard::NONE),        // 0x01 - lmb
            k(SeKeyboard::NONE),        // 0x02 - rmb
            k(SeKeyboard::NONE),        // 0x03 - control-break processing
            k(SeKeyboard::NONE),        // 0x04 - mmb
            k(SeKeyboard::NONE),        // 0x05 - x1 mouse button
            k(SeKeyboard::NONE),        // 0x06 - x2 mouse button
            k(SeKeyboard::NONE),        // 0x07 - undefined
            k(SeKeyboard::BACKSPACE),   // 0x08 - backspace
            k(SeKeyboard::TAB),         // 0x09 - tab
            k(SeKeyboard::NONE),        // 0x0A - reserved
            k(SeKeyboard::NONE),        // 0x0B - reserved
            k(SeKeyboard::NONE),        // 0x0C - clear key
            k(SeKeyboard::ENTER),       // 0x0D - enter
            k(SeKeyboard::NONE),        // 0x0E - undefined
            k(SeKeyboard::NONE),        // 0x0F - undefined
            k(SeKeyboard::SHIFT),       // 0x10 - shift
            k(SeKeyboard::CTRL),        // 0x11 - ctrl
            k(SeKeyboard::ALT),         // 0x12 - alt
            k(SeKeyboard::NONE),        // 0x13 - pause
            k(SeKeyboard::CAPS_LOCK),   // 0x14 - caps lock
            k(SeKeyboard::NONE),        // 0x15 - IME Kana mode / Handuel mode
            k(SeKeyboard::NONE),        // 0x16 - IME On
            k(SeKeyboard::NONE),        // 0x17 - IME Junja mode
            k(SeKeyboard::NONE),        // 0x18 - IME Final mode
            k(SeKeyboard::NONE),        // 0x19 - IME Hanja mode / Kanji mode
            k(SeKeyboard::NONE),        // 0x1A - IME Off
            k(SeKeyboard::ESCAPE),      // 0x1B - ESC key
            k(SeKeyboard::NONE),        // 0x1C - IME convert
            k(SeKeyboard::NONE),        // 0x1D - IME nonconvert
            k(SeKeyboard::NONE),        // 0x1E - IME accept
            k(SeKeyboard::NONE),        // 0x1F - IME mode change request
            k(SeKeyboard::SPACE),       // 0x20 - spacebar
            k(SeKeyboard::NONE),        // 0x21 - page up
            k(SeKeyboard::NONE),        // 0x22 - page down
            k(SeKeyboard::END),         // 0x23 - end
            k(SeKeyboard::HOME),        // 0x24 - home
            k(SeKeyboard::ARROW_LEFT),  // 0x25 - left arrow
            k(SeKeyboard::ARROW_UP),    // 0x26 - up arrow
            k(SeKeyboard::ARROW_RIGHT), // 0x27 - right arrow
            k(SeKeyboard::ARROW_DOWN),  // 0x28 - down arrow
            k(SeKeyboard::NONE),        // 0x29 - select
            k(SeKeyboard::NONE),        // 0x2A - print
            k(SeKeyboard::NONE),        // 0x2B - execute
            k(SeKeyboard::NONE),        // 0x2C - print screen
            k(SeKeyboard::NONE),        // 0x2D - ins
            k(SeKeyboard::DEL),         // 0x2E - del
            k(SeKeyboard::NONE),        // 0x2F - help
            k(SeKeyboard::NUM_0),       // 0x30 - 0
            k(SeKeyboard::NUM_1),       // 0x31 - 1
            k(SeKeyboard::NUM_2),       // 0x32 - 2
            k(SeKeyboard::NUM_3),       // 0x33 - 3
            k(SeKeyboard::NUM_4),       // 0x34 - 4
            k(SeKeyboard::NUM_5),       // 0x35 - 5
            k(SeKeyboard::NUM_6),       // 0x36 - 6
            k(SeKeyboard::NUM_7),       // 0x37 - 7
            k(SeKeyboard::NUM_8),       // 0x38 - 8
            k(SeKeyboard::NUM_9),       // 0x39 - 9
            k(SeKeyboard::NONE),        // 0x3A - undefined
            k(SeKeyboard::NONE),        // 0x3B - undefined
            k(SeKeyboard::NONE),        // 0x3C - undefined
            k(SeKeyboard::NONE),        // 0x3D - undefined
            k(SeKeyboard::NONE),        // 0x3E - undefined
            k(SeKeyboard::NONE),        // 0x3F - undefined
            k(SeKeyboard::NONE),        // 0x40 - undefined
            k(SeKeyboard::A),           // 0x41 - A
            k(SeKeyboard::B),           // 0x42 - B
            k(SeKeyboard::C),           // 0x43 - C
            k(SeKeyboard::D),           // 0x44 - D
            k(SeKeyboard::E),           // 0x45 - E
            k(SeKeyboard::F),           // 0x46 - F
            k(SeKeyboard::G),           // 0x47 - G
            k(SeKeyboard::H),           // 0x48 - H
            k(SeKeyboard::I),           // 0x49 - I
            k(SeKeyboard::G),           // 0x4A - G
            k(SeKeyboard::K),           // 0x4B - K
            k(SeKeyboard::L),           // 0x4C - L
            k(SeKeyboard::M),           // 0x4D - M
            k(SeKeyboard::N),           // 0x4E - N
            k(SeKeyboard::O),           // 0x4F - O
            k(SeKeyboard::P),           // 0x50 - P
            k(SeKeyboard::Q),           // 0x51 - Q
            k(SeKeyboard::R),           // 0x52 - R
            k(SeKeyboard::S),           // 0x53 - S
            k(SeKeyboard::T),           // 0x54 - T
            k(SeKeyboard::U),           // 0x55 - U
            k(SeKeyboard::V),           // 0x56 - V
            k(SeKeyboard::W),           // 0x57 - W
            k(SeKeyboard::X),           // 0x58 - X
            k(SeKeyboard::Y),           // 0x59 - Y
            k(SeKeyboard::Z),           // 0x5A - Z
            k(SeKeyboard::NONE),        // 0x5B - left windows
            k(SeKeyboard::NONE),        // 0x5C - right windows
            k(SeKeyboard::NONE),        // 0x5D - applications
            k(SeKeyboard::NONE),        // 0x5E - reserved
            k(SeKeyboard::NONE),        // 0x5F - computer sleep
            k(SeKeyboard::NONE),        // 0x60 - numeric keypad 0
            k(SeKeyboard::NONE),        // 0x61 - numeric keypad 1
            k(SeKeyboard::NONE),        // 0x62 - numeric keypad 2
            k(SeKeyboard::NONE),        // 0x63 - numeric keypad 3
            k(SeKeyboard::NONE),        // 0x64 - numeric keypad 4
            k(SeKeyboard::NONE),        // 0x65 - numeric keypad 5
            k(SeKeyboard::NONE),        // 0x66 - numeric keypad 6
            k(SeKeyboard::NONE),        // 0x67 - numeric keypad 7
            k(SeKeyboard::NONE),        // 0x68 - numeric keypad 8
            k(SeKeyboard::NONE),        // 0x69 - numeric keypad 9
            k(SeKeyboard::NONE),        // 0x6A - numeric keypad multiply
            k(SeKeyboard::NONE),        // 0x6B - numeric keypad add
            k(SeKeyboard::NONE),        // 0x6C - numeric keypad separator
            k(SeKeyboard::NONE),        // 0x6D - numeric keypad subtract
            k(SeKeyboard::NONE),        // 0x6E - numeric keypad decimal
            k(SeKeyboard::NONE),        // 0x6F - numeric keypad divide
            k(SeKeyboard::F1),          // 0x70 - F1
            k(SeKeyboard::F2),          // 0x71 - F2
            k(SeKeyboard::F3),          // 0x72 - F3
            k(SeKeyboard::F4),          // 0x73 - F4
            k(SeKeyboard::F5),          // 0x74 - F5
            k(SeKeyboard::F6),          // 0x75 - F6
            k(SeKeyboard::F7),          // 0x76 - F7
            k(SeKeyboard::F8),          // 0x77 - F8
            k(SeKeyboard::F9),          // 0x78 - F9
            k(SeKeyboard::F10),         // 0x79 - F10
            k(SeKeyboard::F11),         // 0x7A - F11
            k(SeKeyboard::F12),         // 0x7B - F12
            k(SeKeyboard::NONE),        // 0x7C - F13
            k(SeKeyboard::NONE),        // 0x7D - F14
            k(SeKeyboard::NONE),        // 0x7E - F15
            k(SeKeyboard::NONE),        // 0x7F - F16
            k(SeKeyboard::NONE),        // 0x80 - F17
            k(SeKeyboard::NONE),        // 0x81 - F18
            k(SeKeyboard::NONE),        // 0x82 - F19
            k(SeKeyboard::NONE),        // 0x83 - F20
            k(SeKeyboard::NONE),        // 0x84 - F21
            k(SeKeyboard::NONE),        // 0x85 - F22
            k(SeKeyboard::NONE),        // 0x86 - F23
            k(SeKeyboard::NONE),        // 0x87 - F24
            k(SeKeyboard::NONE),        // 0x88 - unassigned
            k(SeKeyboard::NONE),        // 0x89 - unassigned
            k(SeKeyboard::NONE),        // 0x8A - unassigned
            k(SeKeyboard::NONE),        // 0x8B - unassigned
            k(SeKeyboard::NONE),        // 0x8C - unassigned
            k(SeKeyboard::NONE),        // 0x8D - unassigned
            k(SeKeyboard::NONE),        // 0x8E - unassigned
            k(SeKeyboard::NONE),        // 0x8F - unassigned
            k(SeKeyboard::NONE),        // 0x90 - num lock
            k(SeKeyboard::NONE),        // 0x91 - scroll lock
            k(SeKeyboard::NONE),        // 0x92 - OEM specific
            k(SeKeyboard::NONE),        // 0x93 - OEM specific
            k(SeKeyboard::NONE),        // 0x94 - OEM specific
            k(SeKeyboard::NONE),        // 0x95 - OEM specific
            k(SeKeyboard::NONE),        // 0x96 - OEM specific
            k(SeKeyboard::NONE),        // 0x97 - unassigned
            k(SeKeyboard::NONE),        // 0x98 - unassigned
            k(SeKeyboard::NONE),        // 0x99 - unassigned
            k(SeKeyboard::NONE),        // 0x9A - unassigned
            k(SeKeyboard::NONE),        // 0x9B - unassigned
            k(SeKeyboard::NONE),        // 0x9C - unassigned
            k(SeKeyboard::NONE),        // 0x9D - unassigned
            k(SeKeyboard::NONE),        // 0x9E - unassigned
            k(SeKeyboard::NONE),        // 0x9F - unassigned
            k(SeKeyboard::L_SHIFT),     // 0xA0 - left shift
            k(SeKeyboard::R_SHIFT),     // 0xA1 - right shift
            k(SeKeyboard::L_CTRL),      // 0xA2 - left ctrl
            k(SeKeyboard::R_CTRL),      // 0xA3 - right ctrl
            k(SeKeyboard::NONE),        // 0xA4 - left menu
            k(SeKeyboard::NONE),        // 0xA5 - right menu
            k(SeKeyboard::NONE),        // 0xA6 - browser back
            k(SeKeyboard::NONE),        // 0xA7 - browser forward
            k(SeKeyboard::NONE),        // 0xA8 - browser refresh
            k(SeKeyboard::NONE),        // 0xA9 - browser stop
            k(SeKeyboard::NONE),        // 0xAA - browser search
            k(SeKeyboard::NONE),        // 0xAB - browser favorites
            k(SeKeyboard::NONE),        // 0xAC - browser start and home
            k(SeKeyboard::NONE),        // 0xAD - volume mute
            k(SeKeyboard::NONE),        // 0xAE - volume down
            k(SeKeyboard::NONE),        // 0xAF - volume up
            k(SeKeyboard::NONE),        // 0xB0 - next track
            k(SeKeyboard::NONE),        // 0xB1 - previous track
            k(SeKeyboard::NONE),        // 0xB2 - stop media
            k(SeKeyboard::NONE),        // 0xB3 - play / pause media
            k(SeKeyboard::NONE),        // 0xB4 - start mail
            k(SeKeyboard::NONE),        // 0xB5 - select media
            k(SeKeyboard::NONE),        // 0xB6 - start app 1
            k(SeKeyboard::NONE),        // 0xB7 - start app 2
            k(SeKeyboard::NONE),        // 0xB8 - reserved
            k(SeKeyboard::NONE),        // 0xB9 - reserved
            k(SeKeyboard::SEMICOLON),   // 0xBA - ';:' for US keyboard
            k(SeKeyboard::EQUALS),      // 0xBB - plus / equals
            k(SeKeyboard::COMMA),       // 0xBC - comma
            k(SeKeyboard::MINUS),       // 0xBD - minus
            k(SeKeyboard::PERIOD),      // 0xBE - period
            k(SeKeyboard::SLASH),       // 0xBF - '/?' for US keyboard
            k(SeKeyboard::TILDA),       // 0xC0 - '~' for US keyboard
            k(SeKeyboard::NONE),        // 0xC1 - reserved
            k(SeKeyboard::NONE),        // 0xC2 - reserved
            k(SeKeyboard::NONE),        // 0xC3 - reserved
            k(SeKeyboard::NONE),        // 0xC4 - reserved
            k(SeKeyboard::NONE),        // 0xC5 - reserved
            k(SeKeyboard::NONE),        // 0xC6 - reserved
            k(SeKeyboard::NONE),        // 0xC7 - reserved
            k(SeKeyboard::NONE),        // 0xC8 - reserved
            k(SeKeyboard::NONE),        // 0xC9 - reserved
            k(SeKeyboard::NONE),        // 0xCA - reserved
            k(SeKeyboard::NONE),        // 0xCB - reserved
            k(SeKeyboard::NONE),        // 0xCC - reserved
            k(SeKeyboard::NONE),        // 0xCD - reserved
            k(SeKeyboard::NONE),        // 0xCE - reserved
            k(SeKeyboard::NONE),        // 0xCF - reserved
            k(SeKeyboard::NONE),        // 0xD0 - reserved
            k(SeKeyboard::NONE),        // 0xD1 - reserved
            k(SeKeyboard::NONE),        // 0xD2 - reserved
            k(SeKeyboard::NONE),        // 0xD3 - reserved
            k(SeKeyboard::NONE),        // 0xD4 - reserved
            k(SeKeyboard::NONE),        // 0xD5 - reserved
            k(SeKeyboard::NONE),        // 0xD6 - reserved
            k(SeKeyboard::NONE),        // 0xD7 - reserved
            k(SeKeyboard::NONE),        // 0xD8 - unassigned
            k(SeKeyboard::NONE),        // 0xD9 - unassigned
            k(SeKeyboard::NONE),        // 0xDA - unassigned
            k(SeKeyboard::L_BRACKET),   // 0xDB - '[{' for US keyboard
            k(SeKeyboard::BACKSLASH),   // 0xDC - '\|' for US keyboard
            k(SeKeyboard::R_BRACKET),   // 0xDD - ']}' for US keyboard
            k(SeKeyboard::APOSTROPHE),  // 0xDE - ''"' for US keyboard
            k(SeKeyboard::NONE),        // 0xDF - misc char
            k(SeKeyboard::NONE),        // 0xE0 - reserved
            k(SeKeyboard::NONE),        // 0xE1 - OEM specific
            k(SeKeyboard::NONE),        // 0xE2 - VK_OEM_102
            k(SeKeyboard::NONE),        // 0xE3 - OEM specific
            k(SeKeyboard::NONE),        // 0xE4 - OEM specific
            k(SeKeyboard::NONE),        // 0xE5 - IME process
            k(SeKeyboard::NONE),        // 0xE6 - OEM specific
            k(SeKeyboard::NONE),        // 0xE7 - VK_PACKET
            k(SeKeyboard::NONE),        // 0xE8 - unassigned
            k(SeKeyboard::NONE),        // 0xE9 - OEM specific
            k(SeKeyboard::NONE),        // 0xEA - OEM specific
            k(SeKeyboard::NONE),        // 0xEB - OEM specific
            k(SeKeyboard::NONE),        // 0xEC - OEM specific
            k(SeKeyboard::NONE),        // 0xED - OEM specific
            k(SeKeyboard::NONE),        // 0xEE - OEM specific
            k(SeKeyboard::NONE),        // 0xEF - OEM specific
            k(SeKeyboard::NONE),        // 0xF0 - OEM specific
            k(SeKeyboard::NONE),        // 0xF1 - OEM specific
            k(SeKeyboard::NONE),        // 0xF2 - OEM specific
            k(SeKeyboard::NONE),        // 0xF3 - OEM specific
            k(SeKeyboard::NONE),        // 0xF4 - OEM specific
            k(SeKeyboard::NONE),        // 0xF5 - OEM specific
            k(SeKeyboard::NONE),        // 0xF6 - Attn
            k(SeKeyboard::NONE),        // 0xF7 - CrSel
            k(SeKeyboard::NONE),        // 0xF8 - ExSel
            k(SeKeyboard::NONE),        // 0xF9 - Erase EOF
            k(SeKeyboard::NONE),        // 0xFA - Play
            k(SeKeyboard::NONE),        // 0xFB - Zoom
            k(SeKeyboard::NONE),        // 0xFC - reserved
            k(SeKeyboard::NONE),        // 0xFD - PA1
            k(SeKeyboard::NONE)         // 0xFE - clear
        };
        switch (message)
        {
            case WM_KEYDOWN:
            {
                const uint64_t flag = VK_CODE_TO_KEYBOARD_FLAGS[wParam];
                const uint64_t id = flag / 64ULL;
                g_window.input.keyboardButtonsCurrent[id] |= BIT(flag - id * 64);

                switch (SeKeyboard::Type keyboardFlag = SeKeyboard::Type(flag))
                {
                    case SeKeyboard::BACKSPACE:
                    case SeKeyboard::DEL:
                    case SeKeyboard::ENTER:
                    case SeKeyboard::TAB:
                    case SeKeyboard::ARROW_LEFT:
                    case SeKeyboard::ARROW_RIGHT:
                    case SeKeyboard::HOME:
                    case SeKeyboard::END:
                        dynamic_array::push(g_window.characterInput, { .type = SeCharacterInput::SPECIAL, .special = keyboardFlag });
                }

                break;
            }
            case WM_KEYUP:
            {
                const uint64_t flag = VK_CODE_TO_KEYBOARD_FLAGS[wParam];
                const uint64_t id = flag / 64ULL;
                g_window.input.keyboardButtonsCurrent[id] &= ~BIT(flag - id * 64);
                break;
            }
            default:
            {
                return false;
            }
        }
        return true;
        #undef k
        #undef BIT
    }
}

#else
#   error Unsupported platform
#endif
