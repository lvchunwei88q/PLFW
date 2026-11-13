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

// We don't need all the fancy stuff
#ifndef NOMINMAX
 #define NOMINMAX
#endif

#ifndef VC_EXTRALEAN
 #define VC_EXTRALEAN
#endif

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif

// This is a workaround for the fact that plfw3.h needs to export APIENTRY (for
// example to allow applications to correctly declare a GL_KHR_debug callback)
// but windows.h assumes no one will define APIENTRY before it does
#undef APIENTRY

// PLFW on Windows is Unicode only and does not work in MBCS mode
#ifndef UNICODE
 #define UNICODE
#endif

// PLFW requires Windows 7 or later
#if WINVER < 0x0601
 #undef WINVER
 #define WINVER 0x0601
#endif
#if _WIN32_WINNT < 0x0601
 #undef _WIN32_WINNT
 #define _WIN32_WINNT 0x0601
#endif

// PLFW uses DirectInput8 interfaces
#define DIRECTINPUT_VERSION 0x0800

// PLFW uses OEM cursor resources
#define OEMRESOURCE

#include <wctype.h>
#include <windows.h>
#include <dwmapi.h>
#include <dinput.h>
#include <xinput.h>
#include <dbt.h>

// HACK: Define macros that some windows.h variants don't
#ifndef WM_COPYGLOBALDATA
 #define WM_COPYGLOBALDATA 0x0049
#endif
#ifndef WM_DPICHANGED
 #define WM_DPICHANGED 0x02E0
#endif
#ifndef EDS_ROTATEDMODE
 #define EDS_ROTATEDMODE 0x00000004
#endif
#ifndef _WIN32_WINNT_WINBLUE
 #define _WIN32_WINNT_WINBLUE 0x0603
#endif
#ifndef _WIN32_WINNT_WIN8
 #define _WIN32_WINNT_WIN8 0x0602
#endif
#ifndef WM_GETDPISCALEDSIZE
 #define WM_GETDPISCALEDSIZE 0x02e4
#endif
#ifndef USER_DEFAULT_SCREEN_DPI
 #define USER_DEFAULT_SCREEN_DPI 96
#endif

#ifndef DPI_ENUMS_DECLARED
typedef enum
{
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
} PROCESS_DPI_AWARENESS;
typedef enum
{
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;
#endif /*DPI_ENUMS_DECLARED*/

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((HANDLE) -4)
#endif /*DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2*/

// Replacement for versionhelpers.h macros, as we cannot rely on the
// application having a correct embedded manifest
//
#define IsWindows8OrGreater()                                         \
    _plfwIsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WIN8),    \
                                        LOBYTE(_WIN32_WINNT_WIN8), 0)
#define IsWindows8Point1OrGreater()                                   \
    _plfwIsWindowsVersionOrGreaterWin32(HIBYTE(_WIN32_WINNT_WINBLUE), \
                                        LOBYTE(_WIN32_WINNT_WINBLUE), 0)

// Windows 10 Anniversary Update
#define _plfwIsWindows10Version1607OrGreaterWin32() \
    _plfwIsWindows10BuildOrGreaterWin32(14393)
// Windows 10 Creators Update
#define _plfwIsWindows10Version1703OrGreaterWin32() \
    _plfwIsWindows10BuildOrGreaterWin32(15063)

// HACK: Define macros that some xinput.h variants don't
#ifndef XINPUT_CAPS_WIRELESS
 #define XINPUT_CAPS_WIRELESS 0x0002
#endif
#ifndef XINPUT_DEVSUBTYPE_WHEEL
 #define XINPUT_DEVSUBTYPE_WHEEL 0x02
#endif
#ifndef XINPUT_DEVSUBTYPE_ARCADE_STICK
 #define XINPUT_DEVSUBTYPE_ARCADE_STICK 0x03
