//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2016 Google Inc.
// Copyright (c) 2016-2017 Camilla Löwy <elmindreda@glfw.org>
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

#include <stdlib.h>
#include <string.h>


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwConnectNull(int platformID, _PLFWplatform* platform)
{
    const _PLFWplatform null =
    {
        .platformID = PLFW_PLATFORM_NULL,
        .init = _plfwInitNull,
        .terminate = _plfwTerminateNull,
        .getCursorPos = _plfwGetCursorPosNull,
        .setCursorPos = _plfwSetCursorPosNull,
        .setCursorMode = _plfwSetCursorModeNull,
        .setRawMouseMotion = _plfwSetRawMouseMotionNull,
        .rawMouseMotionSupported = _plfwRawMouseMotionSupportedNull,
        .createCursor = _plfwCreateCursorNull,
        .createStandardCursor = _plfwCreateStandardCursorNull,
        .destroyCursor = _plfwDestroyCursorNull,
        .setCursor = _plfwSetCursorNull,
        .getScancodeName = _plfwGetScancodeNameNull,
        .getKeyScancode = _plfwGetKeyScancodeNull,
        .setClipboardString = _plfwSetClipboardStringNull,
        .getClipboardString = _plfwGetClipboardStringNull,
        .initJoysticks = _plfwInitJoysticksNull,
        .terminateJoysticks = _plfwTerminateJoysticksNull,
        .pollJoystick = _plfwPollJoystickNull,
        .getMappingName = _plfwGetMappingNameNull,
        .updateGamepadGUID = _plfwUpdateGamepadGUIDNull,
        .freeMonitor = _plfwFreeMonitorNull,
        .getMonitorPos = _plfwGetMonitorPosNull,
        .getMonitorContentScale = _plfwGetMonitorContentScaleNull,
        .getMonitorWorkarea = _plfwGetMonitorWorkareaNull,
        .getVideoModes = _plfwGetVideoModesNull,
        .getVideoMode = _plfwGetVideoModeNull,
        .getGammaRamp = _plfwGetGammaRampNull,
        .setGammaRamp = _plfwSetGammaRampNull,
        .createWindow = _plfwCreateWindowNull,
        .destroyWindow = _plfwDestroyWindowNull,
        .setWindowTitle = _plfwSetWindowTitleNull,
        .setWindowIcon = _plfwSetWindowIconNull,
        .getWindowPos = _plfwGetWindowPosNull,
        .setWindowPos = _plfwSetWindowPosNull,
        .getWindowSize = _plfwGetWindowSizeNull,
        .setWindowSize = _plfwSetWindowSizeNull,
        .setWindowSizeLimits = _plfwSetWindowSizeLimitsNull,
        .setWindowAspectRatio = _plfwSetWindowAspectRatioNull,
        .getFramebufferSize = _plfwGetFramebufferSizeNull,
        .getWindowFrameSize = _plfwGetWindowFrameSizeNull,
        .getWindowContentScale = _plfwGetWindowContentScaleNull,
        .iconifyWindow = _plfwIconifyWindowNull,
        .restoreWindow = _plfwRestoreWindowNull,
        .maximizeWindow = _plfwMaximizeWindowNull,
        .showWindow = _plfwShowWindowNull,
        .hideWindow = _plfwHideWindowNull,
        .requestWindowAttention = _plfwRequestWindowAttentionNull,
        .focusWindow = _plfwFocusWindowNull,
        .setWindowMonitor = _plfwSetWindowMonitorNull,
        .windowFocused = _plfwWindowFocusedNull,
        .windowIconified = _plfwWindowIconifiedNull,
        .windowVisible = _plfwWindowVisibleNull,
        .windowMaximized = _plfwWindowMaximizedNull,
        .windowHovered = _plfwWindowHoveredNull,
        .framebufferTransparent = _plfwFramebufferTransparentNull,
        .getWindowOpacity = _plfwGetWindowOpacityNull,
        .setWindowResizable = _plfwSetWindowResizableNull,
        .setWindowDecorated = _plfwSetWindowDecoratedNull,
        .setWindowFloating = _plfwSetWindowFloatingNull,
        .setWindowOpacity = _plfwSetWindowOpacityNull,
        .setWindowMousePassthrough = _plfwSetWindowMousePassthroughNull,
        .pollEvents = _plfwPollEventsNull,
        .waitEvents = _plfwWaitEventsNull,
        .waitEventsTimeout = _plfwWaitEventsTimeoutNull,
        .postEmptyEvent = _plfwPostEmptyEventNull
    };

    *platform = null;
    return PLFW_TRUE;
}

