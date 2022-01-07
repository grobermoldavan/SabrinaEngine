
#include "se_window_subsystem.h"
#include "engine/engine.h"

static void fill_iface(SeWindowSubsystemInterface* iface);
static void process_windows();
static void destroy_windows();

static SeWindowSubsystemInterface g_Iface;

SE_DLL_EXPORT void se_init(SabrinaEngine* engine)
{
    fill_iface(&g_Iface);
}

SE_DLL_EXPORT void se_terminate(SabrinaEngine* engine)
{
    destroy_windows();
}

SE_DLL_EXPORT void se_update(SabrinaEngine* engine, const SeUpdateInfo* info)
{
    process_windows();
}

SE_DLL_EXPORT void* se_get_interface(SabrinaEngine* engine)
{
    return &g_Iface;
}

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Windowsx.h>
#include <string.h>

typedef struct SeWindowResizeCallbackWin32
{
    void* userData;
    void (*callback)(SeWindowHandle handle, void* userData);
} SeWindowResizeCallbackWin32;

typedef struct SeWindowResizeCallbackContainerWin32
{
    SeWindowResizeCallbackWin32 callbacks[32];
    size_t numCallbacks;
} SeWindowResizeCallbackContainerWin32;

typedef struct SeWindowWin32
{
    SeWindowResizeCallbackContainerWin32 resizeCallbacks[__SE_WINDOW_RESIZE_CALLBACK_PRIORITY_COUNT];
    SeWindowSubsystemInput input;
    HWND handle;
    uint32_t width;
    uint32_t height;
} SeWindowWin32;

SeWindowWin32 g_windowPool[4] = {0};

static SeWindowHandle                   win32_window_create(SeWindowSubsystemCreateInfo* createInfo);
static void                             win32_window_destroy(SeWindowHandle handle);
static const SeWindowSubsystemInput*    win32_window_get_input(SeWindowHandle handle);
static uint32_t                         win32_window_get_width(SeWindowHandle handle);
static uint32_t                         win32_window_get_height(SeWindowHandle handle);
static void*                            win32_window_get_native_handle(SeWindowHandle handle);
static SeWindowResizeCallbackHandle     win32_window_add_resize_callback(SeWindowHandle handle, SeWindowResizeCallbackInfo* callbackInfo);
static void                             win32_window_remove_resize_callback(SeWindowHandle handle, SeWindowResizeCallbackHandle cbHandle);
static void                             win32_window_process(SeWindowWin32* window);
static LRESULT                          win32_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static bool                             win32_window_process_mouse_input(SeWindowWin32* window, UINT message, WPARAM wParam, LPARAM lParam);
static bool                             win32_window_process_keyboard_input(SeWindowWin32* window, UINT message, WPARAM wParam, LPARAM lParam);

static void fill_iface(SeWindowSubsystemInterface* iface)
{
    *iface = (SeWindowSubsystemInterface)
    {
        .create = win32_window_create,
        .destroy = win32_window_destroy,
        .get_input = win32_window_get_input,
        .get_width = win32_window_get_width,
        .get_height = win32_window_get_height,
        .get_native_handle = win32_window_get_native_handle,
        .add_resize_callback = win32_window_add_resize_callback,
        .remove_resize_callback = win32_window_remove_resize_callback,
    };
}

static void process_windows()
{
    for (size_t i = 0; i < se_array_size(g_windowPool); i++)
        if (g_windowPool[i].handle)
            win32_window_process(&g_windowPool[i]);
}

static void destroy_windows()
{
    for (size_t i = 0; i < se_array_size(g_windowPool); i++)
        if (g_windowPool[i].handle)
            win32_window_destroy((SeWindowHandle){ &g_windowPool[i] });
}

static SeWindowWin32* win32_get_new_window()
{
    for (size_t i = 0; i < se_array_size(g_windowPool); i++)
        if (g_windowPool[i].handle == NULL)
            return &g_windowPool[i];
    return NULL;
}

