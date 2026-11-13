//========================================================================
// PLFW 3.5 Wayland - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2014 Jonas Ådahl <jadahl@gmail.com>
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

#if defined(_PLFW_WAYLAND)

#include <errno.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "wayland-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"

// NOTE: Versions of wayland-scanner prior to 1.17.91 named every global array of
//       wl_interface pointers 'types', making it impossible to combine several unmodified
//       private-code files into a single compilation unit
// HACK: We override this name with a macro for each file, allowing them to coexist

#define types _plfw_wayland_types
#include "wayland-client-protocol-code.h"
#undef types

#define types _plfw_xdg_shell_types
#include "xdg-shell-client-protocol-code.h"
#undef types

#define types _plfw_xdg_decoration_types
#include "xdg-decoration-unstable-v1-client-protocol-code.h"
#undef types

#define types _plfw_viewporter_types
#include "viewporter-client-protocol-code.h"
#undef types

#define types _plfw_relative_pointer_types
#include "relative-pointer-unstable-v1-client-protocol-code.h"
#undef types

#define types _plfw_pointer_constraints_types
#include "pointer-constraints-unstable-v1-client-protocol-code.h"
#undef types

#define types _plfw_fractional_scale_types
#include "fractional-scale-v1-client-protocol-code.h"
#undef types

#define types _plfw_xdg_activation_types
#include "xdg-activation-v1-client-protocol-code.h"
#undef types

#define types _plfw_idle_inhibit_types
#include "idle-inhibit-unstable-v1-client-protocol-code.h"
#undef types

static void wmBaseHandlePing(void* userData,
                             struct xdg_wm_base* wmBase,
                             uint32_t serial)
{
    xdg_wm_base_pong(wmBase, serial);
}

static const struct xdg_wm_base_listener wmBaseListener =
{
    wmBaseHandlePing
};

static void registryHandleGlobal(void* userData,
                                 struct wl_registry* registry,
                                 uint32_t name,
                                 const char* interface,
                                 uint32_t version)
{
    if (strcmp(interface, "wl_compositor") == 0)
    {
        _plfw.wl.compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface,
                             _plfw_min(3, version));
    }
    else if (strcmp(interface, "wl_subcompositor") == 0)
    {
        _plfw.wl.subcompositor =
            wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        _plfw.wl.shm =
            wl_registry_bind(registry, name, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, "wl_output") == 0)
    {
        _plfwAddOutputWayland(name, version);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        if (!_plfw.wl.seat)
        {
            _plfw.wl.seat =
                wl_registry_bind(registry, name, &wl_seat_interface,
                                 _plfw_min(8, version));
            _plfwAddSeatListenerWayland(_plfw.wl.seat);
        }
    }
    else if (strcmp(interface, "wl_data_device_manager") == 0)
    {
        if (!_plfw.wl.dataDeviceManager)
        {
            _plfw.wl.dataDeviceManager =
                wl_registry_bind(registry, name,
                                 &wl_data_device_manager_interface, 1);
        }
    }
    else if (strcmp(interface, "xdg_wm_base") == 0)
    {
        _plfw.wl.wmBase =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(_plfw.wl.wmBase, &wmBaseListener, NULL);
    }
    else if (strcmp(interface, "zxdg_decoration_manager_v1") == 0)
    {
        _plfw.wl.decorationManager =
            wl_registry_bind(registry, name,
                             &zxdg_decoration_manager_v1_interface,
                             1);
    }
    else if (strcmp(interface, "wp_viewporter") == 0)
    {
        _plfw.wl.viewporter =
            wl_registry_bind(registry, name, &wp_viewporter_interface, 1);
    }
    else if (strcmp(interface, "zwp_relative_pointer_manager_v1") == 0)
    {
        _plfw.wl.relativePointerManager =
            wl_registry_bind(registry, name,
                             &zwp_relative_pointer_manager_v1_interface,
                             1);
    }
    else if (strcmp(interface, "zwp_pointer_constraints_v1") == 0)
    {
        _plfw.wl.pointerConstraints =
            wl_registry_bind(registry, name,
                             &zwp_pointer_constraints_v1_interface,
                             1);
    }
    else if (strcmp(interface, "zwp_idle_inhibit_manager_v1") == 0)
    {
        _plfw.wl.idleInhibitManager =
            wl_registry_bind(registry, name,
                             &zwp_idle_inhibit_manager_v1_interface,
                             1);
    }
    else if (strcmp(interface, "xdg_activation_v1") == 0)
    {
        _plfw.wl.activationManager =
            wl_registry_bind(registry, name,
                             &xdg_activation_v1_interface,
                             1);
    }
    else if (strcmp(interface, "wp_fractional_scale_manager_v1") == 0)
    {
        _plfw.wl.fractionalScaleManager =
            wl_registry_bind(registry, name,
                             &wp_fractional_scale_manager_v1_interface,
                             1);
    }
}

static void registryHandleGlobalRemove(void* userData,
                                       struct wl_registry* registry,
                                       uint32_t name)
{
    for (int i = 0; i < _plfw.monitorCount; ++i)
    {
        _PLFWmonitor* monitor = _plfw.monitors[i];
        if (monitor->wl.name == name)
        {
            _plfwInputMonitor(monitor, PLFW_DISCONNECTED, 0);
            return;
        }
    }
}


static const struct wl_registry_listener registryListener =
{
    registryHandleGlobal,
    registryHandleGlobalRemove
};

void libdecorHandleError(struct libdecor* context,
                         enum libdecor_error error,
                         const char* message)
{
    _plfwInputError(PLFW_PLATFORM_ERROR,
                    "Wayland: libdecor error %u: %s",
                    error, message);
}

static const struct libdecor_interface libdecorInterface =
{
    libdecorHandleError
};

