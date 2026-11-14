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

#include <stdlib.h>

static const GUID _plfw_GUID_DEVINTERFACE_HID =
    {0x4d1e55b2,0xf16f,0x11cf,{0x88,0xcb,0x00,0x11,0x11,0x00,0x00,0x30}};

#define GUID_DEVINTERFACE_HID _plfw_GUID_DEVINTERFACE_HID

#if defined(_PLFW_USE_HYBRID_HPG)

#if defined(_PLFW_BUILD_DLL)
 #pragma message("These symbols must be exported by the executable and have no effect in a DLL")
#endif

// Executables (but not DLLs) exporting this symbol with this value will be
// automatically directed to the high-performance GPU on Nvidia Optimus systems
// with up-to-date drivers
//
__declspec(dllexport) DWORD NvOptimusEnablement = 1;

// Executables (but not DLLs) exporting this symbol with this value will be
// automatically directed to the high-performance GPU on AMD PowerXpress systems
// with up-to-date drivers
//
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

#endif // _PLFW_USE_HYBRID_HPG

#if defined(_PLFW_BUILD_DLL)

// PLFW DLL entry point
//
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    return TRUE;
}

#endif // _PLFW_BUILD_DLL

// Load necessary libraries (DLLs)
//
static PLFWbool loadLibraries(void)
{
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (const WCHAR*) &_plfw,
                            (HMODULE*) &_plfw.win32.instance))
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to retrieve own module handle");
        return PLFW_FALSE;
    }

    _plfw.win32.user32.instance = _plfwPlatformLoadModule("user32.dll");
    if (!_plfw.win32.user32.instance)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to load user32.dll");
        return PLFW_FALSE;
    }

    _plfw.win32.user32.EnableNonClientDpiScaling_ = (PFN_EnableNonClientDpiScaling)
        _plfwPlatformGetModuleSymbol(_plfw.win32.user32.instance, "EnableNonClientDpiScaling");
    _plfw.win32.user32.SetProcessDpiAwarenessContext_ = (PFN_SetProcessDpiAwarenessContext)
        _plfwPlatformGetModuleSymbol(_plfw.win32.user32.instance, "SetProcessDpiAwarenessContext");
    _plfw.win32.user32.GetDpiForWindow_ = (PFN_GetDpiForWindow)
        _plfwPlatformGetModuleSymbol(_plfw.win32.user32.instance, "GetDpiForWindow");
    _plfw.win32.user32.AdjustWindowRectExForDpi_ = (PFN_AdjustWindowRectExForDpi)
        _plfwPlatformGetModuleSymbol(_plfw.win32.user32.instance, "AdjustWindowRectExForDpi");
    _plfw.win32.user32.GetSystemMetricsForDpi_ = (PFN_GetSystemMetricsForDpi)
        _plfwPlatformGetModuleSymbol(_plfw.win32.user32.instance, "GetSystemMetricsForDpi");

    _plfw.win32.dinput8.instance = _plfwPlatformLoadModule("dinput8.dll");
    if (_plfw.win32.dinput8.instance)
    {
        _plfw.win32.dinput8.Create = (PFN_DirectInput8Create)
            _plfwPlatformGetModuleSymbol(_plfw.win32.dinput8.instance, "DirectInput8Create");
    }

    {
        int i;
        const char* names[] =
        {
            "xinput1_4.dll",
            "xinput1_3.dll",
            "xinput9_1_0.dll",
            "xinput1_2.dll",
            "xinput1_1.dll",
            NULL
        };

        for (i = 0;  names[i];  i++)
        {
            _plfw.win32.xinput.instance = _plfwPlatformLoadModule(names[i]);
            if (_plfw.win32.xinput.instance)
            {
                _plfw.win32.xinput.GetCapabilities = (PFN_XInputGetCapabilities)
                    _plfwPlatformGetModuleSymbol(_plfw.win32.xinput.instance, "XInputGetCapabilities");
                _plfw.win32.xinput.GetState = (PFN_XInputGetState)
                    _plfwPlatformGetModuleSymbol(_plfw.win32.xinput.instance, "XInputGetState");

                break;
            }
        }
    }

    _plfw.win32.shcore.instance = _plfwPlatformLoadModule("shcore.dll");
    if (_plfw.win32.shcore.instance)
    {
        _plfw.win32.shcore.SetProcessDpiAwareness_ = (PFN_SetProcessDpiAwareness)
            _plfwPlatformGetModuleSymbol(_plfw.win32.shcore.instance, "SetProcessDpiAwareness");
        _plfw.win32.shcore.GetDpiForMonitor_ = (PFN_GetDpiForMonitor)
            _plfwPlatformGetModuleSymbol(_plfw.win32.shcore.instance, "GetDpiForMonitor");
    }

    _plfw.win32.ntdll.instance = _plfwPlatformLoadModule("ntdll.dll");
    if (_plfw.win32.ntdll.instance)
    {
        _plfw.win32.ntdll.RtlVerifyVersionInfo_ = (PFN_RtlVerifyVersionInfo)
            _plfwPlatformGetModuleSymbol(_plfw.win32.ntdll.instance, "RtlVerifyVersionInfo");
    }

    return PLFW_TRUE;
}