#endif
#ifndef XINPUT_DEVSUBTYPE_FLIGHT_STICK
 #define XINPUT_DEVSUBTYPE_FLIGHT_STICK 0x04
#endif
#ifndef XINPUT_DEVSUBTYPE_DANCE_PAD
 #define XINPUT_DEVSUBTYPE_DANCE_PAD 0x05
#endif
#ifndef XINPUT_DEVSUBTYPE_GUITAR
 #define XINPUT_DEVSUBTYPE_GUITAR 0x06
#endif
#ifndef XINPUT_DEVSUBTYPE_DRUM_KIT
 #define XINPUT_DEVSUBTYPE_DRUM_KIT 0x08
#endif
#ifndef XINPUT_DEVSUBTYPE_ARCADE_PAD
 #define XINPUT_DEVSUBTYPE_ARCADE_PAD 0x13
#endif
#ifndef XUSER_MAX_COUNT
 #define XUSER_MAX_COUNT 4
#endif

// HACK: Define macros that some dinput.h variants don't
#ifndef DIDFT_OPTIONAL
 #define DIDFT_OPTIONAL 0x80000000
#endif

// xinput.dll function pointer typedefs
typedef DWORD (WINAPI * PFN_XInputGetCapabilities)(DWORD,DWORD,XINPUT_CAPABILITIES*);
typedef DWORD (WINAPI * PFN_XInputGetState)(DWORD,XINPUT_STATE*);
#define XInputGetCapabilities _plfw.win32.xinput.GetCapabilities
#define XInputGetState _plfw.win32.xinput.GetState

// dinput8.dll function pointer typedefs
typedef HRESULT (WINAPI * PFN_DirectInput8Create)(HINSTANCE,DWORD,REFIID,LPVOID*,LPUNKNOWN);
#define DirectInput8Create _plfw.win32.dinput8.Create

// user32.dll function pointer typedefs
typedef BOOL (WINAPI * PFN_EnableNonClientDpiScaling)(HWND);
typedef BOOL (WINAPI * PFN_SetProcessDpiAwarenessContext)(HANDLE);
typedef UINT (WINAPI * PFN_GetDpiForWindow)(HWND);
typedef BOOL (WINAPI * PFN_AdjustWindowRectExForDpi)(LPRECT,DWORD,BOOL,DWORD,UINT);
typedef int (WINAPI * PFN_GetSystemMetricsForDpi)(int,UINT);
#define EnableNonClientDpiScaling _plfw.win32.user32.EnableNonClientDpiScaling_
#define SetProcessDpiAwarenessContext _plfw.win32.user32.SetProcessDpiAwarenessContext_
#define GetDpiForWindow _plfw.win32.user32.GetDpiForWindow_
#define AdjustWindowRectExForDpi _plfw.win32.user32.AdjustWindowRectExForDpi_
#define GetSystemMetricsForDpi _plfw.win32.user32.GetSystemMetricsForDpi_

// dwmapi.dll function pointer typedefs
typedef HRESULT (WINAPI * PFN_DwmIsCompositionEnabled)(BOOL*);
typedef HRESULT (WINAPI * PFN_DwmFlush)(VOID);
typedef HRESULT(WINAPI * PFN_DwmEnableBlurBehindWindow)(HWND,const DWM_BLURBEHIND*);
typedef HRESULT (WINAPI * PFN_DwmGetColorizationColor)(DWORD*,BOOL*);
#define DwmIsCompositionEnabled _plfw.win32.dwmapi.IsCompositionEnabled
#define DwmFlush _plfw.win32.dwmapi.Flush
#define DwmEnableBlurBehindWindow _plfw.win32.dwmapi.EnableBlurBehindWindow
#define DwmGetColorizationColor _plfw.win32.dwmapi.GetColorizationColor