static void libdecorReadyCallback(void* userData,
                                  struct wl_callback* callback,
                                  uint32_t time)
{
    _plfw.wl.libdecor.ready = PLFW_TRUE;
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener libdecorReadyListener =
{
    libdecorReadyCallback
};

// Create key code translation tables
//
static void createKeyTables(void)
{
    memset(_plfw.wl.keycodes, -1, sizeof(_plfw.wl.keycodes));
    memset(_plfw.wl.scancodes, -1, sizeof(_plfw.wl.scancodes));

    _plfw.wl.keycodes[KEY_GRAVE]      = PLFW_KEY_GRAVE_ACCENT;
    _plfw.wl.keycodes[KEY_1]          = PLFW_KEY_1;
    _plfw.wl.keycodes[KEY_2]          = PLFW_KEY_2;
    _plfw.wl.keycodes[KEY_3]          = PLFW_KEY_3;
    _plfw.wl.keycodes[KEY_4]          = PLFW_KEY_4;
    _plfw.wl.keycodes[KEY_5]          = PLFW_KEY_5;
    _plfw.wl.keycodes[KEY_6]          = PLFW_KEY_6;
    _plfw.wl.keycodes[KEY_7]          = PLFW_KEY_7;
    _plfw.wl.keycodes[KEY_8]          = PLFW_KEY_8;
    _plfw.wl.keycodes[KEY_9]          = PLFW_KEY_9;
    _plfw.wl.keycodes[KEY_0]          = PLFW_KEY_0;
    _plfw.wl.keycodes[KEY_SPACE]      = PLFW_KEY_SPACE;
    _plfw.wl.keycodes[KEY_MINUS]      = PLFW_KEY_MINUS;
    _plfw.wl.keycodes[KEY_EQUAL]      = PLFW_KEY_EQUAL;
    _plfw.wl.keycodes[KEY_Q]          = PLFW_KEY_Q;
    _plfw.wl.keycodes[KEY_W]          = PLFW_KEY_W;
    _plfw.wl.keycodes[KEY_E]          = PLFW_KEY_E;
    _plfw.wl.keycodes[KEY_R]          = PLFW_KEY_R;
    _plfw.wl.keycodes[KEY_T]          = PLFW_KEY_T;
    _plfw.wl.keycodes[KEY_Y]          = PLFW_KEY_Y;
    _plfw.wl.keycodes[KEY_U]          = PLFW_KEY_U;
    _plfw.wl.keycodes[KEY_I]          = PLFW_KEY_I;
    _plfw.wl.keycodes[KEY_O]          = PLFW_KEY_O;
    _plfw.wl.keycodes[KEY_P]          = PLFW_KEY_P;
    _plfw.wl.keycodes[KEY_LEFTBRACE]  = PLFW_KEY_LEFT_BRACKET;
    _plfw.wl.keycodes[KEY_RIGHTBRACE] = PLFW_KEY_RIGHT_BRACKET;
    _plfw.wl.keycodes[KEY_A]          = PLFW_KEY_A;
    _plfw.wl.keycodes[KEY_S]          = PLFW_KEY_S;
    _plfw.wl.keycodes[KEY_D]          = PLFW_KEY_D;
    _plfw.wl.keycodes[KEY_F]          = PLFW_KEY_F;
    _plfw.wl.keycodes[KEY_G]          = PLFW_KEY_G;
    _plfw.wl.keycodes[KEY_H]          = PLFW_KEY_H;
    _plfw.wl.keycodes[KEY_J]          = PLFW_KEY_J;
    _plfw.wl.keycodes[KEY_K]          = PLFW_KEY_K;
    _plfw.wl.keycodes[KEY_L]          = PLFW_KEY_L;
    _plfw.wl.keycodes[KEY_SEMICOLON]  = PLFW_KEY_SEMICOLON;
    _plfw.wl.keycodes[KEY_APOSTROPHE] = PLFW_KEY_APOSTROPHE;
    _plfw.wl.keycodes[KEY_Z]          = PLFW_KEY_Z;
    _plfw.wl.keycodes[KEY_X]          = PLFW_KEY_X;
    _plfw.wl.keycodes[KEY_C]          = PLFW_KEY_C;
    _plfw.wl.keycodes[KEY_V]          = PLFW_KEY_V;
    _plfw.wl.keycodes[KEY_B]          = PLFW_KEY_B;
    _plfw.wl.keycodes[KEY_N]          = PLFW_KEY_N;
    _plfw.wl.keycodes[KEY_M]          = PLFW_KEY_M;
    _plfw.wl.keycodes[KEY_COMMA]      = PLFW_KEY_COMMA;
    _plfw.wl.keycodes[KEY_DOT]        = PLFW_KEY_PERIOD;
    _plfw.wl.keycodes[KEY_SLASH]      = PLFW_KEY_SLASH;
    _plfw.wl.keycodes[KEY_BACKSLASH]  = PLFW_KEY_BACKSLASH;
    _plfw.wl.keycodes[KEY_ESC]        = PLFW_KEY_ESCAPE;
    _plfw.wl.keycodes[KEY_TAB]        = PLFW_KEY_TAB;
    _plfw.wl.keycodes[KEY_LEFTSHIFT]  = PLFW_KEY_LEFT_SHIFT;
    _plfw.wl.keycodes[KEY_RIGHTSHIFT] = PLFW_KEY_RIGHT_SHIFT;
    _plfw.wl.keycodes[KEY_LEFTCTRL]   = PLFW_KEY_LEFT_CONTROL;
    _plfw.wl.keycodes[KEY_RIGHTCTRL]  = PLFW_KEY_RIGHT_CONTROL;
    _plfw.wl.keycodes[KEY_LEFTALT]    = PLFW_KEY_LEFT_ALT;
    _plfw.wl.keycodes[KEY_RIGHTALT]   = PLFW_KEY_RIGHT_ALT;
    _plfw.wl.keycodes[KEY_LEFTMETA]   = PLFW_KEY_LEFT_SUPER;
    _plfw.wl.keycodes[KEY_RIGHTMETA]  = PLFW_KEY_RIGHT_SUPER;
    _plfw.wl.keycodes[KEY_COMPOSE]    = PLFW_KEY_MENU;
    _plfw.wl.keycodes[KEY_NUMLOCK]    = PLFW_KEY_NUM_LOCK;
    _plfw.wl.keycodes[KEY_CAPSLOCK]   = PLFW_KEY_CAPS_LOCK;
    _plfw.wl.keycodes[KEY_PRINT]      = PLFW_KEY_PRINT_SCREEN;
    _plfw.wl.keycodes[KEY_SCROLLLOCK] = PLFW_KEY_SCROLL_LOCK;
    _plfw.wl.keycodes[KEY_PAUSE]      = PLFW_KEY_PAUSE;
    _plfw.wl.keycodes[KEY_DELETE]     = PLFW_KEY_DELETE;
    _plfw.wl.keycodes[KEY_BACKSPACE]  = PLFW_KEY_BACKSPACE;
    _plfw.wl.keycodes[KEY_ENTER]      = PLFW_KEY_ENTER;
    _plfw.wl.keycodes[KEY_HOME]       = PLFW_KEY_HOME;
    _plfw.wl.keycodes[KEY_END]        = PLFW_KEY_END;
    _plfw.wl.keycodes[KEY_PAGEUP]     = PLFW_KEY_PAGE_UP;
    _plfw.wl.keycodes[KEY_PAGEDOWN]   = PLFW_KEY_PAGE_DOWN;
    _plfw.wl.keycodes[KEY_INSERT]     = PLFW_KEY_INSERT;
    _plfw.wl.keycodes[KEY_LEFT]       = PLFW_KEY_LEFT;
    _plfw.wl.keycodes[KEY_RIGHT]      = PLFW_KEY_RIGHT;
    _plfw.wl.keycodes[KEY_DOWN]       = PLFW_KEY_DOWN;
    _plfw.wl.keycodes[KEY_UP]         = PLFW_KEY_UP;
    _plfw.wl.keycodes[KEY_F1]         = PLFW_KEY_F1;
    _plfw.wl.keycodes[KEY_F2]         = PLFW_KEY_F2;
    _plfw.wl.keycodes[KEY_F3]         = PLFW_KEY_F3;
    _plfw.wl.keycodes[KEY_F4]         = PLFW_KEY_F4;
    _plfw.wl.keycodes[KEY_F5]         = PLFW_KEY_F5;
    _plfw.wl.keycodes[KEY_F6]         = PLFW_KEY_F6;
    _plfw.wl.keycodes[KEY_F7]         = PLFW_KEY_F7;
    _plfw.wl.keycodes[KEY_F8]         = PLFW_KEY_F8;
    _plfw.wl.keycodes[KEY_F9]         = PLFW_KEY_F9;
    _plfw.wl.keycodes[KEY_F10]        = PLFW_KEY_F10;
    _plfw.wl.keycodes[KEY_F11]        = PLFW_KEY_F11;
    _plfw.wl.keycodes[KEY_F12]        = PLFW_KEY_F12;
    _plfw.wl.keycodes[KEY_F13]        = PLFW_KEY_F13;
    _plfw.wl.keycodes[KEY_F14]        = PLFW_KEY_F14;
    _plfw.wl.keycodes[KEY_F15]        = PLFW_KEY_F15;
    _plfw.wl.keycodes[KEY_F16]        = PLFW_KEY_F16;
    _plfw.wl.keycodes[KEY_F17]        = PLFW_KEY_F17;
    _plfw.wl.keycodes[KEY_F18]        = PLFW_KEY_F18;
    _plfw.wl.keycodes[KEY_F19]        = PLFW_KEY_F19;
    _plfw.wl.keycodes[KEY_F20]        = PLFW_KEY_F20;
    _plfw.wl.keycodes[KEY_F21]        = PLFW_KEY_F21;
    _plfw.wl.keycodes[KEY_F22]        = PLFW_KEY_F22;
    _plfw.wl.keycodes[KEY_F23]        = PLFW_KEY_F23;
    _plfw.wl.keycodes[KEY_F24]        = PLFW_KEY_F24;
    _plfw.wl.keycodes[KEY_KPSLASH]    = PLFW_KEY_KP_DIVIDE;
    _plfw.wl.keycodes[KEY_KPASTERISK] = PLFW_KEY_KP_MULTIPLY;
    _plfw.wl.keycodes[KEY_KPMINUS]    = PLFW_KEY_KP_SUBTRACT;
    _plfw.wl.keycodes[KEY_KPPLUS]     = PLFW_KEY_KP_ADD;
    _plfw.wl.keycodes[KEY_KP0]        = PLFW_KEY_KP_0;
    _plfw.wl.keycodes[KEY_KP1]        = PLFW_KEY_KP_1;
    _plfw.wl.keycodes[KEY_KP2]        = PLFW_KEY_KP_2;
    _plfw.wl.keycodes[KEY_KP3]        = PLFW_KEY_KP_3;
    _plfw.wl.keycodes[KEY_KP4]        = PLFW_KEY_KP_4;
    _plfw.wl.keycodes[KEY_KP5]        = PLFW_KEY_KP_5;
    _plfw.wl.keycodes[KEY_KP6]        = PLFW_KEY_KP_6;
    _plfw.wl.keycodes[KEY_KP7]        = PLFW_KEY_KP_7;
    _plfw.wl.keycodes[KEY_KP8]        = PLFW_KEY_KP_8;
    _plfw.wl.keycodes[KEY_KP9]        = PLFW_KEY_KP_9;
    _plfw.wl.keycodes[KEY_KPDOT]      = PLFW_KEY_KP_DECIMAL;
    _plfw.wl.keycodes[KEY_KPEQUAL]    = PLFW_KEY_KP_EQUAL;
    _plfw.wl.keycodes[KEY_KPENTER]    = PLFW_KEY_KP_ENTER;
    _plfw.wl.keycodes[KEY_102ND]      = PLFW_KEY_WORLD_2;

    for (int scancode = 0;  scancode < 256;  scancode++)
    {
        if (_plfw.wl.keycodes[scancode] > 0)
            _plfw.wl.scancodes[_plfw.wl.keycodes[scancode]] = scancode;
    }
}

static PLFWbool loadCursorTheme(void)
{
    int cursorSize = 16;

    const char* sizeString = getenv("XCURSOR_SIZE");
    if (sizeString)
    {
        errno = 0;
        const long cursorSizeLong = strtol(sizeString, NULL, 10);
        if (errno == 0 && cursorSizeLong > 0 && cursorSizeLong < INT_MAX)
            cursorSize = (int) cursorSizeLong;
    }

    const char* themeName = getenv("XCURSOR_THEME");

    _plfw.wl.cursorTheme = wl_cursor_theme_load(themeName, cursorSize, _plfw.wl.shm);
    if (!_plfw.wl.cursorTheme)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to load default cursor theme");
        return PLFW_FALSE;
    }

    // If this happens to be NULL, we just fallback to the scale=1 version.
    _plfw.wl.cursorThemeHiDPI =
        wl_cursor_theme_load(themeName, cursorSize * 2, _plfw.wl.shm);

    _plfw.wl.cursorSurface = wl_compositor_create_surface(_plfw.wl.compositor);
    _plfw.wl.cursorTimerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    return PLFW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwConnectWayland(int platformID, _PLFWplatform* platform)
{
    const _PLFWplatform wayland =
    {
        .platformID = PLFW_PLATFORM_WAYLAND,
        .init = _plfwInitWayland,
        .terminate = _plfwTerminateWayland,
        .getCursorPos = _plfwGetCursorPosWayland,
        .setCursorPos = _plfwSetCursorPosWayland,
        .setCursorMode = _plfwSetCursorModeWayland,
        .setRawMouseMotion = _plfwSetRawMouseMotionWayland,
        .rawMouseMotionSupported = _plfwRawMouseMotionSupportedWayland,
        .createCursor = _plfwCreateCursorWayland,
        .createStandardCursor = _plfwCreateStandardCursorWayland,
        .destroyCursor = _plfwDestroyCursorWayland,
        .setCursor = _plfwSetCursorWayland,
        .getScancodeName = _plfwGetScancodeNameWayland,
        .getKeyScancode = _plfwGetKeyScancodeWayland,
        .setClipboardString = _plfwSetClipboardStringWayland,
        .getClipboardString = _plfwGetClipboardStringWayland,
#if defined(PLFW_BUILD_LINUX_JOYSTICK)
        .initJoysticks = _plfwInitJoysticksLinux,
        .terminateJoysticks = _plfwTerminateJoysticksLinux,
        .pollJoystick = _plfwPollJoystickLinux,
        .getMappingName = _plfwGetMappingNameLinux,
        .updateGamepadGUID = _plfwUpdateGamepadGUIDLinux,
#else
        .initJoysticks = _plfwInitJoysticksNull,
        .terminateJoysticks = _plfwTerminateJoysticksNull,
        .pollJoystick = _plfwPollJoystickNull,
        .getMappingName = _plfwGetMappingNameNull,
        .updateGamepadGUID = _plfwUpdateGamepadGUIDNull,
#endif
        .freeMonitor = _plfwFreeMonitorWayland,
        .getMonitorPos = _plfwGetMonitorPosWayland,
        .getMonitorContentScale = _plfwGetMonitorContentScaleWayland,
        .getMonitorWorkarea = _plfwGetMonitorWorkareaWayland,
        .getVideoModes = _plfwGetVideoModesWayland,
        .getVideoMode = _plfwGetVideoModeWayland,
        .getGammaRamp = _plfwGetGammaRampWayland,
        .setGammaRamp = _plfwSetGammaRampWayland,
        .createWindow = _plfwCreateWindowWayland,
        .destroyWindow = _plfwDestroyWindowWayland,
        .setWindowTitle = _plfwSetWindowTitleWayland,
        .setWindowIcon = _plfwSetWindowIconWayland,
        .getWindowPos = _plfwGetWindowPosWayland,
        .setWindowPos = _plfwSetWindowPosWayland,
        .getWindowSize = _plfwGetWindowSizeWayland,
        .setWindowSize = _plfwSetWindowSizeWayland,
        .setWindowSizeLimits = _plfwSetWindowSizeLimitsWayland,
        .setWindowAspectRatio = _plfwSetWindowAspectRatioWayland,
        .getFramebufferSize = _plfwGetFramebufferSizeWayland,
        .getWindowFrameSize = _plfwGetWindowFrameSizeWayland,
        .getWindowContentScale = _plfwGetWindowContentScaleWayland,
        .iconifyWindow = _plfwIconifyWindowWayland,
        .restoreWindow = _plfwRestoreWindowWayland,
        .maximizeWindow = _plfwMaximizeWindowWayland,
        .showWindow = _plfwShowWindowWayland,
        .hideWindow = _plfwHideWindowWayland,
        .requestWindowAttention = _plfwRequestWindowAttentionWayland,
        .focusWindow = _plfwFocusWindowWayland,
        .setWindowMonitor = _plfwSetWindowMonitorWayland,
        .windowFocused = _plfwWindowFocusedWayland,
        .windowIconified = _plfwWindowIconifiedWayland,
        .windowVisible = _plfwWindowVisibleWayland,
        .windowMaximized = _plfwWindowMaximizedWayland,
        .windowHovered = _plfwWindowHoveredWayland,
        .getWindowOpacity = _plfwGetWindowOpacityWayland,
        .setWindowResizable = _plfwSetWindowResizableWayland,
        .setWindowDecorated = _plfwSetWindowDecoratedWayland,
        .setWindowFloating = _plfwSetWindowFloatingWayland,
        .setWindowOpacity = _plfwSetWindowOpacityWayland,
        .setWindowMousePassthrough = _plfwSetWindowMousePassthroughWayland,
        .pollEvents = _plfwPollEventsWayland,
        .waitEvents = _plfwWaitEventsWayland,
        .waitEventsTimeout = _plfwWaitEventsTimeoutWayland,
        .postEmptyEvent = _plfwPostEmptyEventWayland
    };

    void* module = _plfwPlatformLoadModule("libwayland-client.so.0");
    if (!module)
    {
        if (platformID == PLFW_PLATFORM_WAYLAND)
        {
            _plfwInputError(PLFW_PLATFORM_ERROR,
                            "Wayland: Failed to load libwayland-client");
        }

        return PLFW_FALSE;
    }

    PFN_wl_display_connect wl_display_connect = (PFN_wl_display_connect)
        _plfwPlatformGetModuleSymbol(module, "wl_display_connect");
    if (!wl_display_connect)
    {
        if (platformID == PLFW_PLATFORM_WAYLAND)
        {
            _plfwInputError(PLFW_PLATFORM_ERROR,
                            "Wayland: Failed to load libwayland-client entry point");
        }

        _plfwPlatformFreeModule(module);
        return PLFW_FALSE;
    }

    struct wl_display* display = wl_display_connect(NULL);
    if (!display)
    {
        if (platformID == PLFW_PLATFORM_WAYLAND)
            _plfwInputError(PLFW_PLATFORM_ERROR, "Wayland: Failed to connect to display");

        _plfwPlatformFreeModule(module);
        return PLFW_FALSE;
    }

    _plfw.wl.display = display;
    _plfw.wl.client.handle = module;

    *platform = wayland;
    return PLFW_TRUE;
}

int _plfwInitWayland(void)
{
    // These must be set before any failure checks
    _plfw.wl.keyRepeatTimerfd = -1;
    _plfw.wl.cursorTimerfd = -1;

    // libwayland identifies the client library that created a proxy by comparing
// this tag's address, so any process-wide static string works here
static const char* const tag = "PLFW window/input only";
_plfw.wl.tag = tag;

    _plfw.wl.client.display_flush = (PFN_wl_display_flush)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_flush");
    _plfw.wl.client.display_cancel_read = (PFN_wl_display_cancel_read)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_cancel_read");
    _plfw.wl.client.display_dispatch_pending = (PFN_wl_display_dispatch_pending)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_dispatch_pending");
    _plfw.wl.client.display_read_events = (PFN_wl_display_read_events)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_read_events");
    _plfw.wl.client.display_disconnect = (PFN_wl_display_disconnect)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_disconnect");
    _plfw.wl.client.display_roundtrip = (PFN_wl_display_roundtrip)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_roundtrip");
    _plfw.wl.client.display_get_fd = (PFN_wl_display_get_fd)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_get_fd");
    _plfw.wl.client.display_prepare_read = (PFN_wl_display_prepare_read)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_prepare_read");
    _plfw.wl.client.display_create_queue = (PFN_wl_display_create_queue)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_create_queue");
    _plfw.wl.client.display_prepare_read_queue = (PFN_wl_display_prepare_read_queue)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_prepare_read_queue");
    _plfw.wl.client.display_dispatch_queue_pending = (PFN_wl_display_dispatch_queue_pending)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_display_dispatch_queue_pending");
    _plfw.wl.client.event_queue_destroy = (PFN_wl_event_queue_destroy)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_event_queue_destroy");
    _plfw.wl.client.proxy_marshal = (PFN_wl_proxy_marshal)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_marshal");
    _plfw.wl.client.proxy_add_listener = (PFN_wl_proxy_add_listener)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_add_listener");
    _plfw.wl.client.proxy_destroy = (PFN_wl_proxy_destroy)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_destroy");
    _plfw.wl.client.proxy_marshal_constructor = (PFN_wl_proxy_marshal_constructor)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_marshal_constructor");
    _plfw.wl.client.proxy_marshal_constructor_versioned = (PFN_wl_proxy_marshal_constructor_versioned)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_marshal_constructor_versioned");
    _plfw.wl.client.proxy_get_user_data = (PFN_wl_proxy_get_user_data)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_get_user_data");
    _plfw.wl.client.proxy_set_user_data = (PFN_wl_proxy_set_user_data)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_set_user_data");
    _plfw.wl.client.proxy_get_tag = (PFN_wl_proxy_get_tag)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_get_tag");
    _plfw.wl.client.proxy_set_tag = (PFN_wl_proxy_set_tag)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_set_tag");
    _plfw.wl.client.proxy_get_version = (PFN_wl_proxy_get_version)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_get_version");
    _plfw.wl.client.proxy_marshal_flags = (PFN_wl_proxy_marshal_flags)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_marshal_flags");
    _plfw.wl.client.proxy_create_wrapper = (PFN_wl_proxy_create_wrapper)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_create_wrapper");
    _plfw.wl.client.proxy_wrapper_destroy = (PFN_wl_proxy_wrapper_destroy)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_wrapper_destroy");
    _plfw.wl.client.proxy_set_queue = (PFN_wl_proxy_set_queue)
        _plfwPlatformGetModuleSymbol(_plfw.wl.client.handle, "wl_proxy_set_queue");

    if (!_plfw.wl.client.display_flush ||
        !_plfw.wl.client.display_cancel_read ||
        !_plfw.wl.client.display_dispatch_pending ||
        !_plfw.wl.client.display_read_events ||
        !_plfw.wl.client.display_disconnect ||
        !_plfw.wl.client.display_roundtrip ||
        !_plfw.wl.client.display_get_fd ||
        !_plfw.wl.client.display_prepare_read ||
        !_plfw.wl.client.display_create_queue ||
        !_plfw.wl.client.display_prepare_read_queue ||
        !_plfw.wl.client.display_dispatch_queue_pending ||
        !_plfw.wl.client.event_queue_destroy ||
        !_plfw.wl.client.proxy_marshal ||
        !_plfw.wl.client.proxy_add_listener ||
        !_plfw.wl.client.proxy_destroy ||
        !_plfw.wl.client.proxy_marshal_constructor ||
        !_plfw.wl.client.proxy_marshal_constructor_versioned ||
        !_plfw.wl.client.proxy_get_user_data ||
        !_plfw.wl.client.proxy_set_user_data ||
        !_plfw.wl.client.proxy_get_tag ||
        !_plfw.wl.client.proxy_set_tag ||
        !_plfw.wl.client.proxy_create_wrapper ||
        !_plfw.wl.client.proxy_wrapper_destroy ||
        !_plfw.wl.client.proxy_set_queue)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to load libwayland-client entry point");
        return PLFW_FALSE;
    }

    _plfw.wl.cursor.handle = _plfwPlatformLoadModule("libwayland-cursor.so.0");
    if (!_plfw.wl.cursor.handle)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to load libwayland-cursor");
        return PLFW_FALSE;
    }

    _plfw.wl.cursor.theme_load = (PFN_wl_cursor_theme_load)
        _plfwPlatformGetModuleSymbol(_plfw.wl.cursor.handle, "wl_cursor_theme_load");
    _plfw.wl.cursor.theme_destroy = (PFN_wl_cursor_theme_destroy)
        _plfwPlatformGetModuleSymbol(_plfw.wl.cursor.handle, "wl_cursor_theme_destroy");
    _plfw.wl.cursor.theme_get_cursor = (PFN_wl_cursor_theme_get_cursor)
        _plfwPlatformGetModuleSymbol(_plfw.wl.cursor.handle, "wl_cursor_theme_get_cursor");
    _plfw.wl.cursor.image_get_buffer = (PFN_wl_cursor_image_get_buffer)
        _plfwPlatformGetModuleSymbol(_plfw.wl.cursor.handle, "wl_cursor_image_get_buffer");

    _plfw.wl.xkb.handle = _plfwPlatformLoadModule("libxkbcommon.so.0");
    if (!_plfw.wl.xkb.handle)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to load libxkbcommon");
        return PLFW_FALSE;
    }

    _plfw.wl.xkb.context_new = (PFN_xkb_context_new)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_context_new");
    _plfw.wl.xkb.context_unref = (PFN_xkb_context_unref)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_context_unref");
    _plfw.wl.xkb.keymap_new_from_string = (PFN_xkb_keymap_new_from_string)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_keymap_new_from_string");
    _plfw.wl.xkb.keymap_unref = (PFN_xkb_keymap_unref)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_keymap_unref");
    _plfw.wl.xkb.keymap_mod_get_index = (PFN_xkb_keymap_mod_get_index)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_keymap_mod_get_index");
    _plfw.wl.xkb.keymap_key_repeats = (PFN_xkb_keymap_key_repeats)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_keymap_key_repeats");
    _plfw.wl.xkb.keymap_key_get_syms_by_level = (PFN_xkb_keymap_key_get_syms_by_level)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_keymap_key_get_syms_by_level");
    _plfw.wl.xkb.state_new = (PFN_xkb_state_new)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_state_new");
    _plfw.wl.xkb.state_unref = (PFN_xkb_state_unref)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_state_unref");
    _plfw.wl.xkb.state_key_get_syms = (PFN_xkb_state_key_get_syms)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_state_key_get_syms");
    _plfw.wl.xkb.state_update_mask = (PFN_xkb_state_update_mask)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_state_update_mask");
    _plfw.wl.xkb.state_key_get_layout = (PFN_xkb_state_key_get_layout)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_state_key_get_layout");
    _plfw.wl.xkb.state_mod_index_is_active = (PFN_xkb_state_mod_index_is_active)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_state_mod_index_is_active");
    _plfw.wl.xkb.compose_table_new_from_locale = (PFN_xkb_compose_table_new_from_locale)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_compose_table_new_from_locale");
    _plfw.wl.xkb.compose_table_unref = (PFN_xkb_compose_table_unref)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_compose_table_unref");
    _plfw.wl.xkb.compose_state_new = (PFN_xkb_compose_state_new)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_compose_state_new");
    _plfw.wl.xkb.compose_state_unref = (PFN_xkb_compose_state_unref)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_compose_state_unref");
    _plfw.wl.xkb.compose_state_feed = (PFN_xkb_compose_state_feed)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_compose_state_feed");
    _plfw.wl.xkb.compose_state_get_status = (PFN_xkb_compose_state_get_status)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_compose_state_get_status");
    _plfw.wl.xkb.compose_state_get_one_sym = (PFN_xkb_compose_state_get_one_sym)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_compose_state_get_one_sym");
    _plfw.wl.xkb.keysym_to_utf32 = (PFN_xkb_keysym_to_utf32)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_keysym_to_utf32");
    _plfw.wl.xkb.keysym_to_utf8 = (PFN_xkb_keysym_to_utf8)
        _plfwPlatformGetModuleSymbol(_plfw.wl.xkb.handle, "xkb_keysym_to_utf8");

    if (!_plfw.wl.xkb.context_new ||
        !_plfw.wl.xkb.context_unref ||
        !_plfw.wl.xkb.keymap_new_from_string ||
        !_plfw.wl.xkb.keymap_unref ||
        !_plfw.wl.xkb.keymap_mod_get_index ||
        !_plfw.wl.xkb.keymap_key_repeats ||
        !_plfw.wl.xkb.keymap_key_get_syms_by_level ||
        !_plfw.wl.xkb.state_new ||
        !_plfw.wl.xkb.state_unref ||
        !_plfw.wl.xkb.state_key_get_syms ||
        !_plfw.wl.xkb.state_update_mask ||
        !_plfw.wl.xkb.state_key_get_layout ||
        !_plfw.wl.xkb.state_mod_index_is_active ||
        !_plfw.wl.xkb.compose_table_new_from_locale ||
        !_plfw.wl.xkb.compose_table_unref ||
        !_plfw.wl.xkb.compose_state_new ||
        !_plfw.wl.xkb.compose_state_unref ||
        !_plfw.wl.xkb.compose_state_feed ||
        !_plfw.wl.xkb.compose_state_get_status ||
        !_plfw.wl.xkb.compose_state_get_one_sym)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to load all entry points from libxkbcommon");
        return PLFW_FALSE;
    }

    if (_plfw.hints.init.wl.libdecorMode == PLFW_WAYLAND_PREFER_LIBDECOR)
        _plfw.wl.libdecor.handle = _plfwPlatformLoadModule("libdecor-0.so.0");

    if (_plfw.wl.libdecor.handle)
    {
        _plfw.wl.libdecor.libdecor_new_ = (PFN_libdecor_new)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_new");
        _plfw.wl.libdecor.libdecor_unref_ = (PFN_libdecor_unref)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_unref");
        _plfw.wl.libdecor.libdecor_get_fd_ = (PFN_libdecor_get_fd)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_get_fd");
        _plfw.wl.libdecor.libdecor_dispatch_ = (PFN_libdecor_dispatch)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_dispatch");
        _plfw.wl.libdecor.libdecor_decorate_ = (PFN_libdecor_decorate)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_decorate");
        _plfw.wl.libdecor.libdecor_frame_unref_ = (PFN_libdecor_frame_unref)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_unref");
        _plfw.wl.libdecor.libdecor_frame_set_app_id_ = (PFN_libdecor_frame_set_app_id)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_app_id");
        _plfw.wl.libdecor.libdecor_frame_set_title_ = (PFN_libdecor_frame_set_title)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_title");
        _plfw.wl.libdecor.libdecor_frame_set_minimized_ = (PFN_libdecor_frame_set_minimized)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_minimized");
        _plfw.wl.libdecor.libdecor_frame_set_fullscreen_ = (PFN_libdecor_frame_set_fullscreen)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_fullscreen");
        _plfw.wl.libdecor.libdecor_frame_unset_fullscreen_ = (PFN_libdecor_frame_unset_fullscreen)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_unset_fullscreen");
        _plfw.wl.libdecor.libdecor_frame_map_ = (PFN_libdecor_frame_map)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_map");
        _plfw.wl.libdecor.libdecor_frame_commit_ = (PFN_libdecor_frame_commit)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_commit");
        _plfw.wl.libdecor.libdecor_frame_set_min_content_size_ = (PFN_libdecor_frame_set_min_content_size)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_min_content_size");
        _plfw.wl.libdecor.libdecor_frame_set_max_content_size_ = (PFN_libdecor_frame_set_max_content_size)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_max_content_size");
        _plfw.wl.libdecor.libdecor_frame_set_maximized_ = (PFN_libdecor_frame_set_maximized)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_maximized");
        _plfw.wl.libdecor.libdecor_frame_unset_maximized_ = (PFN_libdecor_frame_unset_maximized)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_unset_maximized");
        _plfw.wl.libdecor.libdecor_frame_set_capabilities_ = (PFN_libdecor_frame_set_capabilities)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_capabilities");
        _plfw.wl.libdecor.libdecor_frame_unset_capabilities_ = (PFN_libdecor_frame_unset_capabilities)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_unset_capabilities");
        _plfw.wl.libdecor.libdecor_frame_set_visibility_ = (PFN_libdecor_frame_set_visibility)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_set_visibility");
        _plfw.wl.libdecor.libdecor_frame_is_visible_ = (PFN_libdecor_frame_is_visible)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_is_visible");
        _plfw.wl.libdecor.libdecor_frame_get_xdg_toplevel_ = (PFN_libdecor_frame_get_xdg_toplevel)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_frame_get_xdg_toplevel");
        _plfw.wl.libdecor.libdecor_configuration_get_content_size_ = (PFN_libdecor_configuration_get_content_size)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_configuration_get_content_size");
        _plfw.wl.libdecor.libdecor_configuration_get_window_state_ = (PFN_libdecor_configuration_get_window_state)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_configuration_get_window_state");
        _plfw.wl.libdecor.libdecor_state_new_ = (PFN_libdecor_state_new)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_state_new");
        _plfw.wl.libdecor.libdecor_state_free_ = (PFN_libdecor_state_free)
            _plfwPlatformGetModuleSymbol(_plfw.wl.libdecor.handle, "libdecor_state_free");

        if (!_plfw.wl.libdecor.libdecor_new_ ||
            !_plfw.wl.libdecor.libdecor_unref_ ||
            !_plfw.wl.libdecor.libdecor_get_fd_ ||
            !_plfw.wl.libdecor.libdecor_dispatch_ ||
            !_plfw.wl.libdecor.libdecor_decorate_ ||
            !_plfw.wl.libdecor.libdecor_frame_unref_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_app_id_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_title_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_minimized_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_fullscreen_ ||
            !_plfw.wl.libdecor.libdecor_frame_unset_fullscreen_ ||
            !_plfw.wl.libdecor.libdecor_frame_map_ ||
            !_plfw.wl.libdecor.libdecor_frame_commit_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_min_content_size_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_max_content_size_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_maximized_ ||
            !_plfw.wl.libdecor.libdecor_frame_unset_maximized_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_capabilities_ ||
            !_plfw.wl.libdecor.libdecor_frame_unset_capabilities_ ||
            !_plfw.wl.libdecor.libdecor_frame_set_visibility_ ||
            !_plfw.wl.libdecor.libdecor_frame_is_visible_ ||
            !_plfw.wl.libdecor.libdecor_frame_get_xdg_toplevel_ ||
            !_plfw.wl.libdecor.libdecor_configuration_get_content_size_ ||
            !_plfw.wl.libdecor.libdecor_configuration_get_window_state_ ||
            !_plfw.wl.libdecor.libdecor_state_new_ ||
            !_plfw.wl.libdecor.libdecor_state_free_)
        {
            _plfwPlatformFreeModule(_plfw.wl.libdecor.handle);
            memset(&_plfw.wl.libdecor, 0, sizeof(_plfw.wl.libdecor));
        }
    }

    _plfw.wl.registry = wl_display_get_registry(_plfw.wl.display);
    wl_registry_add_listener(_plfw.wl.registry, &registryListener, NULL);

    createKeyTables();

    _plfw.wl.keyRepeatTimerfd =
        timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (_plfw.wl.keyRepeatTimerfd == -1)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create timerfd: %s",
                        strerror(errno));
        return PLFW_FALSE;
    }

    _plfw.wl.xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!_plfw.wl.xkb.context)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to initialize xkb context");
        return PLFW_FALSE;
    }

    // Sync so we got all registry objects
    wl_display_roundtrip(_plfw.wl.display);

    // Sync so we got all initial output events
    wl_display_roundtrip(_plfw.wl.display);

    if (_plfw.wl.libdecor.handle)
    {
        _plfw.wl.libdecor.context = libdecor_new(_plfw.wl.display, &libdecorInterface);
        if (_plfw.wl.libdecor.context)
        {
            // Perform an initial dispatch and flush to get the init started
            libdecor_dispatch(_plfw.wl.libdecor.context, 0);

            // Create sync point to "know" when libdecor is ready for use
            struct wl_callback* callback = wl_display_sync(_plfw.wl.display);
            wl_callback_add_listener(callback, &libdecorReadyListener, NULL);
        }
    }

    if (!_plfw.wl.wmBase)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to find xdg-shell in your compositor");
        return PLFW_FALSE;
    }

    if (!_plfw.wl.shm)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to find wl_shm in your compositor");
        return PLFW_FALSE;
    }

    if (!loadCursorTheme())
        return PLFW_FALSE;

    if (_plfw.wl.seat && _plfw.wl.dataDeviceManager)
    {
        _plfw.wl.dataDevice =
            wl_data_device_manager_get_data_device(_plfw.wl.dataDeviceManager,
                                                   _plfw.wl.seat);
        _plfwAddDataDeviceListenerWayland(_plfw.wl.dataDevice);
    }

    return PLFW_TRUE;
}