// Create key code translation tables
//
static void createKeyTables(void)
{
    int scancode;

    memset(_plfw.win32.keycodes, -1, sizeof(_plfw.win32.keycodes));
    memset(_plfw.win32.scancodes, -1, sizeof(_plfw.win32.scancodes));

    _plfw.win32.keycodes[0x00B] = PLFW_KEY_0;
    _plfw.win32.keycodes[0x002] = PLFW_KEY_1;
    _plfw.win32.keycodes[0x003] = PLFW_KEY_2;
    _plfw.win32.keycodes[0x004] = PLFW_KEY_3;
    _plfw.win32.keycodes[0x005] = PLFW_KEY_4;
    _plfw.win32.keycodes[0x006] = PLFW_KEY_5;
    _plfw.win32.keycodes[0x007] = PLFW_KEY_6;
    _plfw.win32.keycodes[0x008] = PLFW_KEY_7;
    _plfw.win32.keycodes[0x009] = PLFW_KEY_8;
    _plfw.win32.keycodes[0x00A] = PLFW_KEY_9;
    _plfw.win32.keycodes[0x01E] = PLFW_KEY_A;
    _plfw.win32.keycodes[0x030] = PLFW_KEY_B;
    _plfw.win32.keycodes[0x02E] = PLFW_KEY_C;
    _plfw.win32.keycodes[0x020] = PLFW_KEY_D;
    _plfw.win32.keycodes[0x012] = PLFW_KEY_E;
    _plfw.win32.keycodes[0x021] = PLFW_KEY_F;
    _plfw.win32.keycodes[0x022] = PLFW_KEY_G;
    _plfw.win32.keycodes[0x023] = PLFW_KEY_H;
    _plfw.win32.keycodes[0x017] = PLFW_KEY_I;
    _plfw.win32.keycodes[0x024] = PLFW_KEY_J;
    _plfw.win32.keycodes[0x025] = PLFW_KEY_K;
    _plfw.win32.keycodes[0x026] = PLFW_KEY_L;
    _plfw.win32.keycodes[0x032] = PLFW_KEY_M;
    _plfw.win32.keycodes[0x031] = PLFW_KEY_N;
    _plfw.win32.keycodes[0x018] = PLFW_KEY_O;
    _plfw.win32.keycodes[0x019] = PLFW_KEY_P;
    _plfw.win32.keycodes[0x010] = PLFW_KEY_Q;
    _plfw.win32.keycodes[0x013] = PLFW_KEY_R;
    _plfw.win32.keycodes[0x01F] = PLFW_KEY_S;
    _plfw.win32.keycodes[0x014] = PLFW_KEY_T;
    _plfw.win32.keycodes[0x016] = PLFW_KEY_U;
    _plfw.win32.keycodes[0x02F] = PLFW_KEY_V;
    _plfw.win32.keycodes[0x011] = PLFW_KEY_W;
    _plfw.win32.keycodes[0x02D] = PLFW_KEY_X;
    _plfw.win32.keycodes[0x015] = PLFW_KEY_Y;
    _plfw.win32.keycodes[0x02C] = PLFW_KEY_Z;

    _plfw.win32.keycodes[0x028] = PLFW_KEY_APOSTROPHE;
    _plfw.win32.keycodes[0x02B] = PLFW_KEY_BACKSLASH;
    _plfw.win32.keycodes[0x033] = PLFW_KEY_COMMA;
    _plfw.win32.keycodes[0x00D] = PLFW_KEY_EQUAL;
    _plfw.win32.keycodes[0x029] = PLFW_KEY_GRAVE_ACCENT;
    _plfw.win32.keycodes[0x01A] = PLFW_KEY_LEFT_BRACKET;
    _plfw.win32.keycodes[0x00C] = PLFW_KEY_MINUS;
    _plfw.win32.keycodes[0x034] = PLFW_KEY_PERIOD;
    _plfw.win32.keycodes[0x01B] = PLFW_KEY_RIGHT_BRACKET;
    _plfw.win32.keycodes[0x027] = PLFW_KEY_SEMICOLON;
    _plfw.win32.keycodes[0x035] = PLFW_KEY_SLASH;
    _plfw.win32.keycodes[0x056] = PLFW_KEY_WORLD_2;

    _plfw.win32.keycodes[0x00E] = PLFW_KEY_BACKSPACE;
    _plfw.win32.keycodes[0x153] = PLFW_KEY_DELETE;
    _plfw.win32.keycodes[0x14F] = PLFW_KEY_END;
    _plfw.win32.keycodes[0x01C] = PLFW_KEY_ENTER;
    _plfw.win32.keycodes[0x001] = PLFW_KEY_ESCAPE;
    _plfw.win32.keycodes[0x147] = PLFW_KEY_HOME;
    _plfw.win32.keycodes[0x152] = PLFW_KEY_INSERT;
    _plfw.win32.keycodes[0x15D] = PLFW_KEY_MENU;
    _plfw.win32.keycodes[0x151] = PLFW_KEY_PAGE_DOWN;
    _plfw.win32.keycodes[0x149] = PLFW_KEY_PAGE_UP;
    _plfw.win32.keycodes[0x045] = PLFW_KEY_PAUSE;
    _plfw.win32.keycodes[0x039] = PLFW_KEY_SPACE;
    _plfw.win32.keycodes[0x00F] = PLFW_KEY_TAB;
    _plfw.win32.keycodes[0x03A] = PLFW_KEY_CAPS_LOCK;
    _plfw.win32.keycodes[0x145] = PLFW_KEY_NUM_LOCK;
    _plfw.win32.keycodes[0x046] = PLFW_KEY_SCROLL_LOCK;
    _plfw.win32.keycodes[0x03B] = PLFW_KEY_F1;
    _plfw.win32.keycodes[0x03C] = PLFW_KEY_F2;
    _plfw.win32.keycodes[0x03D] = PLFW_KEY_F3;
    _plfw.win32.keycodes[0x03E] = PLFW_KEY_F4;
    _plfw.win32.keycodes[0x03F] = PLFW_KEY_F5;
    _plfw.win32.keycodes[0x040] = PLFW_KEY_F6;
    _plfw.win32.keycodes[0x041] = PLFW_KEY_F7;
    _plfw.win32.keycodes[0x042] = PLFW_KEY_F8;
    _plfw.win32.keycodes[0x043] = PLFW_KEY_F9;
    _plfw.win32.keycodes[0x044] = PLFW_KEY_F10;
    _plfw.win32.keycodes[0x057] = PLFW_KEY_F11;
    _plfw.win32.keycodes[0x058] = PLFW_KEY_F12;
    _plfw.win32.keycodes[0x064] = PLFW_KEY_F13;
    _plfw.win32.keycodes[0x065] = PLFW_KEY_F14;
    _plfw.win32.keycodes[0x066] = PLFW_KEY_F15;
    _plfw.win32.keycodes[0x067] = PLFW_KEY_F16;
    _plfw.win32.keycodes[0x068] = PLFW_KEY_F17;
    _plfw.win32.keycodes[0x069] = PLFW_KEY_F18;
    _plfw.win32.keycodes[0x06A] = PLFW_KEY_F19;
    _plfw.win32.keycodes[0x06B] = PLFW_KEY_F20;
    _plfw.win32.keycodes[0x06C] = PLFW_KEY_F21;
    _plfw.win32.keycodes[0x06D] = PLFW_KEY_F22;
    _plfw.win32.keycodes[0x06E] = PLFW_KEY_F23;
    _plfw.win32.keycodes[0x076] = PLFW_KEY_F24;
    _plfw.win32.keycodes[0x038] = PLFW_KEY_LEFT_ALT;
    _plfw.win32.keycodes[0x01D] = PLFW_KEY_LEFT_CONTROL;
    _plfw.win32.keycodes[0x02A] = PLFW_KEY_LEFT_SHIFT;
    _plfw.win32.keycodes[0x15B] = PLFW_KEY_LEFT_SUPER;
    _plfw.win32.keycodes[0x137] = PLFW_KEY_PRINT_SCREEN;
    _plfw.win32.keycodes[0x138] = PLFW_KEY_RIGHT_ALT;
    _plfw.win32.keycodes[0x11D] = PLFW_KEY_RIGHT_CONTROL;
    _plfw.win32.keycodes[0x036] = PLFW_KEY_RIGHT_SHIFT;
    _plfw.win32.keycodes[0x15C] = PLFW_KEY_RIGHT_SUPER;
    _plfw.win32.keycodes[0x150] = PLFW_KEY_DOWN;
    _plfw.win32.keycodes[0x14B] = PLFW_KEY_LEFT;
    _plfw.win32.keycodes[0x14D] = PLFW_KEY_RIGHT;
    _plfw.win32.keycodes[0x148] = PLFW_KEY_UP;

    _plfw.win32.keycodes[0x052] = PLFW_KEY_KP_0;
    _plfw.win32.keycodes[0x04F] = PLFW_KEY_KP_1;
    _plfw.win32.keycodes[0x050] = PLFW_KEY_KP_2;
    _plfw.win32.keycodes[0x051] = PLFW_KEY_KP_3;
    _plfw.win32.keycodes[0x04B] = PLFW_KEY_KP_4;
    _plfw.win32.keycodes[0x04C] = PLFW_KEY_KP_5;
    _plfw.win32.keycodes[0x04D] = PLFW_KEY_KP_6;
    _plfw.win32.keycodes[0x047] = PLFW_KEY_KP_7;
    _plfw.win32.keycodes[0x048] = PLFW_KEY_KP_8;
    _plfw.win32.keycodes[0x049] = PLFW_KEY_KP_9;
    _plfw.win32.keycodes[0x04E] = PLFW_KEY_KP_ADD;
    _plfw.win32.keycodes[0x053] = PLFW_KEY_KP_DECIMAL;
    _plfw.win32.keycodes[0x135] = PLFW_KEY_KP_DIVIDE;
    _plfw.win32.keycodes[0x11C] = PLFW_KEY_KP_ENTER;
    _plfw.win32.keycodes[0x059] = PLFW_KEY_KP_EQUAL;
    _plfw.win32.keycodes[0x037] = PLFW_KEY_KP_MULTIPLY;
    _plfw.win32.keycodes[0x04A] = PLFW_KEY_KP_SUBTRACT;

    for (scancode = 0;  scancode < 512;  scancode++)
    {
        if (_plfw.win32.keycodes[scancode] > 0)
            _plfw.win32.scancodes[_plfw.win32.keycodes[scancode]] = scancode;
    }
}