// shcore.dll function pointer typedefs
typedef HRESULT (WINAPI * PFN_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS);
typedef HRESULT (WINAPI * PFN_GetDpiForMonitor)(HMONITOR,MONITOR_DPI_TYPE,UINT*,UINT*);
#define SetProcessDpiAwareness _plfw.win32.shcore.SetProcessDpiAwareness_
#define GetDpiForMonitor _plfw.win32.shcore.GetDpiForMonitor_

// ntdll.dll function pointer typedefs
typedef LONG (WINAPI * PFN_RtlVerifyVersionInfo)(OSVERSIONINFOEXW*,ULONG,ULONGLONG);
#define RtlVerifyVersionInfo _plfw.win32.ntdll.RtlVerifyVersionInfo_

#define PLFW_WIN32_WINDOW_STATE         _PLFWwindowWin32  win32;
#define PLFW_WIN32_LIBRARY_WINDOW_STATE _PLFWlibraryWin32 win32;
#define PLFW_WIN32_MONITOR_STATE        _PLFWmonitorWin32 win32;
#define PLFW_WIN32_CURSOR_STATE         _PLFWcursorWin32  win32;

// Win32-specific per-window data
//
typedef struct _PLFWwindowWin32
{
    HWND                handle;
    HICON               bigIcon;
    HICON               smallIcon;

    PLFWbool            cursorTracked;
    PLFWbool            frameAction;
    PLFWbool            iconified;
    PLFWbool            maximized;
    // Whether to enable framebuffer transparency on DWM
    PLFWbool            transparent;
    PLFWbool            scaleToMonitor;
    PLFWbool            keymenu;
    PLFWbool            showDefault;

    // Cached size used to filter out duplicate events
    int                 width, height;

    // The last received cursor position, regardless of source
    int                 lastCursorPosX, lastCursorPosY;
    // The last received high surrogate when decoding pairs of UTF-16 messages
    WCHAR               highSurrogate;
} _PLFWwindowWin32;

// Win32-specific global data
//
typedef struct _PLFWlibraryWin32
{
    HINSTANCE           instance;
    HWND                helperWindowHandle;
    ATOM                helperWindowClass;
    ATOM                mainWindowClass;
    HDEVNOTIFY          deviceNotificationHandle;
    int                 acquiredMonitorCount;
    char*               clipboardString;
    short int           keycodes[512];
    short int           scancodes[PLFW_KEY_LAST + 1];
    char                keynames[PLFW_KEY_LAST + 1][5];
    // Where to place the cursor when re-enabled
    double              restoreCursorPosX, restoreCursorPosY;
    // The window whose disabled cursor mode is active
    _PLFWwindow*        disabledCursorWindow;
    // The window the cursor is captured in
    _PLFWwindow*        capturedCursorWindow;
    RAWINPUT*           rawInput;
    int                 rawInputSize;
    UINT                mouseTrailSize;
    // The cursor handle to use to hide the cursor (NULL or a transparent cursor)
    HCURSOR             blankCursor;

    struct {
        HINSTANCE                       instance;
        PFN_DirectInput8Create          Create;
        IDirectInput8W*                 api;
    } dinput8;

    struct {
        HINSTANCE                       instance;
        PFN_XInputGetCapabilities       GetCapabilities;
        PFN_XInputGetState              GetState;
    } xinput;

    struct {
        HINSTANCE                       instance;
        PFN_EnableNonClientDpiScaling   EnableNonClientDpiScaling_;
        PFN_SetProcessDpiAwarenessContext SetProcessDpiAwarenessContext_;
        PFN_GetDpiForWindow             GetDpiForWindow_;
        PFN_AdjustWindowRectExForDpi    AdjustWindowRectExForDpi_;
        PFN_GetSystemMetricsForDpi      GetSystemMetricsForDpi_;
    } user32;

    struct {
        HINSTANCE                       instance;
        PFN_DwmIsCompositionEnabled     IsCompositionEnabled;
        PFN_DwmFlush                    Flush;
        PFN_DwmEnableBlurBehindWindow   EnableBlurBehindWindow;
        PFN_DwmGetColorizationColor     GetColorizationColor;
    } dwmapi;

    struct {
        HINSTANCE                       instance;
        PFN_SetProcessDpiAwareness      SetProcessDpiAwareness_;
        PFN_GetDpiForMonitor            GetDpiForMonitor_;
    } shcore;

    struct {
        HINSTANCE                       instance;
        PFN_RtlVerifyVersionInfo        RtlVerifyVersionInfo_;
    } ntdll;
} _PLFWlibraryWin32;

