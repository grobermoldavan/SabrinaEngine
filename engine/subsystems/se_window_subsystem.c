
#include "se_window_subsystem.h"
#include "engine/engine.h"

static struct SeWindowSubsystemInterface iface;

static void fill_iface(struct SeWindowSubsystemInterface* iface);

SE_DLL_EXPORT void se_init(struct SabrinaEngine* engine)
{

}

SE_DLL_EXPORT void se_terminate(struct SabrinaEngine* engine)
{

}

SE_DLL_EXPORT void se_update(struct SabrinaEngine* engine)
{

}

SE_DLL_EXPORT void* se_get_interface(struct SabrinaEngine* engine)
{
    return &iface;
}

#ifdef _WIN32

#include <Windows.h>
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

struct SeWindowWin32
{
    HWND handle;
    uint32_t width;
    uint32_t height;
};

static LRESULT window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static bool process_mouse_input(struct SeWindowWin32* window, UINT message, WPARAM wParam, LPARAM lParam);
static bool process_keyboard_input(struct SeWindowWin32* window, UINT message, WPARAM wParam, LPARAM lParam);

static void fill_iface(struct SeWindowSubsystemInterface* iface)
{
    
}

void platform_window_construct(struct SeWindowWin32* window, struct SeWindowSubsystemCreateInfo* createInfo)
{
    memset(window, 0, sizeof(struct SeWindowWin32));

    HMODULE moduleHandle = GetModuleHandle(NULL);
    
    WNDCLASSA wc = {0};
    wc.lpfnWndProc      = window_proc;
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
        createInfo->name,      // Window class
        createInfo->name,      // Window text
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
    // platform_window_process(window);
}

LRESULT window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    struct SeWindowWin32* window = (struct SeWindowWin32*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (window)
    {
        // if (process_mouse_input(window, uMsg, wParam, lParam))
        // {
        //     return 0;
        // }
        // if (process_keyboard_input(window, uMsg, wParam, lParam))
        // {
        //     return 0;
        // }
        switch (uMsg)
        {
        // case WM_CLOSE:
        //     window->isCloseButtonPressed = true;
        //     return 0;
        case WM_SIZE:
            window->width = (uint32_t)(LOWORD(lParam));
            window->height = (uint32_t)(HIWORD(lParam));
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#else
#   error Unsupported platform
#endif