// Window procedure for the hidden helper window
//
static LRESULT CALLBACK helperWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DISPLAYCHANGE:
            _plfwPollMonitorsWin32();
            break;

        case WM_DEVICECHANGE:
        {
            if (!_plfw.joysticksInitialized)
                break;

            if (wParam == DBT_DEVICEARRIVAL)
            {
                DEV_BROADCAST_HDR* dbh = (DEV_BROADCAST_HDR*) lParam;
                if (dbh && dbh->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                    _plfwDetectJoystickConnectionWin32();
            }
            else if (wParam == DBT_DEVICEREMOVECOMPLETE)
            {
                DEV_BROADCAST_HDR* dbh = (DEV_BROADCAST_HDR*) lParam;
                if (dbh && dbh->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE)
                    _plfwDetectJoystickDisconnectionWin32();
            }

            break;
        }
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// Creates a dummy window for behind-the-scenes work
//
static PLFWbool createHelperWindow(void)
{
    MSG msg;
    WNDCLASSEXW wc = { sizeof(wc) };

    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = (WNDPROC) helperWindowProc;
    wc.hInstance     = _plfw.win32.instance;
    wc.lpszClassName = L"PLFW3 Helper";

    _plfw.win32.helperWindowClass = RegisterClassExW(&wc);
    if (!_plfw.win32.helperWindowClass)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to register helper window class");
        return PLFW_FALSE;
    }

    _plfw.win32.helperWindowHandle =
        CreateWindowExW(WS_EX_OVERLAPPEDWINDOW,
                        MAKEINTATOM(_plfw.win32.helperWindowClass),
                        L"PLFW message window",
                        WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                        0, 0, 1, 1,
                        NULL, NULL,
                        _plfw.win32.instance,
                        NULL);

    if (!_plfw.win32.helperWindowHandle)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to create helper window");
        return PLFW_FALSE;
    }

    // HACK: The command to the first ShowWindow call is ignored if the parent
    //       process passed along a STARTUPINFO, so clear that with a no-op call
    ShowWindow(_plfw.win32.helperWindowHandle, SW_HIDE);

    // Register for HID device notifications
    {
        DEV_BROADCAST_DEVICEINTERFACE_W dbi;
        ZeroMemory(&dbi, sizeof(dbi));
        dbi.dbcc_size = sizeof(dbi);
        dbi.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        dbi.dbcc_classguid = GUID_DEVINTERFACE_HID;

        _plfw.win32.deviceNotificationHandle =
            RegisterDeviceNotificationW(_plfw.win32.helperWindowHandle,
                                        (DEV_BROADCAST_HDR*) &dbi,
                                        DEVICE_NOTIFY_WINDOW_HANDLE);
    }

    while (PeekMessageW(&msg, _plfw.win32.helperWindowHandle, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

   return PLFW_TRUE;
}

//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Returns a wide string version of the specified UTF-8 string
//
WCHAR* _plfwCreateWideStringFromUTF8Win32(const char* source)
{
    WCHAR* target;
    int count;

    count = MultiByteToWideChar(CP_UTF8, 0, source, -1, NULL, 0);
    if (!count)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to convert string from UTF-8");
        return NULL;
    }

    target = _plfw_calloc(count, sizeof(WCHAR));

    if (!MultiByteToWideChar(CP_UTF8, 0, source, -1, target, count))
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to convert string from UTF-8");
        _plfw_free(target);
        return NULL;
    }

    return target;
}