// Win32-specific per-monitor data
//
typedef struct _PLFWmonitorWin32
{
    HMONITOR            handle;
    // This size matches the static size of DISPLAY_DEVICE.DeviceName
    WCHAR               adapterName[32];
    WCHAR               displayName[32];
    char                publicAdapterName[32];
    char                publicDisplayName[32];
    PLFWbool            modesPruned;
    PLFWbool            modeChanged;
} _PLFWmonitorWin32;

// Win32-specific per-cursor data
//
typedef struct _PLFWcursorWin32
{
    HCURSOR             handle;
} _PLFWcursorWin32;


PLFWbool _plfwConnectWin32(int platformID, _PLFWplatform* platform);
int _plfwInitWin32(void);
void _plfwTerminateWin32(void);

WCHAR* _plfwCreateWideStringFromUTF8Win32(const char* source);
char* _plfwCreateUTF8FromWideStringWin32(const WCHAR* source);
BOOL _plfwIsWindowsVersionOrGreaterWin32(WORD major, WORD minor, WORD sp);
BOOL _plfwIsWindows10BuildOrGreaterWin32(WORD build);
void _plfwInputErrorWin32(int error, const char* description);
void _plfwUpdateKeyNamesWin32(void);

void _plfwPollMonitorsWin32(void);
void _plfwSetVideoModeWin32(_PLFWmonitor* monitor, const PLFWvidmode* desired);
void _plfwRestoreVideoModeWin32(_PLFWmonitor* monitor);
void _plfwGetHMONITORContentScaleWin32(HMONITOR handle, float* xscale, float* yscale);

PLFWbool _plfwCreateWindowWin32(_PLFWwindow* window, const _PLFWwndconfig* wndconfig, const _PLFWfbconfig* fbconfig);
void _plfwDestroyWindowWin32(_PLFWwindow* window);
void _plfwSetWindowTitleWin32(_PLFWwindow* window, const char* title);
void _plfwSetWindowIconWin32(_PLFWwindow* window, int count, const PLFWimage* images);
void _plfwGetWindowPosWin32(_PLFWwindow* window, int* xpos, int* ypos);
void _plfwSetWindowPosWin32(_PLFWwindow* window, int xpos, int ypos);
void _plfwGetWindowSizeWin32(_PLFWwindow* window, int* width, int* height);
void _plfwSetWindowSizeWin32(_PLFWwindow* window, int width, int height);
void _plfwSetWindowSizeLimitsWin32(_PLFWwindow* window, int minwidth, int minheight, int maxwidth, int maxheight);
void _plfwSetWindowAspectRatioWin32(_PLFWwindow* window, int numer, int denom);
void _plfwGetFramebufferSizeWin32(_PLFWwindow* window, int* width, int* height);
void _plfwGetWindowFrameSizeWin32(_PLFWwindow* window, int* left, int* top, int* right, int* bottom);
void _plfwGetWindowContentScaleWin32(_PLFWwindow* window, float* xscale, float* yscale);
void _plfwIconifyWindowWin32(_PLFWwindow* window);
void _plfwRestoreWindowWin32(_PLFWwindow* window);
void _plfwMaximizeWindowWin32(_PLFWwindow* window);
void _plfwShowWindowWin32(_PLFWwindow* window);
void _plfwHideWindowWin32(_PLFWwindow* window);
void _plfwRequestWindowAttentionWin32(_PLFWwindow* window);
void _plfwFocusWindowWin32(_PLFWwindow* window);
void _plfwSetWindowMonitorWin32(_PLFWwindow* window, _PLFWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate);
PLFWbool _plfwWindowFocusedWin32(_PLFWwindow* window);
PLFWbool _plfwWindowIconifiedWin32(_PLFWwindow* window);
PLFWbool _plfwWindowVisibleWin32(_PLFWwindow* window);
PLFWbool _plfwWindowMaximizedWin32(_PLFWwindow* window);
PLFWbool _plfwWindowHoveredWin32(_PLFWwindow* window);
PLFWbool _plfwFramebufferTransparentWin32(_PLFWwindow* window);
void _plfwSetWindowResizableWin32(_PLFWwindow* window, PLFWbool enabled);
void _plfwSetWindowDecoratedWin32(_PLFWwindow* window, PLFWbool enabled);
void _plfwSetWindowFloatingWin32(_PLFWwindow* window, PLFWbool enabled);
void _plfwSetWindowMousePassthroughWin32(_PLFWwindow* window, PLFWbool enabled);
float _plfwGetWindowOpacityWin32(_PLFWwindow* window);
void _plfwSetWindowOpacityWin32(_PLFWwindow* window, float opacity);

