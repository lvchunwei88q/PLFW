//========================================================================
// PLFW 3.5 Win32 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2019 Camilla Löwy <elmindreda@glfw.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"

#if defined(_PLFW_WIN32)

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <windowsx.h>
#include <shellapi.h>

// Returns the window style for the specified window
//
static DWORD getWindowStyle(const _PLFWwindow* window)
{
    DWORD style = WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

    if (window->monitor)
        style |= WS_POPUP;
    else
    {
        style |= WS_SYSMENU | WS_MINIMIZEBOX;

        if (window->decorated)
        {
            style |= WS_CAPTION;

            if (window->resizable)
                style |= WS_MAXIMIZEBOX | WS_THICKFRAME;
        }
        else
            style |= WS_POPUP;
    }

    return style;
}

// Returns the extended window style for the specified window
//
static DWORD getWindowExStyle(const _PLFWwindow* window)
{
    DWORD style = WS_EX_APPWINDOW;

    if (window->monitor || window->floating)
        style |= WS_EX_TOPMOST;

    return style;
}

// Returns the image whose area most closely matches the desired one
//
static const PLFWimage* chooseImage(int count, const PLFWimage* images,
                                    int width, int height)
{
    int i, leastDiff = INT_MAX;
    const PLFWimage* closest = NULL;

    for (i = 0;  i < count;  i++)
    {
        const int currDiff = abs(images[i].width * images[i].height -
                                 width * height);
        if (currDiff < leastDiff)
        {
            closest = images + i;
            leastDiff = currDiff;
        }
    }

    return closest;
}

// Creates an RGBA icon or cursor
//
static HICON createIcon(const PLFWimage* image, int xhot, int yhot, PLFWbool icon)
{
    int i;
    HDC dc;
    HICON handle;
    HBITMAP color, mask;
    BITMAPV5HEADER bi;
    ICONINFO ii;
    unsigned char* target = NULL;
    unsigned char* source = image->pixels;

    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size        = sizeof(bi);
    bi.bV5Width       = image->width;
    bi.bV5Height      = -image->height;
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00ff0000;
    bi.bV5GreenMask   = 0x0000ff00;
    bi.bV5BlueMask    = 0x000000ff;
    bi.bV5AlphaMask   = 0xff000000;

    dc = GetDC(NULL);
    color = CreateDIBSection(dc,
                             (BITMAPINFO*) &bi,
                             DIB_RGB_COLORS,
                             (void**) &target,
                             NULL,
                             (DWORD) 0);
    ReleaseDC(NULL, dc);

    if (!color)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to create RGBA bitmap");
        return NULL;
    }

    mask = CreateBitmap(image->width, image->height, 1, 1, NULL);
    if (!mask)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to create mask bitmap");
        DeleteObject(color);
        return NULL;
    }

    for (i = 0;  i < image->width * image->height;  i++)
    {
        target[0] = source[2];
        target[1] = source[1];
        target[2] = source[0];
        target[3] = source[3];
        target += 4;
        source += 4;
    }

    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon    = icon;
    ii.xHotspot = xhot;
    ii.yHotspot = yhot;
    ii.hbmMask  = mask;
    ii.hbmColor = color;

    handle = CreateIconIndirect(&ii);

    DeleteObject(color);
    DeleteObject(mask);

    if (!handle)
    {
        if (icon)
        {
            _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                                 "Win32: Failed to create icon");
        }
        else
        {
            _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                                 "Win32: Failed to create cursor");
        }
    }

    return handle;
}

// Enforce the content area aspect ratio based on which edge is being dragged
//
static void applyAspectRatio(_PLFWwindow* window, int edge, RECT* area)
{
    RECT frame = {0};
    const float ratio = (float) window->numer / (float) window->denom;
    const DWORD style = getWindowStyle(window);
    const DWORD exStyle = getWindowExStyle(window);

    if (_plfwIsWindows10Version1607OrGreaterWin32())
    {
        AdjustWindowRectExForDpi(&frame, style, FALSE, exStyle,
                                 GetDpiForWindow(window->win32.handle));
    }
    else
        AdjustWindowRectEx(&frame, style, FALSE, exStyle);

    if (edge == WMSZ_LEFT  || edge == WMSZ_BOTTOMLEFT ||
        edge == WMSZ_RIGHT || edge == WMSZ_BOTTOMRIGHT)
    {
        area->bottom = area->top + (frame.bottom - frame.top) +
            (int) (((area->right - area->left) - (frame.right - frame.left)) / ratio);
    }
    else if (edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT)
    {
        area->top = area->bottom - (frame.bottom - frame.top) -
            (int) (((area->right - area->left) - (frame.right - frame.left)) / ratio);
    }
    else if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM)
    {
        area->right = area->left + (frame.right - frame.left) +
            (int) (((area->bottom - area->top) - (frame.bottom - frame.top)) * ratio);
    }
}

// Updates the cursor image according to its cursor mode
//
static void updateCursorImage(_PLFWwindow* window)
{
    if (window->cursorMode == PLFW_CURSOR_NORMAL ||
        window->cursorMode == PLFW_CURSOR_CAPTURED)
    {
        if (window->cursor)
            SetCursor(window->cursor->win32.handle);
        else
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
    }
    else
    {
        // NOTE: Via Remote Desktop, setting the cursor to NULL does not hide it.
        // HACK: When running locally, it is set to NULL, but when connected via Remote
        //       Desktop, this is a transparent cursor.
        SetCursor(_plfw.win32.blankCursor);
    }
}

// Sets the cursor clip rect to the window content area
//
static void captureCursor(_PLFWwindow* window)
{
    RECT clipRect;
    GetClientRect(window->win32.handle, &clipRect);
    ClientToScreen(window->win32.handle, (POINT*) &clipRect.left);
    ClientToScreen(window->win32.handle, (POINT*) &clipRect.right);
    ClipCursor(&clipRect);
    _plfw.win32.capturedCursorWindow = window;
}

// Disabled clip cursor
//
static void releaseCursor(void)
{
    ClipCursor(NULL);
    _plfw.win32.capturedCursorWindow = NULL;
}

// Enables WM_INPUT messages for the mouse for the specified window
//
static void enableRawMouseMotion(_PLFWwindow* window)
{
    const RAWINPUTDEVICE rid = { 0x01, 0x02, 0, window->win32.handle };

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to register raw input device");
    }
}

// Disables WM_INPUT messages for the mouse
//
static void disableRawMouseMotion(_PLFWwindow* window)
{
    const RAWINPUTDEVICE rid = { 0x01, 0x02, RIDEV_REMOVE, NULL };

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid)))
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to remove raw input device");
    }
}

// Apply disabled cursor mode to a focused window
//
static void disableCursor(_PLFWwindow* window)
{
    _plfw.win32.disabledCursorWindow = window;
    _plfwGetCursorPosWin32(window,
                           &_plfw.win32.restoreCursorPosX,
                           &_plfw.win32.restoreCursorPosY);
    updateCursorImage(window);
    _plfwCenterCursorInContentArea(window);
    captureCursor(window);

    if (window->rawMouseMotion)
        enableRawMouseMotion(window);
}

// Exit disabled cursor mode for the specified window
//
static void enableCursor(_PLFWwindow* window)
{
    if (window->rawMouseMotion)
        disableRawMouseMotion(window);

    _plfw.win32.disabledCursorWindow = NULL;
    releaseCursor();
    _plfwSetCursorPosWin32(window,
                           _plfw.win32.restoreCursorPosX,
                           _plfw.win32.restoreCursorPosY);
    updateCursorImage(window);
}

// Returns whether the cursor is in the content area of the specified window
//
static PLFWbool cursorInContentArea(_PLFWwindow* window)
{
    RECT area;
    POINT pos;

    if (!GetCursorPos(&pos))
        return PLFW_FALSE;

    if (WindowFromPoint(pos) != window->win32.handle)
        return PLFW_FALSE;

    GetClientRect(window->win32.handle, &area);
    ClientToScreen(window->win32.handle, (POINT*) &area.left);
    ClientToScreen(window->win32.handle, (POINT*) &area.right);

    return PtInRect(&area, pos);
}