static SeWindowHandle win32_window_create(SeWindowSubsystemCreateInfo* createInfo)
{
    SeWindowWin32* window = win32_get_new_window();
    memset(window, 0, sizeof(SeWindowWin32));

    HMODULE moduleHandle = GetModuleHandle(NULL);

    WNDCLASSA wc = {0};
    wc.lpfnWndProc      = win32_window_proc;
    wc.hInstance        = moduleHandle;
    wc.lpszClassName    = createInfo->name;
    wc.hCursor          = LoadCursor(NULL, IDC_ARROW);

    const bool isClassRegistered = RegisterClassA(&wc);
    // al_assert(isClassRegistered);

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

    DWORD activeStyle = createInfo->isFullscreen ? fullscreenStyle : (createInfo->isResizable ? windowedStyleResize : windowedStyleNoResize);

    // @TODO : safe cast from uint32_t to int
    int targetWidth = (int)createInfo->width;
    int targetHeight = (int)createInfo->height;

    window->handle = CreateWindowExA(
        0,                  // Optional window styles.
        createInfo->name,   // Window class
        createInfo->name,   // Window text
        activeStyle,        // Window styles
        CW_USEDEFAULT,      // Position X
        CW_USEDEFAULT,      // Position Y
        targetWidth,        // Size X
        targetHeight,       // Size Y
        NULL,               // Parent window
        NULL,               // Menu
        moduleHandle,       // Instance handle
        NULL                // Additional application data
    );
    // al_assert(window->handle);
    if (!createInfo->isFullscreen)
    {
        // Set correct size for a client rect
        // Retrieve currecnt client rect size and calculate difference with target size
        RECT clientRect;
        bool getClientRectResult = GetClientRect(window->handle, &clientRect);
        // al_assert(getClientRectResult);
        int widthDiff = targetWidth - (clientRect.right - clientRect.left);
        int heightDiff = targetHeight - (clientRect.bottom - clientRect.top);
        // Retrieve currecnt window rect size (this includes borders!)
        RECT windowRect;
        bool getWindowRectResult = GetWindowRect(window->handle, &windowRect);
        // al_assert(getWindowRectResult);
        int windowWidth = windowRect.right - windowRect.left;
        int windowHeight = windowRect.bottom - windowRect.top;
        // Set new window size based on calculated diff
        bool setWindowPosResult = SetWindowPos(window->handle, NULL, windowRect.left, windowRect.top, windowWidth + widthDiff, windowHeight + heightDiff, SWP_NOMOVE | SWP_NOZORDER);
        // al_assert(setWindowPosResult);
        // Retrieve resulting size
        getClientRectResult = GetClientRect(window->handle, &clientRect);
        // al_assert(getClientRectResult);
        window->width = (uint32_t)(clientRect.right - clientRect.left);
        window->height = (uint32_t)(clientRect.bottom - clientRect.top);
    }
    SetWindowLongPtr(window->handle, GWLP_USERDATA, (LONG_PTR)window);
    ShowWindow(window->handle, createInfo->isFullscreen ? SW_MAXIMIZE : SW_SHOW);
    UpdateWindow(window->handle);
    win32_window_process(window);

    return (SeWindowHandle){ window };
}

static void win32_window_destroy(SeWindowHandle handle)
{
    SeWindowWin32* window = (SeWindowWin32*)handle.ptr;
    DestroyWindow(window->handle);
    win32_window_process(window);
    memset(window, 0, sizeof(SeWindowWin32));
}

static const SeWindowSubsystemInput* win32_window_get_input(SeWindowHandle handle)
{
    SeWindowWin32* window = (SeWindowWin32*)handle.ptr;
    return &window->input;
}

static uint32_t win32_window_get_width(SeWindowHandle handle)
{
    SeWindowWin32* window = (SeWindowWin32*)handle.ptr;
    return window->width;
}

static uint32_t win32_window_get_height(SeWindowHandle handle)
{
    SeWindowWin32* window = (SeWindowWin32*)handle.ptr;
    return window->height;
}

static void* win32_window_get_native_handle(SeWindowHandle handle)
{
    SeWindowWin32* window = (SeWindowWin32*)handle.ptr;
    return window->handle;
}