// Returns a UTF-8 string version of the specified wide string
//
char* _plfwCreateUTF8FromWideStringWin32(const WCHAR* source)
{
    char* target;
    int size;

    size = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (!size)
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to convert string to UTF-8");
        return NULL;
    }

    target = _plfw_calloc(size, 1);

    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, size, NULL, NULL))
    {
        _plfwInputErrorWin32(PLFW_PLATFORM_ERROR,
                             "Win32: Failed to convert string to UTF-8");
        _plfw_free(target);
        return NULL;
    }

    return target;
}

// Reports the specified error, appending information about the last Win32 error
//
void _plfwInputErrorWin32(int error, const char* description)
{
    WCHAR buffer[_PLFW_MESSAGE_SIZE] = L"";
    char message[_PLFW_MESSAGE_SIZE] = "";

    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS |
                       FORMAT_MESSAGE_MAX_WIDTH_MASK,
                   NULL,
                   GetLastError() & 0xffff,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer,
                   sizeof(buffer) / sizeof(WCHAR),
                   NULL);
    WideCharToMultiByte(CP_UTF8, 0, buffer, -1, message, sizeof(message), NULL, NULL);

    _plfwInputError(error, "%s: %s", description, message);
}

// Updates key names according to the current keyboard layout
//
void _plfwUpdateKeyNamesWin32(void)
{
    int key;
    BYTE state[256] = {0};

    memset(_plfw.win32.keynames, 0, sizeof(_plfw.win32.keynames));

    for (key = PLFW_KEY_SPACE;  key <= PLFW_KEY_LAST;  key++)
    {
        UINT vk;
        int scancode, length;
        WCHAR chars[16];

        scancode = _plfw.win32.scancodes[key];
        if (scancode == -1)
            continue;

        if (key >= PLFW_KEY_KP_0 && key <= PLFW_KEY_KP_ADD)
        {
            const UINT vks[] =
            {
                VK_NUMPAD0,  VK_NUMPAD1,  VK_NUMPAD2, VK_NUMPAD3,
                VK_NUMPAD4,  VK_NUMPAD5,  VK_NUMPAD6, VK_NUMPAD7,
                VK_NUMPAD8,  VK_NUMPAD9,  VK_DECIMAL, VK_DIVIDE,
                VK_MULTIPLY, VK_SUBTRACT, VK_ADD
            };

            vk = vks[key - PLFW_KEY_KP_0];
        }
        else
            vk = MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK);

        length = ToUnicode(vk, scancode, state,
                           chars, sizeof(chars) / sizeof(WCHAR),
                           0);

        if (length == -1)
        {
            // This is a dead key, so we need a second simulated key press
            // to make it output its own character (usually a diacritic)
            length = ToUnicode(vk, scancode, state,
                               chars, sizeof(chars) / sizeof(WCHAR),
                               0);
        }

        if (length < 1)
            continue;

        WideCharToMultiByte(CP_UTF8, 0, chars, 1,
                            _plfw.win32.keynames[key],
                            sizeof(_plfw.win32.keynames[key]),
                            NULL, NULL);
    }
}