// Update native window styles to match attributes
//
static void updateWindowStyles(const _PLFWwindow* window)
{
    RECT rect;
    DWORD style = GetWindowLongW(window->win32.handle, GWL_STYLE);
    style &= ~(WS_OVERLAPPEDWINDOW | WS_POPUP);
    style |= getWindowStyle(window);

    GetClientRect(window->win32.handle, &rect);

    if (_plfwIsWindows10Version1607OrGreaterWin32())
    {
        AdjustWindowRectExForDpi(&rect, style, FALSE,
                                 getWindowExStyle(window),
                                 GetDpiForWindow(window->win32.handle));
    }
    else
        AdjustWindowRectEx(&rect, style, FALSE, getWindowExStyle(window));

    ClientToScreen(window->win32.handle, (POINT*) &rect.left);
    ClientToScreen(window->win32.handle, (POINT*) &rect.right);
    SetWindowLongW(window->win32.handle, GWL_STYLE, style);
    SetWindowPos(window->win32.handle, HWND_TOP,
                 rect.left, rect.top,
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER);
}

// Retrieves and translates modifier keys
//
static int getKeyMods(void)
{
    int mods = 0;

    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= PLFW_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= PLFW_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= PLFW_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        mods |= PLFW_MOD_SUPER;
    if (GetKeyState(VK_CAPITAL) & 1)
        mods |= PLFW_MOD_CAPS_LOCK;
    if (GetKeyState(VK_NUMLOCK) & 1)
        mods |= PLFW_MOD_NUM_LOCK;

    return mods;
}

static void fitToMonitor(_PLFWwindow* window)
{
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(window->monitor->win32.handle, &mi);
    SetWindowPos(window->win32.handle, HWND_TOPMOST,
                 mi.rcMonitor.left,
                 mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS);
}

// Make the specified window and its video mode active on its monitor
//
static void acquireMonitor(_PLFWwindow* window)
{
    if (!_plfw.win32.acquiredMonitorCount)
    {
        SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);

        // HACK: When mouse trails are enabled the cursor becomes invisible when
        //       the OpenGL ICD switches to page flipping
        SystemParametersInfoW(SPI_GETMOUSETRAILS, 0, &_plfw.win32.mouseTrailSize, 0);
        SystemParametersInfoW(SPI_SETMOUSETRAILS, 0, 0, 0);
    }

    if (!window->monitor->window)
        _plfw.win32.acquiredMonitorCount++;

    _plfwSetVideoModeWin32(window->monitor, &window->videoMode);
    _plfwInputMonitorWindow(window->monitor, window);
}

// Remove the window and restore the original video mode
//
static void releaseMonitor(_PLFWwindow* window)
{
    if (window->monitor->window != window)
        return;

    _plfw.win32.acquiredMonitorCount--;
    if (!_plfw.win32.acquiredMonitorCount)
    {
        SetThreadExecutionState(ES_CONTINUOUS);

        // HACK: Restore mouse trail length saved in acquireMonitor
        SystemParametersInfoW(SPI_SETMOUSETRAILS, _plfw.win32.mouseTrailSize, 0, 0);
    }

    _plfwInputMonitorWindow(window->monitor, NULL);
    _plfwRestoreVideoModeWin32(window->monitor);
}