static SeWindowResizeCallbackHandle win32_window_add_resize_callback(SeWindowHandle handle, SeWindowResizeCallbackInfo* callbackInfo)
{
    se_assert(callbackInfo->priority < __SE_WINDOW_RESIZE_CALLBACK_PRIORITY_COUNT);
    se_assert(callbackInfo->priority > __SE_WINDOW_RESIZE_CALLBACK_PRIORITY_UNDEFINED);
    SeWindowWin32* window = (SeWindowWin32*)handle.ptr;
    SeWindowResizeCallbackContainerWin32* callbackContainer = &window->resizeCallbacks[callbackInfo->priority];
    se_assert(callbackContainer->numCallbacks != se_array_size(callbackContainer->callbacks));
    callbackContainer->callbacks[callbackContainer->numCallbacks] = (SeWindowResizeCallbackWin32) { callbackInfo->userData, callbackInfo->callback, };
    return (SeWindowResizeCallbackHandle)(callbackContainer->callbacks + (callbackContainer->numCallbacks++));
}

static void win32_window_remove_resize_callback(SeWindowHandle handle, SeWindowResizeCallbackHandle cbHandle)
{
    SeWindowWin32* window = (SeWindowWin32*)handle.ptr;
    SeWindowResizeCallbackWin32* callback = (SeWindowResizeCallbackWin32*)cbHandle;
    for (size_t it = 0; it < __SE_WINDOW_RESIZE_CALLBACK_PRIORITY_COUNT; it++)
    {
        SeWindowResizeCallbackContainerWin32* callbackContainer = &window->resizeCallbacks[it];
        const ptrdiff_t offset = callback - callbackContainer->callbacks;
        if (offset < se_array_size(callbackContainer->callbacks))
        {
            se_assert(callbackContainer->numCallbacks != 0);
            callbackContainer->callbacks[offset] = callbackContainer->callbacks[callbackContainer->numCallbacks - 1];
            callbackContainer->numCallbacks -= 1;
            return;
        }
    }
    se_assert_msg(false, "Trying to remove wrong resize callback");
}

static void win32_window_process(SeWindowWin32* window)
{
    memcpy(window->input.keyboardButtonsPrevious, window->input.keyboardButtonsCurrent, sizeof(window->input.keyboardButtonsPrevious));
    const uint32_t prevWidth = window->width;
    const uint32_t prevHeight = window->height;
    MSG msg = {0};
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    if (window->width != prevWidth || window->height != prevHeight)
        for (SeWindowResizeCallbackPriority priority = __SE_WINDOW_RESIZE_CALLBACK_PRIORITY_UNDEFINED + 1; priority < __SE_WINDOW_RESIZE_CALLBACK_PRIORITY_COUNT; priority++)
            for (size_t it = 0; it < window->resizeCallbacks[priority].numCallbacks; it++)
            {
                SeWindowResizeCallbackWin32* callback = &window->resizeCallbacks[priority].callbacks[it];
                callback->callback((SeWindowHandle){ window }, callback->userData);
            }
}