void _plfwTerminateWayland(void)
{
    if (_plfw.wl.libdecor.context)
    {
        // Allow libdecor to finish receiving all its requested globals
        // and ensure the associated sync callback object is destroyed
        while (!_plfw.wl.libdecor.ready)
            _plfwWaitEventsWayland();

        libdecor_unref(_plfw.wl.libdecor.context);
    }

    if (_plfw.wl.xkb.composeState)
        xkb_compose_state_unref(_plfw.wl.xkb.composeState);
    if (_plfw.wl.xkb.keymap)
        xkb_keymap_unref(_plfw.wl.xkb.keymap);
    if (_plfw.wl.xkb.state)
        xkb_state_unref(_plfw.wl.xkb.state);
    if (_plfw.wl.xkb.context)
        xkb_context_unref(_plfw.wl.xkb.context);

    if (_plfw.wl.cursorTheme)
        wl_cursor_theme_destroy(_plfw.wl.cursorTheme);
    if (_plfw.wl.cursorThemeHiDPI)
        wl_cursor_theme_destroy(_plfw.wl.cursorThemeHiDPI);

    for (unsigned int i = 0; i < _plfw.wl.offerCount; i++)
        wl_data_offer_destroy(_plfw.wl.offers[i].offer);

    _plfw_free(_plfw.wl.offers);

    if (_plfw.wl.cursorSurface)
        wl_surface_destroy(_plfw.wl.cursorSurface);
    if (_plfw.wl.subcompositor)
        wl_subcompositor_destroy(_plfw.wl.subcompositor);
    if (_plfw.wl.compositor)
        wl_compositor_destroy(_plfw.wl.compositor);
    if (_plfw.wl.shm)
        wl_shm_destroy(_plfw.wl.shm);
    if (_plfw.wl.viewporter)
        wp_viewporter_destroy(_plfw.wl.viewporter);
    if (_plfw.wl.decorationManager)
        zxdg_decoration_manager_v1_destroy(_plfw.wl.decorationManager);
    if (_plfw.wl.wmBase)
        xdg_wm_base_destroy(_plfw.wl.wmBase);
    if (_plfw.wl.selectionOffer)
        wl_data_offer_destroy(_plfw.wl.selectionOffer);
    if (_plfw.wl.dragOffer)
        wl_data_offer_destroy(_plfw.wl.dragOffer);
    if (_plfw.wl.selectionSource)
        wl_data_source_destroy(_plfw.wl.selectionSource);
    if (_plfw.wl.dataDevice)
        wl_data_device_destroy(_plfw.wl.dataDevice);
    if (_plfw.wl.dataDeviceManager)
        wl_data_device_manager_destroy(_plfw.wl.dataDeviceManager);
    if (_plfw.wl.pointer)
        wl_pointer_destroy(_plfw.wl.pointer);
    if (_plfw.wl.keyboard)
        wl_keyboard_destroy(_plfw.wl.keyboard);
    if (_plfw.wl.seat)
        wl_seat_destroy(_plfw.wl.seat);
    if (_plfw.wl.relativePointerManager)
        zwp_relative_pointer_manager_v1_destroy(_plfw.wl.relativePointerManager);
    if (_plfw.wl.pointerConstraints)
        zwp_pointer_constraints_v1_destroy(_plfw.wl.pointerConstraints);
    if (_plfw.wl.idleInhibitManager)
        zwp_idle_inhibit_manager_v1_destroy(_plfw.wl.idleInhibitManager);
    if (_plfw.wl.activationManager)
        xdg_activation_v1_destroy(_plfw.wl.activationManager);
    if (_plfw.wl.fractionalScaleManager)
        wp_fractional_scale_manager_v1_destroy(_plfw.wl.fractionalScaleManager);
    if (_plfw.wl.registry)
        wl_registry_destroy(_plfw.wl.registry);
    if (_plfw.wl.display)
    {
        wl_display_flush(_plfw.wl.display);
        wl_display_disconnect(_plfw.wl.display);
    }

    if (_plfw.wl.keyRepeatTimerfd >= 0)
        close(_plfw.wl.keyRepeatTimerfd);
    if (_plfw.wl.cursorTimerfd >= 0)
        close(_plfw.wl.cursorTimerfd);

    // Free modules only after all Wayland termination functions are called

    _plfwPlatformFreeModule(_plfw.wl.libdecor.handle);
    _plfwPlatformFreeModule(_plfw.wl.xkb.handle);
    _plfwPlatformFreeModule(_plfw.wl.cursor.handle);
    _plfwPlatformFreeModule(_plfw.wl.client.handle);

    _plfw_free(_plfw.wl.clipboardString);

    memset(&_plfw.wl, 0, sizeof(_plfw.wl));
}

#endif // _PLFW_WAYLAND