// Replacement for IsWindowsVersionOrGreater, as we cannot rely on the
// application having a correct embedded manifest
//
BOOL _plfwIsWindowsVersionOrGreaterWin32(WORD major, WORD minor, WORD sp)
{
    OSVERSIONINFOEXW osvi = { sizeof(osvi), major, minor, 0, 0, {0}, sp };
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    // HACK: Use RtlVerifyVersionInfo instead of VerifyVersionInfoW as the
    //       latter lies unless the user knew to embed a non-default manifest
    //       announcing support for Windows 10 via supportedOS GUID
    return RtlVerifyVersionInfo(&osvi, mask, cond) == 0;
}

// Checks whether we are on at least the specified build of Windows 10
//
BOOL _plfwIsWindows10BuildOrGreaterWin32(WORD build)
{
    OSVERSIONINFOEXW osvi = { sizeof(osvi), 10, 0, build };
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER;
    ULONGLONG cond = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_MINORVERSION, VER_GREATER_EQUAL);
    cond = VerSetConditionMask(cond, VER_BUILDNUMBER, VER_GREATER_EQUAL);
    // HACK: Use RtlVerifyVersionInfo instead of VerifyVersionInfoW as the
    //       latter lies unless the user knew to embed a non-default manifest
    //       announcing support for Windows 10 via supportedOS GUID
    return RtlVerifyVersionInfo(&osvi, mask, cond) == 0;
}