int _plfwInitNull(void)
{
    int scancode;

    memset(_plfw.null.keycodes, -1, sizeof(_plfw.null.keycodes));
    memset(_plfw.null.scancodes, -1, sizeof(_plfw.null.scancodes));

    _plfw.null.keycodes[PLFW_NULL_SC_SPACE]         = PLFW_KEY_SPACE;
    _plfw.null.keycodes[PLFW_NULL_SC_APOSTROPHE]    = PLFW_KEY_APOSTROPHE;
    _plfw.null.keycodes[PLFW_NULL_SC_COMMA]         = PLFW_KEY_COMMA;
    _plfw.null.keycodes[PLFW_NULL_SC_MINUS]         = PLFW_KEY_MINUS;
    _plfw.null.keycodes[PLFW_NULL_SC_PERIOD]        = PLFW_KEY_PERIOD;
    _plfw.null.keycodes[PLFW_NULL_SC_SLASH]         = PLFW_KEY_SLASH;
    _plfw.null.keycodes[PLFW_NULL_SC_0]             = PLFW_KEY_0;
    _plfw.null.keycodes[PLFW_NULL_SC_1]             = PLFW_KEY_1;
    _plfw.null.keycodes[PLFW_NULL_SC_2]             = PLFW_KEY_2;
    _plfw.null.keycodes[PLFW_NULL_SC_3]             = PLFW_KEY_3;
    _plfw.null.keycodes[PLFW_NULL_SC_4]             = PLFW_KEY_4;
    _plfw.null.keycodes[PLFW_NULL_SC_5]             = PLFW_KEY_5;
    _plfw.null.keycodes[PLFW_NULL_SC_6]             = PLFW_KEY_6;
    _plfw.null.keycodes[PLFW_NULL_SC_7]             = PLFW_KEY_7;
    _plfw.null.keycodes[PLFW_NULL_SC_8]             = PLFW_KEY_8;
    _plfw.null.keycodes[PLFW_NULL_SC_9]             = PLFW_KEY_9;
    _plfw.null.keycodes[PLFW_NULL_SC_SEMICOLON]     = PLFW_KEY_SEMICOLON;
    _plfw.null.keycodes[PLFW_NULL_SC_EQUAL]         = PLFW_KEY_EQUAL;
    _plfw.null.keycodes[PLFW_NULL_SC_A]             = PLFW_KEY_A;
    _plfw.null.keycodes[PLFW_NULL_SC_B]             = PLFW_KEY_B;
    _plfw.null.keycodes[PLFW_NULL_SC_C]             = PLFW_KEY_C;
    _plfw.null.keycodes[PLFW_NULL_SC_D]             = PLFW_KEY_D;
    _plfw.null.keycodes[PLFW_NULL_SC_E]             = PLFW_KEY_E;
    _plfw.null.keycodes[PLFW_NULL_SC_F]             = PLFW_KEY_F;
    _plfw.null.keycodes[PLFW_NULL_SC_G]             = PLFW_KEY_G;
    _plfw.null.keycodes[PLFW_NULL_SC_H]             = PLFW_KEY_H;
    _plfw.null.keycodes[PLFW_NULL_SC_I]             = PLFW_KEY_I;
    _plfw.null.keycodes[PLFW_NULL_SC_J]             = PLFW_KEY_J;
    _plfw.null.keycodes[PLFW_NULL_SC_K]             = PLFW_KEY_K;
    _plfw.null.keycodes[PLFW_NULL_SC_L]             = PLFW_KEY_L;
    _plfw.null.keycodes[PLFW_NULL_SC_M]             = PLFW_KEY_M;
    _plfw.null.keycodes[PLFW_NULL_SC_N]             = PLFW_KEY_N;
    _plfw.null.keycodes[PLFW_NULL_SC_O]             = PLFW_KEY_O;
    _plfw.null.keycodes[PLFW_NULL_SC_P]             = PLFW_KEY_P;
    _plfw.null.keycodes[PLFW_NULL_SC_Q]             = PLFW_KEY_Q;
    _plfw.null.keycodes[PLFW_NULL_SC_R]             = PLFW_KEY_R;
    _plfw.null.keycodes[PLFW_NULL_SC_S]             = PLFW_KEY_S;
    _plfw.null.keycodes[PLFW_NULL_SC_T]             = PLFW_KEY_T;
    _plfw.null.keycodes[PLFW_NULL_SC_U]             = PLFW_KEY_U;
    _plfw.null.keycodes[PLFW_NULL_SC_V]             = PLFW_KEY_V;
    _plfw.null.keycodes[PLFW_NULL_SC_W]             = PLFW_KEY_W;
    _plfw.null.keycodes[PLFW_NULL_SC_X]             = PLFW_KEY_X;
    _plfw.null.keycodes[PLFW_NULL_SC_Y]             = PLFW_KEY_Y;
    _plfw.null.keycodes[PLFW_NULL_SC_Z]             = PLFW_KEY_Z;
    _plfw.null.keycodes[PLFW_NULL_SC_LEFT_BRACKET]  = PLFW_KEY_LEFT_BRACKET;
    _plfw.null.keycodes[PLFW_NULL_SC_BACKSLASH]     = PLFW_KEY_BACKSLASH;
    _plfw.null.keycodes[PLFW_NULL_SC_RIGHT_BRACKET] = PLFW_KEY_RIGHT_BRACKET;
    _plfw.null.keycodes[PLFW_NULL_SC_GRAVE_ACCENT]  = PLFW_KEY_GRAVE_ACCENT;
    _plfw.null.keycodes[PLFW_NULL_SC_WORLD_1]       = PLFW_KEY_WORLD_1;
    _plfw.null.keycodes[PLFW_NULL_SC_WORLD_2]       = PLFW_KEY_WORLD_2;
    _plfw.null.keycodes[PLFW_NULL_SC_ESCAPE]        = PLFW_KEY_ESCAPE;
    _plfw.null.keycodes[PLFW_NULL_SC_ENTER]         = PLFW_KEY_ENTER;
    _plfw.null.keycodes[PLFW_NULL_SC_TAB]           = PLFW_KEY_TAB;
    _plfw.null.keycodes[PLFW_NULL_SC_BACKSPACE]     = PLFW_KEY_BACKSPACE;
    _plfw.null.keycodes[PLFW_NULL_SC_INSERT]        = PLFW_KEY_INSERT;
    _plfw.null.keycodes[PLFW_NULL_SC_DELETE]        = PLFW_KEY_DELETE;
    _plfw.null.keycodes[PLFW_NULL_SC_RIGHT]         = PLFW_KEY_RIGHT;
    _plfw.null.keycodes[PLFW_NULL_SC_LEFT]          = PLFW_KEY_LEFT;
    _plfw.null.keycodes[PLFW_NULL_SC_DOWN]          = PLFW_KEY_DOWN;
    _plfw.null.keycodes[PLFW_NULL_SC_UP]            = PLFW_KEY_UP;
    _plfw.null.keycodes[PLFW_NULL_SC_PAGE_UP]       = PLFW_KEY_PAGE_UP;
    _plfw.null.keycodes[PLFW_NULL_SC_PAGE_DOWN]     = PLFW_KEY_PAGE_DOWN;
    _plfw.null.keycodes[PLFW_NULL_SC_HOME]          = PLFW_KEY_HOME;
    _plfw.null.keycodes[PLFW_NULL_SC_END]           = PLFW_KEY_END;
    _plfw.null.keycodes[PLFW_NULL_SC_CAPS_LOCK]     = PLFW_KEY_CAPS_LOCK;
    _plfw.null.keycodes[PLFW_NULL_SC_SCROLL_LOCK]   = PLFW_KEY_SCROLL_LOCK;
    _plfw.null.keycodes[PLFW_NULL_SC_NUM_LOCK]      = PLFW_KEY_NUM_LOCK;
    _plfw.null.keycodes[PLFW_NULL_SC_PRINT_SCREEN]  = PLFW_KEY_PRINT_SCREEN;
    _plfw.null.keycodes[PLFW_NULL_SC_PAUSE]         = PLFW_KEY_PAUSE;
    _plfw.null.keycodes[PLFW_NULL_SC_F1]            = PLFW_KEY_F1;
    _plfw.null.keycodes[PLFW_NULL_SC_F2]            = PLFW_KEY_F2;
    _plfw.null.keycodes[PLFW_NULL_SC_F3]            = PLFW_KEY_F3;
    _plfw.null.keycodes[PLFW_NULL_SC_F4]            = PLFW_KEY_F4;
    _plfw.null.keycodes[PLFW_NULL_SC_F5]            = PLFW_KEY_F5;
    _plfw.null.keycodes[PLFW_NULL_SC_F6]            = PLFW_KEY_F6;
    _plfw.null.keycodes[PLFW_NULL_SC_F7]            = PLFW_KEY_F7;
    _plfw.null.keycodes[PLFW_NULL_SC_F8]            = PLFW_KEY_F8;
    _plfw.null.keycodes[PLFW_NULL_SC_F9]            = PLFW_KEY_F9;
    _plfw.null.keycodes[PLFW_NULL_SC_F10]           = PLFW_KEY_F10;
    _plfw.null.keycodes[PLFW_NULL_SC_F11]           = PLFW_KEY_F11;
    _plfw.null.keycodes[PLFW_NULL_SC_F12]           = PLFW_KEY_F12;
    _plfw.null.keycodes[PLFW_NULL_SC_F13]           = PLFW_KEY_F13;
    _plfw.null.keycodes[PLFW_NULL_SC_F14]           = PLFW_KEY_F14;
    _plfw.null.keycodes[PLFW_NULL_SC_F15]           = PLFW_KEY_F15;
    _plfw.null.keycodes[PLFW_NULL_SC_F16]           = PLFW_KEY_F16;
    _plfw.null.keycodes[PLFW_NULL_SC_F17]           = PLFW_KEY_F17;
    _plfw.null.keycodes[PLFW_NULL_SC_F18]           = PLFW_KEY_F18;
    _plfw.null.keycodes[PLFW_NULL_SC_F19]           = PLFW_KEY_F19;
    _plfw.null.keycodes[PLFW_NULL_SC_F20]           = PLFW_KEY_F20;
    _plfw.null.keycodes[PLFW_NULL_SC_F21]           = PLFW_KEY_F21;
    _plfw.null.keycodes[PLFW_NULL_SC_F22]           = PLFW_KEY_F22;
    _plfw.null.keycodes[PLFW_NULL_SC_F23]           = PLFW_KEY_F23;
    _plfw.null.keycodes[PLFW_NULL_SC_F24]           = PLFW_KEY_F24;
    _plfw.null.keycodes[PLFW_NULL_SC_F25]           = PLFW_KEY_F25;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_0]          = PLFW_KEY_KP_0;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_1]          = PLFW_KEY_KP_1;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_2]          = PLFW_KEY_KP_2;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_3]          = PLFW_KEY_KP_3;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_4]          = PLFW_KEY_KP_4;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_5]          = PLFW_KEY_KP_5;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_6]          = PLFW_KEY_KP_6;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_7]          = PLFW_KEY_KP_7;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_8]          = PLFW_KEY_KP_8;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_9]          = PLFW_KEY_KP_9;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_DECIMAL]    = PLFW_KEY_KP_DECIMAL;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_DIVIDE]     = PLFW_KEY_KP_DIVIDE;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_MULTIPLY]   = PLFW_KEY_KP_MULTIPLY;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_SUBTRACT]   = PLFW_KEY_KP_SUBTRACT;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_ADD]        = PLFW_KEY_KP_ADD;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_ENTER]      = PLFW_KEY_KP_ENTER;
    _plfw.null.keycodes[PLFW_NULL_SC_KP_EQUAL]      = PLFW_KEY_KP_EQUAL;
    _plfw.null.keycodes[PLFW_NULL_SC_LEFT_SHIFT]    = PLFW_KEY_LEFT_SHIFT;
    _plfw.null.keycodes[PLFW_NULL_SC_LEFT_CONTROL]  = PLFW_KEY_LEFT_CONTROL;
    _plfw.null.keycodes[PLFW_NULL_SC_LEFT_ALT]      = PLFW_KEY_LEFT_ALT;
    _plfw.null.keycodes[PLFW_NULL_SC_LEFT_SUPER]    = PLFW_KEY_LEFT_SUPER;
    _plfw.null.keycodes[PLFW_NULL_SC_RIGHT_SHIFT]   = PLFW_KEY_RIGHT_SHIFT;
    _plfw.null.keycodes[PLFW_NULL_SC_RIGHT_CONTROL] = PLFW_KEY_RIGHT_CONTROL;
    _plfw.null.keycodes[PLFW_NULL_SC_RIGHT_ALT]     = PLFW_KEY_RIGHT_ALT;
    _plfw.null.keycodes[PLFW_NULL_SC_RIGHT_SUPER]   = PLFW_KEY_RIGHT_SUPER;
    _plfw.null.keycodes[PLFW_NULL_SC_MENU]          = PLFW_KEY_MENU;

    for (scancode = PLFW_NULL_SC_FIRST;  scancode < PLFW_NULL_SC_LAST;  scancode++)
    {
        if (_plfw.null.keycodes[scancode] > 0)
            _plfw.null.scancodes[_plfw.null.keycodes[scancode]] = scancode;
    }

    _plfwPollMonitorsNull();
    return PLFW_TRUE;
}

void _plfwTerminateNull(void)
{
    free(_plfw.null.clipboardString);
    memset(&_plfw.null, 0, sizeof(_plfw.null));
}