// Manually maximize the window, for when SW_MAXIMIZE cannot be used
//
static void maximizeWindowManually(_PLFWwindow* window)
{
    RECT rect;
    DWORD style;
    MONITORINFO mi = { sizeof(mi) };

    GetMonitorInfoW(MonitorFromWindow(window->win32.handle,
                                      MONITOR_DEFAULTTONEAREST), &mi);

    rect = mi.rcWork;

    if (window->maxwidth != PLFW_DONT_CARE && window->maxheight != PLFW_DONT_CARE)
    {
        rect.right = _plfw_min(rect.right, rect.left + window->maxwidth);
        rect.bottom = _plfw_min(rect.bottom, rect.top + window->maxheight);
    }

    style = GetWindowLongW(window->win32.handle, GWL_STYLE);
    style |= WS_MAXIMIZE;
    SetWindowLongW(window->win32.handle, GWL_STYLE, style);

    if (window->decorated)
    {
        const DWORD exStyle = GetWindowLongW(window->win32.handle, GWL_EXSTYLE);

        if (_plfwIsWindows10Version1607OrGreaterWin32())
        {
            const UINT dpi = GetDpiForWindow(window->win32.handle);
            AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle, dpi);
            OffsetRect(&rect, 0, GetSystemMetricsForDpi(SM_CYCAPTION, dpi));
        }
        else
        {
            AdjustWindowRectEx(&rect, style, FALSE, exStyle);
            OffsetRect(&rect, 0, GetSystemMetrics(SM_CYCAPTION));
        }

        rect.bottom = _plfw_min(rect.bottom, mi.rcWork.bottom);
    }

    SetWindowPos(window->win32.handle, HWND_TOP,
                 rect.left,
                 rect.top,
                 rect.right - rect.left,
                 rect.bottom - rect.top,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

// Window procedure for user-created windows
//
static LRESULT CALLBACK windowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    _PLFWwindow* window = GetPropW(hWnd, L"PLFW");
    if (!window)
    {
        if (uMsg == WM_NCCREATE)
        {
            if (_plfwIsWindows10Version1607OrGreaterWin32())
            {
                const CREATESTRUCTW* cs = (const CREATESTRUCTW*) lParam;
                const _PLFWwndconfig* wndconfig = cs->lpCreateParams;

                // On per-monitor DPI aware V1 systems, only enable
                // non-client scaling for windows that scale the client area
                // We need WM_GETDPISCALEDSIZE from V2 to keep the client
                // area static when the non-client area is scaled
                if (wndconfig && wndconfig->scaleToMonitor)
                    EnableNonClientDpiScaling(hWnd);
            }
        }

        return DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    switch (uMsg)
    {
        case WM_MOUSEACTIVATE:
        {
            // HACK: Postpone cursor disabling when the window was activated by
            //       clicking a caption button
            if (HIWORD(lParam) == WM_LBUTTONDOWN)
            {
                if (LOWORD(lParam) != HTCLIENT)
                    window->win32.frameAction = PLFW_TRUE;
            }

            break;
        }

        case WM_CAPTURECHANGED:
        {
            // HACK: Disable the cursor once the caption button action has been
            //       completed or cancelled
            if (lParam == 0 && window->win32.frameAction)
            {
                if (window->cursorMode == PLFW_CURSOR_DISABLED)
                    disableCursor(window);
                else if (window->cursorMode == PLFW_CURSOR_CAPTURED)
                    captureCursor(window);

                window->win32.frameAction = PLFW_FALSE;
            }

            break;
        }

        case WM_SETFOCUS:
        {
            _plfwInputWindowFocus(window, PLFW_TRUE);

            // HACK: Do not disable cursor while the user is interacting with
            //       a caption button
            if (window->win32.frameAction)
                break;

            if (window->cursorMode == PLFW_CURSOR_DISABLED)
                disableCursor(window);
            else if (window->cursorMode == PLFW_CURSOR_CAPTURED)
                captureCursor(window);

            return 0;
        }

        case WM_KILLFOCUS:
        {
            if (window->cursorMode == PLFW_CURSOR_DISABLED)
                enableCursor(window);
            else if (window->cursorMode == PLFW_CURSOR_CAPTURED)
                releaseCursor();

            if (window->monitor && window->autoIconify)
                _plfwIconifyWindowWin32(window);

            _plfwInputWindowFocus(window, PLFW_FALSE);
            return 0;
        }

        case WM_SYSCOMMAND:
        {
            switch (wParam & 0xfff0)
            {
                case SC_SCREENSAVE:
                case SC_MONITORPOWER:
                {
                    if (window->monitor)
                    {
                        // We are running in full screen mode, so disallow
                        // screen saver and screen blanking
                        return 0;
                    }
                    else
                        break;
                }

                // User trying to access application menu using ALT?
                case SC_KEYMENU:
                {
                    if (!window->win32.keymenu)
                        return 0;

                    break;
                }
            }
            break;
        }

        case WM_CLOSE:
        {
            _plfwInputWindowCloseRequest(window);
            return 0;
        }

        case WM_INPUTLANGCHANGE:
        {
            _plfwUpdateKeyNamesWin32();
            break;
        }

        case WM_CHAR:
        case WM_SYSCHAR:
        {
            if (wParam >= 0xd800 && wParam <= 0xdbff)
                window->win32.highSurrogate = (WCHAR) wParam;
            else
            {
                uint32_t codepoint = 0;

                if (wParam >= 0xdc00 && wParam <= 0xdfff)
                {
                    if (window->win32.highSurrogate)
                    {
                        codepoint += (window->win32.highSurrogate - 0xd800) << 10;
                        codepoint += (WCHAR) wParam - 0xdc00;
                        codepoint += 0x10000;
                    }
                }
                else
                    codepoint = (WCHAR) wParam;

                window->win32.highSurrogate = 0;
                _plfwInputChar(window, codepoint, getKeyMods(), uMsg != WM_SYSCHAR);
            }

            if (uMsg == WM_SYSCHAR && window->win32.keymenu)
                break;

            return 0;
        }

        case WM_UNICHAR:
        {
            if (wParam == UNICODE_NOCHAR)
            {
                // WM_UNICHAR is not sent by Windows, but is sent by some
                // third-party input method engine
                // Returning TRUE here announces support for this message
                return TRUE;
            }

            _plfwInputChar(window, (uint32_t) wParam, getKeyMods(), PLFW_TRUE);
            return 0;
        }

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            int key, scancode;
            const int action = (HIWORD(lParam) & KF_UP) ? PLFW_RELEASE : PLFW_PRESS;
            const int mods = getKeyMods();

            scancode = (HIWORD(lParam) & (KF_EXTENDED | 0xff));
            if (!(scancode & 0xff))
            {
                // NOTE: Both media keys and the synthetic key messages from Windows
                //       Clipboard History are reported with a scancode of zero
                // HACK: Map the virtual key to a scancode but preserve extended bit
                scancode |= MapVirtualKeyW((UINT) wParam, MAPVK_VK_TO_VSC);
            }

            // HACK: Alt+PrtSc has a different scancode than just PrtSc
            if (scancode == 0x54)
                scancode = 0x137;

            // HACK: Ctrl+Pause has a different scancode than just Pause
            if (scancode == 0x146)
                scancode = 0x45;

            // HACK: CJK IME sets the extended bit for right Shift
            if (scancode == 0x136)
                scancode = 0x36;

            key = _plfw.win32.keycodes[scancode];

            // The Ctrl keys require special handling
            if (wParam == VK_CONTROL)
            {
                if (HIWORD(lParam) & KF_EXTENDED)
                {
                    // Right side keys have the extended key bit set
                    key = PLFW_KEY_RIGHT_CONTROL;
                }
                else
                {
                    // NOTE: Alt Gr sends Left Ctrl followed by Right Alt
                    // HACK: We only want one event for Alt Gr, so if we detect
                    //       this sequence we discard this Left Ctrl message now
                    //       and later report Right Alt normally
                    MSG next;
                    const DWORD time = GetMessageTime();

                    if (PeekMessageW(&next, NULL, 0, 0, PM_NOREMOVE))
                    {
                        if (next.message == WM_KEYDOWN ||
                            next.message == WM_SYSKEYDOWN ||
                            next.message == WM_KEYUP ||
                            next.message == WM_SYSKEYUP)
                        {
                            if (next.wParam == VK_MENU &&
                                (HIWORD(next.lParam) & KF_EXTENDED) &&
                                next.time == time)
                            {
                                // Next message is Right Alt down so discard this
                                break;
                            }
                        }
                    }

                    // This is a regular Left Ctrl message
                    key = PLFW_KEY_LEFT_CONTROL;
                }
            }
            else if (wParam == VK_PROCESSKEY)
            {
                // IME notifies that keys have been filtered by setting the
                // virtual key-code to VK_PROCESSKEY
                break;
            }

            if (action == PLFW_RELEASE && wParam == VK_SHIFT)
            {
                // HACK: Release both Shift keys on Shift up event, as when both
                //       are pressed the first release does not emit any event
                // NOTE: The other half of this is in _plfwPollEventsWin32
                _plfwInputKey(window, PLFW_KEY_LEFT_SHIFT, scancode, action, mods);
                _plfwInputKey(window, PLFW_KEY_RIGHT_SHIFT, scancode, action, mods);
            }
            else if (wParam == VK_SNAPSHOT)
            {
                // HACK: Key down is not reported for the Print Screen key
                _plfwInputKey(window, key, scancode, PLFW_PRESS, mods);
                _plfwInputKey(window, key, scancode, PLFW_RELEASE, mods);
            }
            else
                _plfwInputKey(window, key, scancode, action, mods);

            break;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            int i, button, action;

            if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP)
                button = PLFW_MOUSE_BUTTON_LEFT;
            else if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP)
                button = PLFW_MOUSE_BUTTON_RIGHT;
            else if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP)
                button = PLFW_MOUSE_BUTTON_MIDDLE;
            else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
                button = PLFW_MOUSE_BUTTON_4;
            else
                button = PLFW_MOUSE_BUTTON_5;

            if (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN ||
                uMsg == WM_MBUTTONDOWN || uMsg == WM_XBUTTONDOWN)
            {
                action = PLFW_PRESS;
            }
            else
                action = PLFW_RELEASE;

            for (i = 0;  i <= PLFW_MOUSE_BUTTON_LAST;  i++)
            {
                if (window->mouseButtons[i] == PLFW_PRESS)
                    break;
            }

            if (i > PLFW_MOUSE_BUTTON_LAST)
                SetCapture(hWnd);

            _plfwInputMouseClick(window, button, action, getKeyMods());

            for (i = 0;  i <= PLFW_MOUSE_BUTTON_LAST;  i++)
            {
                if (window->mouseButtons[i] == PLFW_PRESS)
                    break;
            }

            if (i > PLFW_MOUSE_BUTTON_LAST)
                ReleaseCapture();

            if (uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONUP)
                return TRUE;

            return 0;
        }

        case WM_MOUSEMOVE:
        {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);

            if (!window->win32.cursorTracked)
            {
                TRACKMOUSEEVENT tme;
                ZeroMemory(&tme, sizeof(tme));
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = window->win32.handle;
                TrackMouseEvent(&tme);

                window->win32.cursorTracked = PLFW_TRUE;
                _plfwInputCursorEnter(window, PLFW_TRUE);
            }

            if (window->cursorMode == PLFW_CURSOR_DISABLED)
            {
                const int dx = x - window->win32.lastCursorPosX;
                const int dy = y - window->win32.lastCursorPosY;

                if (_plfw.win32.disabledCursorWindow != window)
                    break;
                if (window->rawMouseMotion)
                    break;

                _plfwInputCursorPos(window,
                                    window->virtualCursorPosX + dx,
                                    window->virtualCursorPosY + dy);
            }
            else
                _plfwInputCursorPos(window, x, y);

            window->win32.lastCursorPosX = x;
            window->win32.lastCursorPosY = y;

            return 0;
        }

        case WM_INPUT:
        {
            UINT size = 0;
            HRAWINPUT ri = (HRAWINPUT) lParam;
            RAWINPUT* data = NULL;
            int dx, dy;

            if (_plfw.win32.disabledCursorWindow != window)
                break;
            if (!window->rawMouseMotion)
                break;

            GetRawInputData(ri, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
            if (size > (UINT) _plfw.win32.rawInputSize)
            {
                _plfw_free(_plfw.win32.rawInput);
                _plfw.win32.rawInput = _plfw_calloc(size, 1);
                _plfw.win32.rawInputSize = size;
            }

            size = _plfw.win32.rawInputSize;
            if (GetRawInputData(ri, RID_INPUT,
                                _plfw.win32.rawInput, &size,
                                sizeof(RAWINPUTHEADER)) == (UINT) -1)
            {
                _plfwInputError(PLFW_PLATFORM_ERROR,
                                "Win32: Failed to retrieve raw input data");
                break;
            }

            data = _plfw.win32.rawInput;
            if (data->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
            {
                POINT pos = {0};
                int width, height;

                if (data->data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP)
                {
                    pos.x += GetSystemMetrics(SM_XVIRTUALSCREEN);
                    pos.y += GetSystemMetrics(SM_YVIRTUALSCREEN);
                    width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                    height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
                }
                else
                {
                    width = GetSystemMetrics(SM_CXSCREEN);
                    height = GetSystemMetrics(SM_CYSCREEN);
                }

                pos.x += (int) ((data->data.mouse.lLastX / 65535.f) * width);
                pos.y += (int) ((data->data.mouse.lLastY / 65535.f) * height);
                ScreenToClient(window->win32.handle, &pos);

                dx = pos.x - window->win32.lastCursorPosX;
                dy = pos.y - window->win32.lastCursorPosY;
            }
            else
            {
                dx = data->data.mouse.lLastX;
                dy = data->data.mouse.lLastY;
            }

            _plfwInputCursorPos(window,
                                window->virtualCursorPosX + dx,
                                window->virtualCursorPosY + dy);

            window->win32.lastCursorPosX += dx;
            window->win32.lastCursorPosY += dy;
            break;
        }

        case WM_MOUSELEAVE:
        {
            window->win32.cursorTracked = PLFW_FALSE;
            _plfwInputCursorEnter(window, PLFW_FALSE);
            return 0;
        }

        case WM_MOUSEWHEEL:
        {
            _plfwInputScroll(window, 0.0, (SHORT) HIWORD(wParam) / (double) WHEEL_DELTA);
            return 0;
        }

        case WM_MOUSEHWHEEL:
        {
            // NOTE: The X-axis is inverted for consistency with macOS and X11
            _plfwInputScroll(window, -((SHORT) HIWORD(wParam) / (double) WHEEL_DELTA), 0.0);
            return 0;
        }

        case WM_ENTERSIZEMOVE:
        case WM_ENTERMENULOOP:
        {
            if (window->win32.frameAction)
                break;

            // HACK: Enable the cursor while the user is moving or
            //       resizing the window or using the window menu
            if (window->cursorMode == PLFW_CURSOR_DISABLED)
                enableCursor(window);
            else if (window->cursorMode == PLFW_CURSOR_CAPTURED)
                releaseCursor();

            break;
        }

        case WM_EXITSIZEMOVE:
        case WM_EXITMENULOOP:
        {
            if (window->win32.frameAction)
                break;

            // HACK: Disable the cursor once the user is done moving or
            //       resizing the window or using the menu
            if (window->cursorMode == PLFW_CURSOR_DISABLED)
                disableCursor(window);
            else if (window->cursorMode == PLFW_CURSOR_CAPTURED)
                captureCursor(window);

            break;
        }

        case WM_SIZE:
        {
            const int width = LOWORD(lParam);
            const int height = HIWORD(lParam);
            const PLFWbool iconified = wParam == SIZE_MINIMIZED;
            const PLFWbool maximized = wParam == SIZE_MAXIMIZED ||
                                       (window->win32.maximized &&
                                        wParam != SIZE_RESTORED);

            if (_plfw.win32.capturedCursorWindow == window)
                captureCursor(window);

            if (window->win32.iconified != iconified)
                _plfwInputWindowIconify(window, iconified);

            if (window->win32.maximized != maximized)
                _plfwInputWindowMaximize(window, maximized);

            if (width != window->win32.width || height != window->win32.height)
            {
                window->win32.width = width;
                window->win32.height = height;

                _plfwInputFramebufferSize(window, width, height);
                _plfwInputWindowSize(window, width, height);
            }

            if (window->monitor && window->win32.iconified != iconified)
            {
                if (iconified)
                    releaseMonitor(window);
                else
                {
                    acquireMonitor(window);
                    fitToMonitor(window);
                }
            }

            window->win32.iconified = iconified;
            window->win32.maximized = maximized;
            return 0;
        }

        case WM_MOVE:
        {
            if (_plfw.win32.capturedCursorWindow == window)
                captureCursor(window);

            // NOTE: This cannot use LOWORD/HIWORD recommended by MSDN, as
            // those macros do not handle negative window positions correctly
            _plfwInputWindowPos(window,
                                GET_X_LPARAM(lParam),
                                GET_Y_LPARAM(lParam));
            return 0;
        }

        case WM_SIZING:
        {
            if (window->numer == PLFW_DONT_CARE ||
                window->denom == PLFW_DONT_CARE)
            {
                break;
            }

            applyAspectRatio(window, (int) wParam, (RECT*) lParam);
            return TRUE;
        }

        case WM_GETMINMAXINFO:
        {
            RECT frame = {0};
            MINMAXINFO* mmi = (MINMAXINFO*) lParam;
            const DWORD style = getWindowStyle(window);
            const DWORD exStyle = getWindowExStyle(window);

            if (window->monitor)
                break;

            if (_plfwIsWindows10Version1607OrGreaterWin32())
            {
                AdjustWindowRectExForDpi(&frame, style, FALSE, exStyle,
                                         GetDpiForWindow(window->win32.handle));
            }
            else
                AdjustWindowRectEx(&frame, style, FALSE, exStyle);

            if (window->minwidth != PLFW_DONT_CARE &&
                window->minheight != PLFW_DONT_CARE)
            {
                mmi->ptMinTrackSize.x = window->minwidth + frame.right - frame.left;
                mmi->ptMinTrackSize.y = window->minheight + frame.bottom - frame.top;
            }

            if (window->maxwidth != PLFW_DONT_CARE &&
                window->maxheight != PLFW_DONT_CARE)
            {
                mmi->ptMaxTrackSize.x = window->maxwidth + frame.right - frame.left;
                mmi->ptMaxTrackSize.y = window->maxheight + frame.bottom - frame.top;
            }

            if (!window->decorated)
            {
                MONITORINFO mi;
                const HMONITOR mh = MonitorFromWindow(window->win32.handle,
                                                      MONITOR_DEFAULTTONEAREST);

                ZeroMemory(&mi, sizeof(mi));
                mi.cbSize = sizeof(mi);
                GetMonitorInfoW(mh, &mi);

                mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
                mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
                mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
                mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
            }

            return 0;
        }

        case WM_PAINT:
        {
            _plfwInputWindowDamage(window);
            break;
        }

        case WM_ERASEBKGND:
        {
            return TRUE;
        }

        case WM_NCACTIVATE:
        case WM_NCPAINT:
        {
            // Prevent title bar from being drawn after restoring a minimized
            // undecorated window
            if (!window->decorated)
                return TRUE;

            break;
        }

        case WM_GETDPISCALEDSIZE:
        {
            if (window->win32.scaleToMonitor)
                break;

            // Adjust the window size to keep the content area size constant
            if (_plfwIsWindows10Version1703OrGreaterWin32())
            {
                RECT source = {0}, target = {0};
                SIZE* size = (SIZE*) lParam;

                AdjustWindowRectExForDpi(&source, getWindowStyle(window),
                                         FALSE, getWindowExStyle(window),
                                         GetDpiForWindow(window->win32.handle));
                AdjustWindowRectExForDpi(&target, getWindowStyle(window),
                                         FALSE, getWindowExStyle(window),
                                         LOWORD(wParam));

                size->cx += (target.right - target.left) -
                            (source.right - source.left);
                size->cy += (target.bottom - target.top) -
                            (source.bottom - source.top);
                return TRUE;
            }

            break;
        }

        case WM_DPICHANGED:
        {
            const float xscale = HIWORD(wParam) / (float) USER_DEFAULT_SCREEN_DPI;
            const float yscale = LOWORD(wParam) / (float) USER_DEFAULT_SCREEN_DPI;

            // Resize windowed mode windows that either permit rescaling or that
            // need it to compensate for non-client area scaling
            if (!window->monitor &&
                (window->win32.scaleToMonitor ||
                 _plfwIsWindows10Version1703OrGreaterWin32()))
            {
                RECT* suggested = (RECT*) lParam;
                SetWindowPos(window->win32.handle, HWND_TOP,
                             suggested->left,
                             suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
            }

            _plfwInputWindowContentScale(window, xscale, yscale);
            break;
        }

        case WM_SETCURSOR:
        {
            if (LOWORD(lParam) == HTCLIENT)
            {
                updateCursorImage(window);
                return TRUE;
            }

            break;
        }

        case WM_DROPFILES:
        {
            HDROP drop = (HDROP) wParam;
            POINT pt;
            int i;

            const int count = DragQueryFileW(drop, 0xffffffff, NULL, 0);
            char** paths = _plfw_calloc(count, sizeof(char*));

            // Move the mouse to the position of the drop
            DragQueryPoint(drop, &pt);
            _plfwInputCursorPos(window, pt.x, pt.y);

            for (i = 0;  i < count;  i++)
            {
                const UINT length = DragQueryFileW(drop, i, NULL, 0);
                WCHAR* buffer = _plfw_calloc((size_t) length + 1, sizeof(WCHAR));

                DragQueryFileW(drop, i, buffer, length + 1);
                paths[i] = _plfwCreateUTF8FromWideStringWin32(buffer);

                _plfw_free(buffer);
            }

            _plfwInputDrop(window, count, (const char**) paths);

            for (i = 0;  i < count;  i++)
                _plfw_free(paths[i]);
            _plfw_free(paths);

            DragFinish(drop);
            return 0;
        }
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// Creates the PLFW window
//
static int createNativeWindow(_PLFWwindow* window,
                              const _PLFWwndconfig* wndconfig,
                              const _PLFWfbconfig* fbconfig)
{
    int frameX, frameY, frameWidth, frameHeight;
    WCHAR* wideTitle;
    DWORD style = getWindowStyle(window);
    DWORD exStyle = getWindowExStyle(window);

    if (!_plfw.win32.mainWindowClass)
    {
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc   = windowProc;
        wc.hInstance     = _plfw.win32.instance;
        wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
#if defined(_PLFW_WNDCLASSNAME)
        wc.lpszClassName = _PLFW_WNDCLASSNAME;
#else
        wc.lpszClassName = L"PLFW30";
#endif
        // Load user-provided icon if available
        wc.hIcon = LoadImageW(GetModuleHandleW(NULL),
                              L"PLFW_ICON", IMAGE_ICON,
                              0, 0, LR_DEFAULTSIZE | LR_SHARED);
        if (!wc.hIcon)
        {
            // No user-provided icon found, load default icon
            wc.hIcon = LoadImageW(NULL,
                                  IDI_APPLICATION, IMAGE_ICON,
                                  0, 0, LR_DEFAULTSIZE | LR_SHARED);
        }

        _plfw.win32.mainWindowClass = RegisterClassExW(&wc);
        if (!_plfw.win32.mainWindowClass)
        {
            _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                                 "Win32: Failed to register window class");
            return PLFW_FALSE;
        }
    }

    if (GetSystemMetrics(SM_REMOTESESSION))
    {
        // NOTE: On Remote Desktop, setting the cursor to NULL does not hide it
        // HACK: Create a transparent cursor and always set that instead of NULL
        //       When not on Remote Desktop, this handle is NULL and normal hiding is used
        if (!_plfw.win32.blankCursor)
        {
            const int cursorWidth = GetSystemMetrics(SM_CXCURSOR);
            const int cursorHeight = GetSystemMetrics(SM_CYCURSOR);

            unsigned char* cursorPixels = _plfw_calloc(cursorWidth * cursorHeight, 4);
            if (!cursorPixels)
                return PLFW_FALSE;

            // NOTE: Windows checks whether the image is fully transparent and if so
            //       just ignores the alpha channel and makes the whole cursor opaque
            // HACK: Make one pixel slightly less transparent
            cursorPixels[3] = 1;

            const PLFWimage cursorImage = { cursorWidth, cursorHeight, cursorPixels };
            _plfw.win32.blankCursor = createIcon(&cursorImage, 0, 0, FALSE);
            _plfw_free(cursorPixels);

            if (!_plfw.win32.blankCursor)
                return PLFW_FALSE;
        }
    }

    if (window->monitor)
    {
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfoW(window->monitor->win32.handle, &mi);

        // NOTE: This window placement is temporary and approximate, as the
        //       correct position and size cannot be known until the monitor
        //       video mode has been picked in _plfwSetVideoModeWin32
        frameX = mi.rcMonitor.left;
        frameY = mi.rcMonitor.top;
        frameWidth  = mi.rcMonitor.right - mi.rcMonitor.left;
        frameHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
    }
    else
    {
        RECT rect = { 0, 0, wndconfig->width, wndconfig->height };

        window->win32.maximized = wndconfig->maximized;
        if (wndconfig->maximized)
            style |= WS_MAXIMIZE;

        AdjustWindowRectEx(&rect, style, FALSE, exStyle);

        if (wndconfig->xpos == PLFW_ANY_POSITION && wndconfig->ypos == PLFW_ANY_POSITION)
        {
            frameX = CW_USEDEFAULT;
            frameY = CW_USEDEFAULT;
        }
        else
        {
            frameX = wndconfig->xpos + rect.left;
            frameY = wndconfig->ypos + rect.top;
        }

        frameWidth  = rect.right - rect.left;
        frameHeight = rect.bottom - rect.top;
    }

    wideTitle = _plfwCreateWideStringFromUTF8Win32(window->title);
    if (!wideTitle)
        return PLFW_FALSE;

    window->win32.handle = CreateWindowExW(exStyle,
                                           MAKEINTATOM(_plfw.win32.mainWindowClass),
                                           wideTitle,
                                           style,
                                           frameX, frameY,
                                           frameWidth, frameHeight,
                                           NULL, // No parent window
                                           NULL, // No window menu
                                           _plfw.win32.instance,
                                           (LPVOID) wndconfig);

    _plfw_free(wideTitle);

    if (!window->win32.handle)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to create window");
        return PLFW_FALSE;
    }

    SetPropW(window->win32.handle, L"PLFW", window);

    ChangeWindowMessageFilterEx(window->win32.handle, WM_DROPFILES, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(window->win32.handle, WM_COPYDATA, MSGFLT_ALLOW, NULL);
    ChangeWindowMessageFilterEx(window->win32.handle, WM_COPYGLOBALDATA, MSGFLT_ALLOW, NULL);

    window->win32.scaleToMonitor = wndconfig->scaleToMonitor;
    window->win32.keymenu = wndconfig->win32.keymenu;
    window->win32.showDefault = wndconfig->win32.showDefault;

    if (!window->monitor)
    {
        RECT rect = { 0, 0, wndconfig->width, wndconfig->height };
        WINDOWPLACEMENT wp = { sizeof(wp) };
        const HMONITOR mh = MonitorFromWindow(window->win32.handle,
                                              MONITOR_DEFAULTTONEAREST);

        // Adjust window rect to account for DPI scaling of the window frame and
        // (if enabled) DPI scaling of the content area
        // This cannot be done until we know what monitor the window was placed on
        // Only update the restored window rect as the window may be maximized

        if (wndconfig->scaleToMonitor)
        {
            float xscale, yscale;
            _plfwGetHMONITORContentScaleWin32(mh, &xscale, &yscale);

            if (xscale > 0.f && yscale > 0.f)
            {
                rect.right = (int) (rect.right * xscale);
                rect.bottom = (int) (rect.bottom * yscale);
            }
        }

        if (_plfwIsWindows10Version1607OrGreaterWin32())
        {
            AdjustWindowRectExForDpi(&rect, style, FALSE, exStyle,
                                     GetDpiForWindow(window->win32.handle));
        }
        else
            AdjustWindowRectEx(&rect, style, FALSE, exStyle);

        GetWindowPlacement(window->win32.handle, &wp);
        OffsetRect(&rect,
                   wp.rcNormalPosition.left - rect.left,
                   wp.rcNormalPosition.top - rect.top);

        wp.rcNormalPosition = rect;
        wp.showCmd = SW_HIDE;
        SetWindowPlacement(window->win32.handle, &wp);

        // Adjust rect of maximized undecorated window, because by default Windows will
        // make such a window cover the whole monitor instead of its workarea

        if (wndconfig->maximized && !wndconfig->decorated)
        {
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfoW(mh, &mi);

            SetWindowPos(window->win32.handle, HWND_TOP,
                         mi.rcWork.left,
                         mi.rcWork.top,
                         mi.rcWork.right - mi.rcWork.left,
                         mi.rcWork.bottom - mi.rcWork.top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    DragAcceptFiles(window->win32.handle, TRUE);

    _plfwGetWindowSizeWin32(window, &window->win32.width, &window->win32.height);

    return PLFW_TRUE;
}

PLFWbool _plfwCreateWindowWin32(_PLFWwindow* window,
                                const _PLFWwndconfig* wndconfig,
                                const _PLFWfbconfig* fbconfig)
{
    if (!createNativeWindow(window, wndconfig, fbconfig))
        return PLFW_FALSE;

    // No context is created: this library owns the window and its input state
    // only, and leaves all rendering to the application.

    if (wndconfig->mousePassthrough)
        _plfwSetWindowMousePassthroughWin32(window, PLFW_TRUE);

    if (window->monitor)
    {
        _plfwShowWindowWin32(window);
        _plfwFocusWindowWin32(window);
        acquireMonitor(window);
        fitToMonitor(window);

        if (wndconfig->centerCursor)
            _plfwCenterCursorInContentArea(window);
    }
    else
    {
        if (wndconfig->visible)
        {
            _plfwShowWindowWin32(window);
            if (wndconfig->focused)
                _plfwFocusWindowWin32(window);
        }
    }

    return PLFW_TRUE;
}

void _plfwDestroyWindowWin32(_PLFWwindow* window)
{
    if (window->monitor)
        releaseMonitor(window);

    if (_plfw.win32.disabledCursorWindow == window)
        enableCursor(window);

    if (_plfw.win32.capturedCursorWindow == window)
        releaseCursor();

    if (window->win32.handle)
    {
        RemovePropW(window->win32.handle, L"PLFW");
        DestroyWindow(window->win32.handle);
        window->win32.handle = NULL;
    }

    if (window->win32.bigIcon)
        DestroyIcon(window->win32.bigIcon);

    if (window->win32.smallIcon)
        DestroyIcon(window->win32.smallIcon);
}

void _plfwSetWindowTitleWin32(_PLFWwindow* window, const char* title)
{
    WCHAR* wideTitle = _plfwCreateWideStringFromUTF8Win32(title);
    if (!wideTitle)
        return;

    SetWindowTextW(window->win32.handle, wideTitle);
    _plfw_free(wideTitle);
}

void _plfwSetWindowIconWin32(_PLFWwindow* window, int count, const PLFWimage* images)
{
    HICON bigIcon = NULL, smallIcon = NULL;

    if (count)
    {
        const PLFWimage* bigImage = chooseImage(count, images,
                                                GetSystemMetrics(SM_CXICON),
                                                GetSystemMetrics(SM_CYICON));
        const PLFWimage* smallImage = chooseImage(count, images,
                                                  GetSystemMetrics(SM_CXSMICON),
                                                  GetSystemMetrics(SM_CYSMICON));

        bigIcon = createIcon(bigImage, 0, 0, PLFW_TRUE);
        smallIcon = createIcon(smallImage, 0, 0, PLFW_TRUE);
    }
    else
    {
        bigIcon = (HICON) GetClassLongPtrW(window->win32.handle, GCLP_HICON);
        smallIcon = (HICON) GetClassLongPtrW(window->win32.handle, GCLP_HICONSM);
    }

    SendMessageW(window->win32.handle, WM_SETICON, ICON_BIG, (LPARAM) bigIcon);
    SendMessageW(window->win32.handle, WM_SETICON, ICON_SMALL, (LPARAM) smallIcon);

    if (window->win32.bigIcon)
        DestroyIcon(window->win32.bigIcon);

    if (window->win32.smallIcon)
        DestroyIcon(window->win32.smallIcon);

    if (count)
    {
        window->win32.bigIcon = bigIcon;
        window->win32.smallIcon = smallIcon;
    }
}

void _plfwGetWindowPosWin32(_PLFWwindow* window, int* xpos, int* ypos)
{
    POINT pos = { 0, 0 };
    ClientToScreen(window->win32.handle, &pos);

    if (xpos)
        *xpos = pos.x;
    if (ypos)
        *ypos = pos.y;
}

void _plfwSetWindowPosWin32(_PLFWwindow* window, int xpos, int ypos)
{
    RECT rect = { xpos, ypos, xpos, ypos };

    if (_plfwIsWindows10Version1607OrGreaterWin32())
    {
        AdjustWindowRectExForDpi(&rect, getWindowStyle(window),
                                 FALSE, getWindowExStyle(window),
                                 GetDpiForWindow(window->win32.handle));
    }
    else
    {
        AdjustWindowRectEx(&rect, getWindowStyle(window),
                           FALSE, getWindowExStyle(window));
    }

    SetWindowPos(window->win32.handle, NULL, rect.left, rect.top, 0, 0,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOSIZE);
}

void _plfwGetWindowSizeWin32(_PLFWwindow* window, int* width, int* height)
{
    RECT area;
    GetClientRect(window->win32.handle, &area);

    if (width)
        *width = area.right;
    if (height)
        *height = area.bottom;
}

void _plfwSetWindowSizeWin32(_PLFWwindow* window, int width, int height)
{
    if (window->monitor)
    {
        if (window->monitor->window == window)
        {
            acquireMonitor(window);
            fitToMonitor(window);
        }
    }
    else
    {
        RECT rect = { 0, 0, width, height };

        if (_plfwIsWindows10Version1607OrGreaterWin32())
        {
            AdjustWindowRectExForDpi(&rect, getWindowStyle(window),
                                     FALSE, getWindowExStyle(window),
                                     GetDpiForWindow(window->win32.handle));
        }
        else
        {
            AdjustWindowRectEx(&rect, getWindowStyle(window),
                               FALSE, getWindowExStyle(window));
        }

        SetWindowPos(window->win32.handle, HWND_TOP,
                     0, 0, rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOZORDER);
    }
}

void _plfwSetWindowSizeLimitsWin32(_PLFWwindow* window,
                                   int minwidth, int minheight,
                                   int maxwidth, int maxheight)
{
    RECT area;

    if ((minwidth == PLFW_DONT_CARE || minheight == PLFW_DONT_CARE) &&
        (maxwidth == PLFW_DONT_CARE || maxheight == PLFW_DONT_CARE))
    {
        return;
    }

    GetWindowRect(window->win32.handle, &area);
    MoveWindow(window->win32.handle,
               area.left, area.top,
               area.right - area.left,
               area.bottom - area.top, TRUE);
}

void _plfwSetWindowAspectRatioWin32(_PLFWwindow* window, int numer, int denom)
{
    RECT area;

    if (numer == PLFW_DONT_CARE || denom == PLFW_DONT_CARE)
        return;

    GetWindowRect(window->win32.handle, &area);
    applyAspectRatio(window, WMSZ_BOTTOMRIGHT, &area);
    MoveWindow(window->win32.handle,
               area.left, area.top,
               area.right - area.left,
               area.bottom - area.top, TRUE);
}

void _plfwGetFramebufferSizeWin32(_PLFWwindow* window, int* width, int* height)
{
    _plfwGetWindowSizeWin32(window, width, height);
}

void _plfwGetWindowFrameSizeWin32(_PLFWwindow* window,
                                  int* left, int* top,
                                  int* right, int* bottom)
{
    RECT rect;
    int width, height;

    _plfwGetWindowSizeWin32(window, &width, &height);
    SetRect(&rect, 0, 0, width, height);

    if (_plfwIsWindows10Version1607OrGreaterWin32())
    {
        AdjustWindowRectExForDpi(&rect, getWindowStyle(window),
                                 FALSE, getWindowExStyle(window),
                                 GetDpiForWindow(window->win32.handle));
    }
    else
    {
        AdjustWindowRectEx(&rect, getWindowStyle(window),
                           FALSE, getWindowExStyle(window));
    }

    if (left)
        *left = -rect.left;
    if (top)
        *top = -rect.top;
    if (right)
        *right = rect.right - width;
    if (bottom)
        *bottom = rect.bottom - height;
}

void _plfwGetWindowContentScaleWin32(_PLFWwindow* window, float* xscale, float* yscale)
{
    const HANDLE handle = MonitorFromWindow(window->win32.handle,
                                            MONITOR_DEFAULTTONEAREST);
    _plfwGetHMONITORContentScaleWin32(handle, xscale, yscale);
}

void _plfwIconifyWindowWin32(_PLFWwindow* window)
{
    ShowWindow(window->win32.handle, SW_MINIMIZE);
}

void _plfwRestoreWindowWin32(_PLFWwindow* window)
{
    ShowWindow(window->win32.handle, SW_RESTORE);
}

void _plfwMaximizeWindowWin32(_PLFWwindow* window)
{
    if (IsWindowVisible(window->win32.handle))
        ShowWindow(window->win32.handle, SW_MAXIMIZE);
    else
        maximizeWindowManually(window);
}

void _plfwShowWindowWin32(_PLFWwindow* window)
{
    int showCommand = SW_SHOWNA;

    if (window->win32.showDefault)
    {
        // NOTE: PLFW windows currently do not seem to match the Windows 10 definition of
        //       a main window, so even SW_SHOWDEFAULT does nothing
        //       This definition is undocumented and can change (source: Raymond Chen)
        // HACK: Apply the STARTUPINFO show command manually if available
        STARTUPINFOW si = { sizeof(si) };
        GetStartupInfoW(&si);
        if (si.dwFlags & STARTF_USESHOWWINDOW)
            showCommand = si.wShowWindow;

        window->win32.showDefault = PLFW_FALSE;
    }

    ShowWindow(window->win32.handle, showCommand);
}

void _plfwHideWindowWin32(_PLFWwindow* window)
{
    ShowWindow(window->win32.handle, SW_HIDE);
}

void _plfwRequestWindowAttentionWin32(_PLFWwindow* window)
{
    FlashWindow(window->win32.handle, TRUE);
}

void _plfwFocusWindowWin32(_PLFWwindow* window)
{
    BringWindowToTop(window->win32.handle);
    SetForegroundWindow(window->win32.handle);
    SetFocus(window->win32.handle);
}

void _plfwSetWindowMonitorWin32(_PLFWwindow* window,
                                _PLFWmonitor* monitor,
                                int xpos, int ypos,
                                int width, int height,
                                int refreshRate)
{
    if (window->monitor == monitor)
    {
        if (monitor)
        {
            if (monitor->window == window)
            {
                acquireMonitor(window);
                fitToMonitor(window);
            }
        }
        else
        {
            RECT rect = { xpos, ypos, xpos + width, ypos + height };

            if (_plfwIsWindows10Version1607OrGreaterWin32())
            {
                AdjustWindowRectExForDpi(&rect, getWindowStyle(window),
                                         FALSE, getWindowExStyle(window),
                                         GetDpiForWindow(window->win32.handle));
            }
            else
            {
                AdjustWindowRectEx(&rect, getWindowStyle(window),
                                   FALSE, getWindowExStyle(window));
            }

            SetWindowPos(window->win32.handle, HWND_TOP,
                         rect.left, rect.top,
                         rect.right - rect.left, rect.bottom - rect.top,
                         SWP_NOCOPYBITS | SWP_NOACTIVATE | SWP_NOZORDER);
        }

        return;
    }

    if (window->monitor)
        releaseMonitor(window);

    _plfwInputWindowMonitor(window, monitor);

    if (window->monitor)
    {
        MONITORINFO mi = { sizeof(mi) };
        UINT flags = SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOCOPYBITS;

        if (window->decorated)
        {
            DWORD style = GetWindowLongW(window->win32.handle, GWL_STYLE);
            style &= ~WS_OVERLAPPEDWINDOW;
            style |= getWindowStyle(window);
            SetWindowLongW(window->win32.handle, GWL_STYLE, style);
            flags |= SWP_FRAMECHANGED;
        }

        acquireMonitor(window);

        GetMonitorInfoW(window->monitor->win32.handle, &mi);
        SetWindowPos(window->win32.handle, HWND_TOPMOST,
                     mi.rcMonitor.left,
                     mi.rcMonitor.top,
                     mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top,
                     flags);
    }
    else
    {
        HWND after;
        RECT rect = { xpos, ypos, xpos + width, ypos + height };
        DWORD style = GetWindowLongW(window->win32.handle, GWL_STYLE);
        UINT flags = SWP_NOACTIVATE | SWP_NOCOPYBITS;

        if (window->decorated)
        {
            style &= ~WS_POPUP;
            style |= getWindowStyle(window);
            SetWindowLongW(window->win32.handle, GWL_STYLE, style);

            flags |= SWP_FRAMECHANGED;
        }

        if (window->floating)
            after = HWND_TOPMOST;
        else
            after = HWND_NOTOPMOST;

        if (_plfwIsWindows10Version1607OrGreaterWin32())
        {
            AdjustWindowRectExForDpi(&rect, getWindowStyle(window),
                                     FALSE, getWindowExStyle(window),
                                     GetDpiForWindow(window->win32.handle));
        }
        else
        {
            AdjustWindowRectEx(&rect, getWindowStyle(window),
                               FALSE, getWindowExStyle(window));
        }

        SetWindowPos(window->win32.handle, after,
                     rect.left, rect.top,
                     rect.right - rect.left, rect.bottom - rect.top,
                     flags);
    }
}

PLFWbool _plfwWindowFocusedWin32(_PLFWwindow* window)
{
    return window->win32.handle == GetActiveWindow();
}

PLFWbool _plfwWindowIconifiedWin32(_PLFWwindow* window)
{
    return IsIconic(window->win32.handle);
}

PLFWbool _plfwWindowVisibleWin32(_PLFWwindow* window)
{
    return IsWindowVisible(window->win32.handle);
}

PLFWbool _plfwWindowMaximizedWin32(_PLFWwindow* window)
{
    return IsZoomed(window->win32.handle);
}

PLFWbool _plfwWindowHoveredWin32(_PLFWwindow* window)
{
    return cursorInContentArea(window);
}

void _plfwSetWindowResizableWin32(_PLFWwindow* window, PLFWbool enabled)
{
    updateWindowStyles(window);
}

void _plfwSetWindowDecoratedWin32(_PLFWwindow* window, PLFWbool enabled)
{
    updateWindowStyles(window);
}

void _plfwSetWindowFloatingWin32(_PLFWwindow* window, PLFWbool enabled)
{
    const HWND after = enabled ? HWND_TOPMOST : HWND_NOTOPMOST;
    SetWindowPos(window->win32.handle, after, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
}

void _plfwSetWindowMousePassthroughWin32(_PLFWwindow* window, PLFWbool enabled)
{
    COLORREF key = 0;
    BYTE alpha = 0;
    DWORD flags = 0;
    DWORD exStyle = GetWindowLongW(window->win32.handle, GWL_EXSTYLE);

    if (exStyle & WS_EX_LAYERED)
        GetLayeredWindowAttributes(window->win32.handle, &key, &alpha, &flags);

    if (enabled)
        exStyle |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
    else
    {
        exStyle &= ~WS_EX_TRANSPARENT;
        // NOTE: Window opacity also needs the layered window style so do not
        //       remove it if the window is alpha blended
        if (exStyle & WS_EX_LAYERED)
        {
            if (!(flags & LWA_ALPHA))
                exStyle &= ~WS_EX_LAYERED;
        }
    }

    SetWindowLongW(window->win32.handle, GWL_EXSTYLE, exStyle);

    if (enabled)
        SetLayeredWindowAttributes(window->win32.handle, key, alpha, flags);
}

float _plfwGetWindowOpacityWin32(_PLFWwindow* window)
{
    BYTE alpha;
    DWORD flags;

    if ((GetWindowLongW(window->win32.handle, GWL_EXSTYLE) & WS_EX_LAYERED) &&
        GetLayeredWindowAttributes(window->win32.handle, NULL, &alpha, &flags))
    {
        if (flags & LWA_ALPHA)
            return alpha / 255.f;
    }

    return 1.f;
}

void _plfwSetWindowOpacityWin32(_PLFWwindow* window, float opacity)
{
    LONG exStyle = GetWindowLongW(window->win32.handle, GWL_EXSTYLE);
    if (opacity < 1.f || (exStyle & WS_EX_TRANSPARENT))
    {
        const BYTE alpha = (BYTE) (255 * opacity);
        exStyle |= WS_EX_LAYERED;
        SetWindowLongW(window->win32.handle, GWL_EXSTYLE, exStyle);
        SetLayeredWindowAttributes(window->win32.handle, 0, alpha, LWA_ALPHA);
    }
    else if (exStyle & WS_EX_TRANSPARENT)
    {
        SetLayeredWindowAttributes(window->win32.handle, 0, 0, 0);
    }
    else
    {
        exStyle &= ~WS_EX_LAYERED;
        SetWindowLongW(window->win32.handle, GWL_EXSTYLE, exStyle);
    }
}

void _plfwSetRawMouseMotionWin32(_PLFWwindow *window, PLFWbool enabled)
{
    if (_plfw.win32.disabledCursorWindow != window)
        return;

    if (enabled)
        enableRawMouseMotion(window);
    else
        disableRawMouseMotion(window);
}

PLFWbool _plfwRawMouseMotionSupportedWin32(void)
{
    return PLFW_TRUE;
}

void _plfwPollEventsWin32(void)
{
    MSG msg;
    HWND handle;
    _PLFWwindow* window;

    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
        {
            // NOTE: While PLFW does not itself post WM_QUIT, other processes
            //       may post it to this one, for example Task Manager
            // HACK: Treat WM_QUIT as a close on all windows

            window = _plfw.windowListHead;
            while (window)
            {
                _plfwInputWindowCloseRequest(window);
                window = window->next;
            }
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // HACK: Release modifier keys that the system did not emit KEYUP for
    // NOTE: Shift keys on Windows tend to "stick" when both are pressed as
    //       no key up message is generated by the first key release
    // NOTE: Windows key is not reported as released by the Win+V hotkey
    //       Other Win hotkeys are handled implicitly by _plfwInputWindowFocus
    //       because they change the input focus
    // NOTE: The other half of this is in the WM_*KEY* handler in windowProc
    handle = GetActiveWindow();
    if (handle)
    {
        window = GetPropW(handle, L"PLFW");
        if (window)
        {
            int i;
            const int keys[4][2] =
            {
                { VK_LSHIFT, PLFW_KEY_LEFT_SHIFT },
                { VK_RSHIFT, PLFW_KEY_RIGHT_SHIFT },
                { VK_LWIN, PLFW_KEY_LEFT_SUPER },
                { VK_RWIN, PLFW_KEY_RIGHT_SUPER }
            };

            for (i = 0;  i < 4;  i++)
            {
                const int vk = keys[i][0];
                const int key = keys[i][1];
                const int scancode = _plfw.win32.scancodes[key];

                if ((GetKeyState(vk) & 0x8000))
                    continue;
                if (window->keys[key] != PLFW_PRESS)
                    continue;

                _plfwInputKey(window, key, scancode, PLFW_RELEASE, getKeyMods());
            }
        }
    }

    window = _plfw.win32.disabledCursorWindow;
    if (window)
    {
        int width, height;
        _plfwGetWindowSizeWin32(window, &width, &height);

        // NOTE: Re-center the cursor only if it has moved since the last call,
        //       to avoid breaking plfwWaitEvents with WM_MOUSEMOVE
        // The re-center is required in order to prevent the mouse cursor stopping at the edges of the screen.
        if (window->win32.lastCursorPosX != width / 2 ||
            window->win32.lastCursorPosY != height / 2)
        {
            _plfwSetCursorPosWin32(window, width / 2, height / 2);
        }
    }
}

void _plfwWaitEventsWin32(void)
{
    WaitMessage();

    _plfwPollEventsWin32();
}

void _plfwWaitEventsTimeoutWin32(double timeout)
{
    MsgWaitForMultipleObjects(0, NULL, FALSE, (DWORD) (timeout * 1e3), QS_ALLINPUT);

    _plfwPollEventsWin32();
}

void _plfwPostEmptyEventWin32(void)
{
    PostMessageW(_plfw.win32.helperWindowHandle, WM_NULL, 0, 0);
}

void _plfwGetCursorPosWin32(_PLFWwindow* window, double* xpos, double* ypos)
{
    POINT pos;

    if (GetCursorPos(&pos))
    {
        ScreenToClient(window->win32.handle, &pos);

        if (xpos)
            *xpos = pos.x;
        if (ypos)
            *ypos = pos.y;
    }
}

void _plfwSetCursorPosWin32(_PLFWwindow* window, double xpos, double ypos)
{
    POINT pos = { (int) xpos, (int) ypos };

    // Store the new position so it can be recognized later
    window->win32.lastCursorPosX = pos.x;
    window->win32.lastCursorPosY = pos.y;

    ClientToScreen(window->win32.handle, &pos);
    SetCursorPos(pos.x, pos.y);
}

void _plfwSetCursorModeWin32(_PLFWwindow* window, int mode)
{
    if (_plfwWindowFocusedWin32(window))
    {
        if (mode == PLFW_CURSOR_DISABLED)
        {
            _plfwGetCursorPosWin32(window,
                                   &_plfw.win32.restoreCursorPosX,
                                   &_plfw.win32.restoreCursorPosY);
            _plfwCenterCursorInContentArea(window);
            if (window->rawMouseMotion)
                enableRawMouseMotion(window);
        }
        else if (_plfw.win32.disabledCursorWindow == window)
        {
            if (window->rawMouseMotion)
                disableRawMouseMotion(window);
        }

        if (mode == PLFW_CURSOR_DISABLED || mode == PLFW_CURSOR_CAPTURED)
            captureCursor(window);
        else
            releaseCursor();

        if (mode == PLFW_CURSOR_DISABLED)
            _plfw.win32.disabledCursorWindow = window;
        else if (_plfw.win32.disabledCursorWindow == window)
        {
            _plfw.win32.disabledCursorWindow = NULL;
            _plfwSetCursorPosWin32(window,
                                   _plfw.win32.restoreCursorPosX,
                                   _plfw.win32.restoreCursorPosY);
        }
    }

    if (cursorInContentArea(window))
        updateCursorImage(window);
}

const char* _plfwGetScancodeNameWin32(int scancode)
{
    if (scancode < 0 || scancode > (KF_EXTENDED | 0xff))
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid scancode %i", scancode);
        return NULL;
    }

    const int key = _plfw.win32.keycodes[scancode];
    if (key == PLFW_KEY_UNKNOWN)
        return NULL;

    return _plfw.win32.keynames[key];
}

int _plfwGetKeyScancodeWin32(int key)
{
    return _plfw.win32.scancodes[key];
}

PLFWbool _plfwCreateCursorWin32(_PLFWcursor* cursor,
                                const PLFWimage* image,
                                int xhot, int yhot)
{
    cursor->win32.handle = (HCURSOR) createIcon(image, xhot, yhot, PLFW_FALSE);
    if (!cursor->win32.handle)
        return PLFW_FALSE;

    return PLFW_TRUE;
}

PLFWbool _plfwCreateStandardCursorWin32(_PLFWcursor* cursor, int shape)
{
    int id = 0;

    switch (shape)
    {
        case PLFW_ARROW_CURSOR:
            id = OCR_NORMAL;
            break;
        case PLFW_IBEAM_CURSOR:
            id = OCR_IBEAM;
            break;
        case PLFW_CROSSHAIR_CURSOR:
            id = OCR_CROSS;
            break;
        case PLFW_POINTING_HAND_CURSOR:
            id = OCR_HAND;
            break;
        case PLFW_RESIZE_EW_CURSOR:
            id = OCR_SIZEWE;
            break;
        case PLFW_RESIZE_NS_CURSOR:
            id = OCR_SIZENS;
            break;
        case PLFW_RESIZE_NWSE_CURSOR:
            id = OCR_SIZENWSE;
            break;
        case PLFW_RESIZE_NESW_CURSOR:
            id = OCR_SIZENESW;
            break;
        case PLFW_RESIZE_ALL_CURSOR:
            id = OCR_SIZEALL;
            break;
        case PLFW_NOT_ALLOWED_CURSOR:
            id = OCR_NO;
            break;
        default:
            _plfwInputError(PLFW_PLATFORM_ERROR, "Win32: Unknown standard cursor");
            return PLFW_FALSE;
    }

    cursor->win32.handle = LoadImageW(NULL,
                                      MAKEINTRESOURCEW(id), IMAGE_CURSOR, 0, 0,
                                      LR_DEFAULTSIZE | LR_SHARED);
    if (!cursor->win32.handle)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to create standard cursor");
        return PLFW_FALSE;
    }

    return PLFW_TRUE;
}

void _plfwDestroyCursorWin32(_PLFWcursor* cursor)
{
    if (cursor->win32.handle)
        DestroyIcon((HICON) cursor->win32.handle);
}

void _plfwSetCursorWin32(_PLFWwindow* window, _PLFWcursor* cursor)
{
    if (cursorInContentArea(window))
        updateCursorImage(window);
}

void _plfwSetClipboardStringWin32(const char* string)
{
    int characterCount, tries = 0;
    HANDLE object;
    WCHAR* buffer;

    characterCount = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
    if (!characterCount)
        return;

    object = GlobalAlloc(GMEM_MOVEABLE, characterCount * sizeof(WCHAR));
    if (!object)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to allocate global handle for clipboard");
        return;
    }

    buffer = GlobalLock(object);
    if (!buffer)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to lock global handle");
        GlobalFree(object);
        return;
    }

    MultiByteToWideChar(CP_UTF8, 0, string, -1, buffer, characterCount);
    GlobalUnlock(object);

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    while (!OpenClipboard(_plfw.win32.helperWindowHandle))
    {
        Sleep(1);
        tries++;

        if (tries == 3)
        {
            _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                                 "Win32: Failed to open clipboard");
            GlobalFree(object);
            return;
        }
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, object);
    CloseClipboard();
}

const char* _plfwGetClipboardStringWin32(void)
{
    HANDLE object;
    WCHAR* buffer;
    int tries = 0;

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    while (!OpenClipboard(_plfw.win32.helperWindowHandle))
    {
        Sleep(1);
        tries++;

        if (tries == 3)
        {
            _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                                 "Win32: Failed to open clipboard");
            return NULL;
        }
    }

    object = GetClipboardData(CF_UNICODETEXT);
    if (!object)
    {
        _plfwInputErrorWin32(PLFW_FORMAT_UNAVAILABLE,
                             "Win32: Failed to convert clipboard to string");
        CloseClipboard();
        return NULL;
    }

    buffer = GlobalLock(object);
    if (!buffer)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to lock global handle");
        CloseClipboard();
        return NULL;
    }

    _plfw_free(_plfw.win32.clipboardString);
    _plfw.win32.clipboardString = _plfwCreateUTF8FromWideStringWin32(buffer);

    GlobalUnlock(object);
    CloseClipboard();

    return _plfw.win32.clipboardString;
}

PLFWAPI HWND plfwGetWin32Window(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_plfw.platform.platformID != PLFW_PLATFORM_WIN32)
    {
        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE,
                        "Win32: Platform not initialized");
        return NULL;
    }

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    return window->win32.handle;
}

#endif // _PLFW_WIN32