static LRESULT win32_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SeWindowWin32* window = (SeWindowWin32*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
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
            window->input.isCloseButtonPressed = true;
            return 0;
        case WM_SIZE:
            window->width = (uint32_t)(LOWORD(lParam));
            window->height = (uint32_t)(HIWORD(lParam));
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static bool win32_window_process_mouse_input(SeWindowWin32* window, UINT message, WPARAM wParam, LPARAM lParam)
{
    #define BIT(bit) (1 << bit)
    #define LOGMSG(msg, ...)
    switch (message)
    {
        case WM_LBUTTONDOWN:
        {
            window->input.mouseButtons |= BIT((uint32_t)(SE_LMB));
            LOGMSG(WM_LBUTTONDOWN);
            SetCapture(window->handle);
            break;
        }
        case WM_LBUTTONUP:
        {
            window->input.mouseButtons &= ~BIT((uint32_t)(SE_LMB));
            LOGMSG(WM_LBUTTONUP);
            ReleaseCapture();
            break;
        }
        case WM_MBUTTONDOWN:
        {
            window->input.mouseButtons |= BIT((uint32_t)(SE_MMB));
            LOGMSG(WM_MBUTTONDOWN);
            SetCapture(window->handle);
            break;
        }
        case WM_MBUTTONUP:
        {
            window->input.mouseButtons &= ~BIT((uint32_t)(SE_MMB));
            LOGMSG(WM_MBUTTONUP);
            ReleaseCapture();
            break;
        }
        case WM_RBUTTONDOWN:
        {
            window->input.mouseButtons |= BIT((uint32_t)(SE_RMB));
            LOGMSG(WM_RBUTTONDOWN);
            SetCapture(window->handle);
            break;
        }
        case WM_RBUTTONUP:
        {
            window->input.mouseButtons &= ~BIT((uint32_t)(SE_RMB));
            LOGMSG(WM_RBUTTONUP);
            ReleaseCapture();
            break;
        }
        case WM_XBUTTONDOWN:
        {
            LOGMSG(WM_XBUTTONDOWN);
            SetCapture(window->handle);
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
            window->input.mouseWheel += GET_WHEEL_DELTA_WPARAM(wParam);
            LOGMSG(WM_MOUSEWHEEL, " ", GET_WHEEL_DELTA_WPARAM(wParam))
            break;
        }
        default:
        {
            return false;
        }
    }
    window->input.mouseX = GET_X_LPARAM(lParam);
    window->input.mouseY = GET_Y_LPARAM(lParam);
    return true;
    #undef LOGMSG
    #undef BIT
}

static bool win32_window_process_keyboard_input(SeWindowWin32* window, UINT message, WPARAM wParam, LPARAM lParam)
{
    #define BIT(bit) (1ULL << bit)
    #define k(key) (uint64_t)(key)
    static uint64_t VK_CODE_TO_KEYBOARD_FLAGS[] =
    {
        k(SE_NONE),        // 0x00 - none
        k(SE_NONE),        // 0x01 - lmb
        k(SE_NONE),        // 0x02 - rmb
        k(SE_NONE),        // 0x03 - control-break processing
        k(SE_NONE),        // 0x04 - mmb
        k(SE_NONE),        // 0x05 - x1 mouse button
        k(SE_NONE),        // 0x06 - x2 mouse button
        k(SE_NONE),        // 0x07 - undefined
        k(SE_BACKSPACE),   // 0x08 - backspace
        k(SE_TAB),         // 0x09 - tab
        k(SE_NONE),        // 0x0A - reserved
        k(SE_NONE),        // 0x0B - reserved
        k(SE_NONE),        // 0x0C - clear key
        k(SE_ENTER),       // 0x0D - enter
        k(SE_NONE),        // 0x0E - undefined
        k(SE_NONE),        // 0x0F - undefined
        k(SE_SHIFT),       // 0x10    - shift
        k(SE_CTRL),        // 0x11 - ctrl
        k(SE_ALT),         // 0x12 - alt
        k(SE_NONE),        // 0x13 - pause
        k(SE_CAPS_LOCK),   // 0x14 - caps lock
        k(SE_NONE),        // 0x15 - IME Kana mode / Handuel mode
        k(SE_NONE),        // 0x16 - IME On
        k(SE_NONE),        // 0x17 - IME Junja mode
        k(SE_NONE),        // 0x18 - IME Final mode
        k(SE_NONE),        // 0x19 - IME Hanja mode / Kanji mode
        k(SE_NONE),        // 0x1A - IME Off
        k(SE_ESCAPE),      // 0x1B - ESC key
        k(SE_NONE),        // 0x1C - IME convert
        k(SE_NONE),        // 0x1D - IME nonconvert
        k(SE_NONE),        // 0x1E - IME accept
        k(SE_NONE),        // 0x1F - IME mode change request
        k(SE_SPACE),       // 0x20 - spacebar
        k(SE_NONE),        // 0x21 - page up
        k(SE_NONE),        // 0x22 - page down
        k(SE_NONE),        // 0x23 - end
        k(SE_NONE),        // 0x24 - home
        k(SE_ARROW_LEFT),  // 0x25 - left arrow
        k(SE_ARROW_UP),    // 0x26 - up arrow
        k(SE_ARROW_RIGHT), // 0x27 - right arrow
        k(SE_ARROW_DOWN),  // 0x28 - down arrow
        k(SE_NONE),        // 0x29 - select
        k(SE_NONE),        // 0x2A - print
        k(SE_NONE),        // 0x2B - execute
        k(SE_NONE),        // 0x2C - print screen
        k(SE_NONE),        // 0x2D - ins
        k(SE_NONE),        // 0x2E - del
        k(SE_NONE),        // 0x2F - help
        k(SE_NUM_0),       // 0x30 - 0
        k(SE_NUM_1),       // 0x31 - 1
        k(SE_NUM_2),       // 0x32 - 2
        k(SE_NUM_3),       // 0x33 - 3
        k(SE_NUM_4),       // 0x34 - 4
        k(SE_NUM_5),       // 0x35 - 5
        k(SE_NUM_6),       // 0x36 - 6
        k(SE_NUM_7),       // 0x37 - 7
        k(SE_NUM_8),       // 0x38 - 8
        k(SE_NUM_9),       // 0x39 - 9
        k(SE_NONE),        // 0x3A - undefined
        k(SE_NONE),        // 0x3B - undefined
        k(SE_NONE),        // 0x3C - undefined
        k(SE_NONE),        // 0x3D - undefined
        k(SE_NONE),        // 0x3E - undefined
        k(SE_NONE),        // 0x3F - undefined
        k(SE_NONE),        // 0x40 - undefined
        k(SE_A),           // 0x41 - A
        k(SE_B),           // 0x42 - B
        k(SE_C),           // 0x43 - C
        k(SE_D),           // 0x44 - D
        k(SE_E),           // 0x45 - E
        k(SE_F),           // 0x46 - F
        k(SE_G),           // 0x47 - G
        k(SE_H),           // 0x48 - H
        k(SE_I),           // 0x49 - I
        k(SE_G),           // 0x4A - G
        k(SE_K),           // 0x4B - K
        k(SE_L),           // 0x4C - L
        k(SE_M),           // 0x4D - M
        k(SE_N),           // 0x4E - N
        k(SE_O),           // 0x4F - O
        k(SE_P),           // 0x50 - P
        k(SE_Q),           // 0x51 - Q
        k(SE_R),           // 0x52 - R
        k(SE_S),           // 0x53 - S
        k(SE_T),           // 0x54 - T
        k(SE_U),           // 0x55 - U
        k(SE_V),           // 0x56 - V
        k(SE_W),           // 0x57 - W
        k(SE_X),           // 0x58 - X
        k(SE_Y),           // 0x59 - Y
        k(SE_Z),           // 0x5A - Z
        k(SE_NONE),        // 0x5B - left windows
        k(SE_NONE),        // 0x5C - right windows
        k(SE_NONE),        // 0x5D - applications
        k(SE_NONE),        // 0x5E - reserved
        k(SE_NONE),        // 0x5F - computer sleep
        k(SE_NONE),        // 0x60 - numeric keypad 0
        k(SE_NONE),        // 0x61 - numeric keypad 1
        k(SE_NONE),        // 0x62 - numeric keypad 2
        k(SE_NONE),        // 0x63 - numeric keypad 3
        k(SE_NONE),        // 0x64 - numeric keypad 4
        k(SE_NONE),        // 0x65 - numeric keypad 5
        k(SE_NONE),        // 0x66 - numeric keypad 6
        k(SE_NONE),        // 0x67 - numeric keypad 7
        k(SE_NONE),        // 0x68 - numeric keypad 8
        k(SE_NONE),        // 0x69 - numeric keypad 9
        k(SE_NONE),        // 0x6A - numeric keypad multiply
        k(SE_NONE),        // 0x6B - numeric keypad add
        k(SE_NONE),        // 0x6C - numeric keypad separator
        k(SE_NONE),        // 0x6D - numeric keypad subtract
        k(SE_NONE),        // 0x6E - numeric keypad decimal
        k(SE_NONE),        // 0x6F - numeric keypad divide
        k(SE_F1),          // 0x70 - F1
        k(SE_F2),          // 0x71 - F2
        k(SE_F3),          // 0x72 - F3
        k(SE_F4),          // 0x73 - F4
        k(SE_F5),          // 0x74 - F5
        k(SE_F6),          // 0x75 - F6
        k(SE_F7),          // 0x76 - F7
        k(SE_F8),          // 0x77 - F8
        k(SE_F9),          // 0x78 - F9
        k(SE_F10),         // 0x79 - F10
        k(SE_F11),         // 0x7A - F11
        k(SE_F12),         // 0x7B - F12
        k(SE_NONE),        // 0x7C - F13
        k(SE_NONE),        // 0x7D - F14
        k(SE_NONE),        // 0x7E - F15
        k(SE_NONE),        // 0x7F - F16
        k(SE_NONE),        // 0x80 - F17
        k(SE_NONE),        // 0x81 - F18
        k(SE_NONE),        // 0x82 - F19
        k(SE_NONE),        // 0x83 - F20
        k(SE_NONE),        // 0x84 - F21
        k(SE_NONE),        // 0x85 - F22
        k(SE_NONE),        // 0x86 - F23
        k(SE_NONE),        // 0x87 - F24
        k(SE_NONE),        // 0x88 - unassigned
        k(SE_NONE),        // 0x89 - unassigned
        k(SE_NONE),        // 0x8A - unassigned
        k(SE_NONE),        // 0x8B - unassigned
        k(SE_NONE),        // 0x8C - unassigned
        k(SE_NONE),        // 0x8D - unassigned
        k(SE_NONE),        // 0x8E - unassigned
        k(SE_NONE),        // 0x8F - unassigned
        k(SE_NONE),        // 0x90 - num lock
        k(SE_NONE),        // 0x91 - scroll lock
        k(SE_NONE),        // 0x92 - OEM specific
        k(SE_NONE),        // 0x93 - OEM specific
        k(SE_NONE),        // 0x94 - OEM specific
        k(SE_NONE),        // 0x95 - OEM specific
        k(SE_NONE),        // 0x96 - OEM specific
        k(SE_NONE),        // 0x97 - unassigned
        k(SE_NONE),        // 0x98 - unassigned
        k(SE_NONE),        // 0x99 - unassigned
        k(SE_NONE),        // 0x9A - unassigned
        k(SE_NONE),        // 0x9B - unassigned
        k(SE_NONE),        // 0x9C - unassigned
        k(SE_NONE),        // 0x9D - unassigned
        k(SE_NONE),        // 0x9E - unassigned
        k(SE_NONE),        // 0x9F - unassigned
        k(SE_L_SHIFT),     // 0xA0 - left shift
        k(SE_R_SHIFT),     // 0xA1 - right shift
        k(SE_L_CTRL),      // 0xA2 - left ctrl
        k(SE_R_CTRL),      // 0xA3 - right ctrl
        k(SE_NONE),        // 0xA4 - left menu
        k(SE_NONE),        // 0xA5 - right menu
        k(SE_NONE),        // 0xA6 - browser back
        k(SE_NONE),        // 0xA7 - browser forward
        k(SE_NONE),        // 0xA8 - browser refresh
        k(SE_NONE),        // 0xA9 - browser stop
        k(SE_NONE),        // 0xAA - browser search
        k(SE_NONE),        // 0xAB - browser favorites
        k(SE_NONE),        // 0xAC - browser start and home
        k(SE_NONE),        // 0xAD - volume mute
        k(SE_NONE),        // 0xAE - volume down
        k(SE_NONE),        // 0xAF - volume up
        k(SE_NONE),        // 0xB0 - next track
        k(SE_NONE),        // 0xB1 - previous track
        k(SE_NONE),        // 0xB2 - stop media
        k(SE_NONE),        // 0xB3 - play / pause media
        k(SE_NONE),        // 0xB4 - start mail
        k(SE_NONE),        // 0xB5 - select media
        k(SE_NONE),        // 0xB6 - start app 1
        k(SE_NONE),        // 0xB7 - start app 2
        k(SE_NONE),        // 0xB8 - reserved
        k(SE_NONE),        // 0xB9 - reserved
        k(SE_SEMICOLON),   // 0xBA - ';:' for US keyboard
        k(SE_EQUALS),      // 0xBB - plus / equals
        k(SE_COMMA),       // 0xBC - comma
        k(SE_MINUS),       // 0xBD - minus
        k(SE_PERIOD),      // 0xBE - period
        k(SE_SLASH),       // 0xBF - '/?' for US keyboard
        k(SE_TILDA),       // 0xC0 - '~' for US keyboard
        k(SE_NONE),        // 0xC1 - reserved
        k(SE_NONE),        // 0xC2 - reserved
        k(SE_NONE),        // 0xC3 - reserved
        k(SE_NONE),        // 0xC4 - reserved
        k(SE_NONE),        // 0xC5 - reserved
        k(SE_NONE),        // 0xC6 - reserved
        k(SE_NONE),        // 0xC7 - reserved
        k(SE_NONE),        // 0xC8 - reserved
        k(SE_NONE),        // 0xC9 - reserved
        k(SE_NONE),        // 0xCA - reserved
        k(SE_NONE),        // 0xCB - reserved
        k(SE_NONE),        // 0xCC - reserved
        k(SE_NONE),        // 0xCD - reserved
        k(SE_NONE),        // 0xCE - reserved
        k(SE_NONE),        // 0xCF - reserved
        k(SE_NONE),        // 0xD0 - reserved
        k(SE_NONE),        // 0xD1 - reserved
        k(SE_NONE),        // 0xD2 - reserved
        k(SE_NONE),        // 0xD3 - reserved
        k(SE_NONE),        // 0xD4 - reserved
        k(SE_NONE),        // 0xD5 - reserved
        k(SE_NONE),        // 0xD6 - reserved
        k(SE_NONE),        // 0xD7 - reserved
        k(SE_NONE),        // 0xD8 - unassigned
        k(SE_NONE),        // 0xD9 - unassigned
        k(SE_NONE),        // 0xDA - unassigned
        k(SE_L_BRACKET),   // 0xDB - '[{' for US keyboard
        k(SE_BACKSLASH),   // 0xDC - '\|' for US keyboard
        k(SE_R_BRACKET),   // 0xDD - ']}' for US keyboard
        k(SE_APOSTROPHE),  // 0xDE - ''"' for US keyboard
        k(SE_NONE),        // 0xDF - misc char
        k(SE_NONE),        // 0xE0 - reserved
        k(SE_NONE),        // 0xE1 - OEM specific
        k(SE_NONE),        // 0xE2 - VK_OEM_102
        k(SE_NONE),        // 0xE3 - OEM specific
        k(SE_NONE),        // 0xE4 - OEM specific
        k(SE_NONE),        // 0xE5 - IME process
        k(SE_NONE),        // 0xE6 - OEM specific
        k(SE_NONE),        // 0xE7 - VK_PACKET
        k(SE_NONE),        // 0xE8 - unassigned
        k(SE_NONE),        // 0xE9 - OEM specific
        k(SE_NONE),        // 0xEA - OEM specific
        k(SE_NONE),        // 0xEB - OEM specific
        k(SE_NONE),        // 0xEC - OEM specific
        k(SE_NONE),        // 0xED - OEM specific
        k(SE_NONE),        // 0xEE - OEM specific
        k(SE_NONE),        // 0xEF - OEM specific
        k(SE_NONE),        // 0xF0 - OEM specific
        k(SE_NONE),        // 0xF1 - OEM specific
        k(SE_NONE),        // 0xF2 - OEM specific
        k(SE_NONE),        // 0xF3 - OEM specific
        k(SE_NONE),        // 0xF4 - OEM specific
        k(SE_NONE),        // 0xF5 - OEM specific
        k(SE_NONE),        // 0xF6 - Attn
        k(SE_NONE),        // 0xF7 - CrSel
        k(SE_NONE),        // 0xF8 - ExSel
        k(SE_NONE),        // 0xF9 - Erase EOF
        k(SE_NONE),        // 0xFA - Play
        k(SE_NONE),        // 0xFB - Zoom
        k(SE_NONE),        // 0xFC - reserved
        k(SE_NONE),        // 0xFD - PA1
        k(SE_NONE)         // 0xFE - clear
    };
    switch (message)
    {
        case WM_KEYDOWN:
        {
            uint64_t flag = VK_CODE_TO_KEYBOARD_FLAGS[wParam];
            uint64_t id = flag / 64ULL;
            window->input.keyboardButtonsCurrent[id] |= BIT(flag - id * 64);
            break;
        }
        case WM_KEYUP:
        {
            uint64_t flag = VK_CODE_TO_KEYBOARD_FLAGS[wParam];
            uint64_t id = flag / 64ULL;
            window->input.keyboardButtonsCurrent[id] &= ~BIT(flag - id * 64);
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

#else
#   error Unsupported platform
#endif