PLFWbool _plfwConnectWin32(int platformID, _PLFWplatform* platform)
{
    const _PLFWplatform win32 =
    {
        .platformID = PLFW_PLATFORM_WIN32,
        .init = _plfwInitWin32,
        .terminate = _plfwTerminateWin32,
        .getCursorPos = _plfwGetCursorPosWin32,
        .setCursorPos = _plfwSetCursorPosWin32,
        .setCursorMode = _plfwSetCursorModeWin32,
        .setRawMouseMotion = _plfwSetRawMouseMotionWin32,
        .rawMouseMotionSupported = _plfwRawMouseMotionSupportedWin32,
        .createCursor = _plfwCreateCursorWin32,
        .createStandardCursor = _plfwCreateStandardCursorWin32,
        .destroyCursor = _plfwDestroyCursorWin32,
        .setCursor = _plfwSetCursorWin32,
        .getScancodeName = _plfwGetScancodeNameWin32,
        .getKeyScancode = _plfwGetKeyScancodeWin32,
        .setClipboardString = _plfwSetClipboardStringWin32,
        .getClipboardString = _plfwGetClipboardStringWin32,
        .initJoysticks = _plfwInitJoysticksWin32,
        .terminateJoysticks = _plfwTerminateJoysticksWin32,
        .pollJoystick = _plfwPollJoystickWin32,
        .getMappingName = _plfwGetMappingNameWin32,
        .updateGamepadGUID = _plfwUpdateGamepadGUIDWin32,
        .freeMonitor = _plfwFreeMonitorWin32,
        .getMonitorPos = _plfwGetMonitorPosWin32,
        .getMonitorContentScale = _plfwGetMonitorContentScaleWin32,
        .getMonitorWorkarea = _plfwGetMonitorWorkareaWin32,
        .getVideoModes = _plfwGetVideoModesWin32,
        .getVideoMode = _plfwGetVideoModeWin32,
        .getGammaRamp = _plfwGetGammaRampWin32,
        .setGammaRamp = _plfwSetGammaRampWin32,
        .createWindow = _plfwCreateWindowWin32,
        .destroyWindow = _plfwDestroyWindowWin32,
        .setWindowTitle = _plfwSetWindowTitleWin32,
        .setWindowIcon = _plfwSetWindowIconWin32,
        .getWindowPos = _plfwGetWindowPosWin32,
        .setWindowPos = _plfwSetWindowPosWin32,
        .getWindowSize = _plfwGetWindowSizeWin32,
        .setWindowSize = _plfwSetWindowSizeWin32,
        .setWindowSizeLimits = _plfwSetWindowSizeLimitsWin32,
        .setWindowAspectRatio = _plfwSetWindowAspectRatioWin32,
        .getFramebufferSize = _plfwGetFramebufferSizeWin32,
        .getWindowFrameSize = _plfwGetWindowFrameSizeWin32,
        .getWindowContentScale = _plfwGetWindowContentScaleWin32,
        .iconifyWindow = _plfwIconifyWindowWin32,
        .restoreWindow = _plfwRestoreWindowWin32,
        .maximizeWindow = _plfwMaximizeWindowWin32,
        .showWindow = _plfwShowWindowWin32,
        .hideWindow = _plfwHideWindowWin32,
        .requestWindowAttention = _plfwRequestWindowAttentionWin32,
        .focusWindow = _plfwFocusWindowWin32,
        .setWindowMonitor = _plfwSetWindowMonitorWin32,
        .windowFocused = _plfwWindowFocusedWin32,
        .windowIconified = _plfwWindowIconifiedWin32,
        .windowVisible = _plfwWindowVisibleWin32,
        .windowMaximized = _plfwWindowMaximizedWin32,
        .windowHovered = _plfwWindowHoveredWin32,
        .getWindowOpacity = _plfwGetWindowOpacityWin32,
        .setWindowResizable = _plfwSetWindowResizableWin32,
        .setWindowDecorated = _plfwSetWindowDecoratedWin32,
        .setWindowFloating = _plfwSetWindowFloatingWin32,
        .setWindowOpacity = _plfwSetWindowOpacityWin32,
        .setWindowMousePassthrough = _plfwSetWindowMousePassthroughWin32,
        .pollEvents = _plfwPollEventsWin32,
        .waitEvents = _plfwWaitEventsWin32,
        .waitEventsTimeout = _plfwWaitEventsTimeoutWin32,
        .postEmptyEvent = _plfwPostEmptyEventWin32
    };

    *platform = win32;
    return PLFW_TRUE;
}