void _plfwSetRawMouseMotionWin32(_PLFWwindow *window, PLFWbool enabled);
PLFWbool _plfwRawMouseMotionSupportedWin32(void);

void _plfwPollEventsWin32(void);
void _plfwWaitEventsWin32(void);
void _plfwWaitEventsTimeoutWin32(double timeout);
void _plfwPostEmptyEventWin32(void);

void _plfwGetCursorPosWin32(_PLFWwindow* window, double* xpos, double* ypos);
void _plfwSetCursorPosWin32(_PLFWwindow* window, double xpos, double ypos);
void _plfwSetCursorModeWin32(_PLFWwindow* window, int mode);
const char* _plfwGetScancodeNameWin32(int scancode);
int _plfwGetKeyScancodeWin32(int key);
PLFWbool _plfwCreateCursorWin32(_PLFWcursor* cursor, const PLFWimage* image, int xhot, int yhot);
PLFWbool _plfwCreateStandardCursorWin32(_PLFWcursor* cursor, int shape);
void _plfwDestroyCursorWin32(_PLFWcursor* cursor);
void _plfwSetCursorWin32(_PLFWwindow* window, _PLFWcursor* cursor);
void _plfwSetClipboardStringWin32(const char* string);
const char* _plfwGetClipboardStringWin32(void);

void _plfwFreeMonitorWin32(_PLFWmonitor* monitor);
void _plfwGetMonitorPosWin32(_PLFWmonitor* monitor, int* xpos, int* ypos);
void _plfwGetMonitorContentScaleWin32(_PLFWmonitor* monitor, float* xscale, float* yscale);
void _plfwGetMonitorWorkareaWin32(_PLFWmonitor* monitor, int* xpos, int* ypos, int* width, int* height);
PLFWvidmode* _plfwGetVideoModesWin32(_PLFWmonitor* monitor, int* count);
PLFWbool _plfwGetVideoModeWin32(_PLFWmonitor* monitor, PLFWvidmode* mode);
PLFWbool _plfwGetGammaRampWin32(_PLFWmonitor* monitor, PLFWgammaramp* ramp);
void _plfwSetGammaRampWin32(_PLFWmonitor* monitor, const PLFWgammaramp* ramp);

PLFWbool _plfwInitJoysticksWin32(void);
void _plfwTerminateJoysticksWin32(void);
PLFWbool _plfwPollJoystickWin32(_PLFWjoystick* js, int mode);
const char* _plfwGetMappingNameWin32(void);
void _plfwUpdateGamepadGUIDWin32(char* guid);