int _plfwInitWin32(void)
{
    if (!loadLibraries())
        return PLFW_FALSE;

    createKeyTables();
    _plfwUpdateKeyNamesWin32();

    if (_plfwIsWindows10Version1703OrGreaterWin32())
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    else if (IsWindows8Point1OrGreater())
        SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    else
        SetProcessDPIAware();

    if (!createHelperWindow())
        return PLFW_FALSE;

    _plfwPollMonitorsWin32();
    return PLFW_TRUE;
}

void _plfwTerminateWin32(void)
{
    if (_plfw.win32.blankCursor)
        DestroyIcon((HICON) _plfw.win32.blankCursor);

    if (_plfw.win32.deviceNotificationHandle)
        UnregisterDeviceNotification(_plfw.win32.deviceNotificationHandle);

    if (_plfw.win32.helperWindowHandle)
        DestroyWindow(_plfw.win32.helperWindowHandle);
    if (_plfw.win32.helperWindowClass)
        UnregisterClassW(MAKEINTATOM(_plfw.win32.helperWindowClass), _plfw.win32.instance);
    if (_plfw.win32.mainWindowClass)
        UnregisterClassW(MAKEINTATOM(_plfw.win32.mainWindowClass), _plfw.win32.instance);

    _plfw_free(_plfw.win32.clipboardString);
    _plfw_free(_plfw.win32.rawInput);

    _plfwPlatformFreeModule(_plfw.win32.xinput.instance);
    _plfwPlatformFreeModule(_plfw.win32.dinput8.instance);
    _plfwPlatformFreeModule(_plfw.win32.user32.instance);
    _plfwPlatformFreeModule(_plfw.win32.shcore.instance);
    _plfwPlatformFreeModule(_plfw.win32.ntdll.instance);

    memset(&_plfw.win32, 0, sizeof(_plfw.win32));
}

#endif // _PLFW_WIN32

