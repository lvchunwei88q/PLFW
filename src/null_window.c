//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2016 Google Inc.
// Copyright (c) 2016-2019 Camilla Löwy <elmindreda@glfw.org>
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

static void applySizeLimits(_PLFWwindow* window, int* width, int* height)
{
    if (window->numer != PLFW_DONT_CARE && window->denom != PLFW_DONT_CARE)
    {
        const float ratio = (float) window->numer / (float) window->denom;
        *height = (int) (*width / ratio);
    }

    if (window->minwidth != PLFW_DONT_CARE)
        *width = _plfw_max(*width, window->minwidth);
    else if (window->maxwidth != PLFW_DONT_CARE)
        *width = _plfw_min(*width, window->maxwidth);

    if (window->minheight != PLFW_DONT_CARE)
        *height = _plfw_min(*height, window->minheight);
    else if (window->maxheight != PLFW_DONT_CARE)
        *height = _plfw_max(*height, window->maxheight);
}

static void fitToMonitor(_PLFWwindow* window)
{
    PLFWvidmode mode;
    _plfwGetVideoModeNull(window->monitor, &mode);
    _plfwGetMonitorPosNull(window->monitor,
                           &window->null.xpos,
                           &window->null.ypos);
    window->null.width = mode.width;
    window->null.height = mode.height;
}

static void acquireMonitor(_PLFWwindow* window)
{
    _plfwInputMonitorWindow(window->monitor, window);
}

static void releaseMonitor(_PLFWwindow* window)
{
    if (window->monitor->window != window)
        return;

    _plfwInputMonitorWindow(window->monitor, NULL);
}

static int createNativeWindow(_PLFWwindow* window,
                              const _PLFWwndconfig* wndconfig,
                              const _PLFWfbconfig* fbconfig)
{
    if (window->monitor)
        fitToMonitor(window);
    else
    {
        if (wndconfig->xpos == PLFW_ANY_POSITION && wndconfig->ypos == PLFW_ANY_POSITION)
        {
            window->null.xpos = 17;
            window->null.ypos = 17;
        }
        else
        {
            window->null.xpos = wndconfig->xpos;
            window->null.ypos = wndconfig->ypos;
        }

        window->null.width = wndconfig->width;
        window->null.height = wndconfig->height;
    }

    window->null.visible = wndconfig->visible;
    window->null.decorated = wndconfig->decorated;
    window->null.maximized = wndconfig->maximized;
    window->null.floating = wndconfig->floating;
    window->null.transparent = fbconfig->transparent;
    window->null.opacity = 1.f;

    return PLFW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwCreateWindowNull(_PLFWwindow* window,
                               const _PLFWwndconfig* wndconfig,
                               const _PLFWfbconfig* fbconfig)
{
    if (!createNativeWindow(window, wndconfig, fbconfig))
        return PLFW_FALSE;

    // No context is created: this library owns the window and its input state
    // only, and leaves all rendering to the application.

    if (wndconfig->mousePassthrough)
        _plfwSetWindowMousePassthroughNull(window, PLFW_TRUE);

    if (window->monitor)
    {
        _plfwShowWindowNull(window);
        _plfwFocusWindowNull(window);
        acquireMonitor(window);

        if (wndconfig->centerCursor)
            _plfwCenterCursorInContentArea(window);
    }
    else
    {
        if (wndconfig->visible)
        {
            _plfwShowWindowNull(window);
            if (wndconfig->focused)
                _plfwFocusWindowNull(window);
        }
    }

    return PLFW_TRUE;
}

void _plfwDestroyWindowNull(_PLFWwindow* window)
{
    if (window->monitor)
        releaseMonitor(window);

    if (_plfw.null.focusedWindow == window)
        _plfw.null.focusedWindow = NULL;
}

void _plfwSetWindowTitleNull(_PLFWwindow* window, const char* title)
{
}

void _plfwSetWindowIconNull(_PLFWwindow* window, int count, const PLFWimage* images)
{
}

void _plfwSetWindowMonitorNull(_PLFWwindow* window,
                               _PLFWmonitor* monitor,
                               int xpos, int ypos,
                               int width, int height,
                               int refreshRate)
{
    if (window->monitor == monitor)
    {
        if (!monitor)
        {
            _plfwSetWindowPosNull(window, xpos, ypos);
            _plfwSetWindowSizeNull(window, width, height);
        }

        return;
    }

    if (window->monitor)
        releaseMonitor(window);

    _plfwInputWindowMonitor(window, monitor);

    if (window->monitor)
    {
        window->null.visible = PLFW_TRUE;
        acquireMonitor(window);
        fitToMonitor(window);
    }
    else
    {
        _plfwSetWindowPosNull(window, xpos, ypos);
        _plfwSetWindowSizeNull(window, width, height);
    }
}

void _plfwGetWindowPosNull(_PLFWwindow* window, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = window->null.xpos;
    if (ypos)
        *ypos = window->null.ypos;
}

void _plfwSetWindowPosNull(_PLFWwindow* window, int xpos, int ypos)
{
    if (window->monitor)
        return;

    if (window->null.xpos != xpos || window->null.ypos != ypos)
    {
        window->null.xpos = xpos;
        window->null.ypos = ypos;
        _plfwInputWindowPos(window, xpos, ypos);
    }
}

void _plfwGetWindowSizeNull(_PLFWwindow* window, int* width, int* height)
{
    if (width)
        *width = window->null.width;
    if (height)
        *height = window->null.height;
}

void _plfwSetWindowSizeNull(_PLFWwindow* window, int width, int height)
{
    if (window->monitor)
        return;

    if (window->null.width != width || window->null.height != height)
    {
        window->null.width = width;
        window->null.height = height;
        _plfwInputFramebufferSize(window, width, height);
        _plfwInputWindowDamage(window);
        _plfwInputWindowSize(window, width, height);
    }
}

void _plfwSetWindowSizeLimitsNull(_PLFWwindow* window,
                                  int minwidth, int minheight,
                                  int maxwidth, int maxheight)
{
    int width = window->null.width;
    int height = window->null.height;
    applySizeLimits(window, &width, &height);
    _plfwSetWindowSizeNull(window, width, height);
}

void _plfwSetWindowAspectRatioNull(_PLFWwindow* window, int n, int d)
{
    int width = window->null.width;
    int height = window->null.height;
    applySizeLimits(window, &width, &height);
    _plfwSetWindowSizeNull(window, width, height);
}

void _plfwGetFramebufferSizeNull(_PLFWwindow* window, int* width, int* height)
{
    if (width)
        *width = window->null.width;
    if (height)
        *height = window->null.height;
}

void _plfwGetWindowFrameSizeNull(_PLFWwindow* window,
                                 int* left, int* top,
                                 int* right, int* bottom)
{
    if (window->null.decorated && !window->monitor)
    {
        if (left)
            *left = 1;
        if (top)
            *top = 10;
        if (right)
            *right = 1;
        if (bottom)
            *bottom = 1;
    }
    else
    {
        if (left)
            *left = 0;
        if (top)
            *top = 0;
        if (right)
            *right = 0;
        if (bottom)
            *bottom = 0;
    }
}

void _plfwGetWindowContentScaleNull(_PLFWwindow* window, float* xscale, float* yscale)
{
    if (xscale)
        *xscale = 1.f;
    if (yscale)
        *yscale = 1.f;
}

void _plfwIconifyWindowNull(_PLFWwindow* window)
{
    if (_plfw.null.focusedWindow == window)
    {
        _plfw.null.focusedWindow = NULL;
        _plfwInputWindowFocus(window, PLFW_FALSE);
    }

    if (!window->null.iconified)
    {
        window->null.iconified = PLFW_TRUE;
        _plfwInputWindowIconify(window, PLFW_TRUE);

        if (window->monitor)
            releaseMonitor(window);
    }
}

void _plfwRestoreWindowNull(_PLFWwindow* window)
{
    if (window->null.iconified)
    {
        window->null.iconified = PLFW_FALSE;
        _plfwInputWindowIconify(window, PLFW_FALSE);

        if (window->monitor)
            acquireMonitor(window);
    }
    else if (window->null.maximized)
    {
        window->null.maximized = PLFW_FALSE;
        _plfwInputWindowMaximize(window, PLFW_FALSE);
    }
}

void _plfwMaximizeWindowNull(_PLFWwindow* window)
{
    if (!window->null.maximized)
    {
        window->null.maximized = PLFW_TRUE;
        _plfwInputWindowMaximize(window, PLFW_TRUE);
    }
}

PLFWbool _plfwWindowMaximizedNull(_PLFWwindow* window)
{
    return window->null.maximized;
}

PLFWbool _plfwWindowHoveredNull(_PLFWwindow* window)
{
    return _plfw.null.xcursor >= window->null.xpos &&
           _plfw.null.ycursor >= window->null.ypos &&
           _plfw.null.xcursor <= window->null.xpos + window->null.width - 1 &&
           _plfw.null.ycursor <= window->null.ypos + window->null.height - 1;
}

PLFWbool _plfwFramebufferTransparentNull(_PLFWwindow* window)
{
    return window->null.transparent;
}

void _plfwSetWindowResizableNull(_PLFWwindow* window, PLFWbool enabled)
{
    window->null.resizable = enabled;
}

void _plfwSetWindowDecoratedNull(_PLFWwindow* window, PLFWbool enabled)
{
    window->null.decorated = enabled;
}

void _plfwSetWindowFloatingNull(_PLFWwindow* window, PLFWbool enabled)
{
    window->null.floating = enabled;
}

void _plfwSetWindowMousePassthroughNull(_PLFWwindow* window, PLFWbool enabled)
{
}

float _plfwGetWindowOpacityNull(_PLFWwindow* window)
{
    return window->null.opacity;
}

void _plfwSetWindowOpacityNull(_PLFWwindow* window, float opacity)
{
    window->null.opacity = opacity;
}

void _plfwSetRawMouseMotionNull(_PLFWwindow *window, PLFWbool enabled)
{
}

PLFWbool _plfwRawMouseMotionSupportedNull(void)
{
    return PLFW_TRUE;
}

void _plfwShowWindowNull(_PLFWwindow* window)
{
    window->null.visible = PLFW_TRUE;
}

void _plfwRequestWindowAttentionNull(_PLFWwindow* window)
{
}

void _plfwHideWindowNull(_PLFWwindow* window)
{
    if (_plfw.null.focusedWindow == window)
    {
        _plfw.null.focusedWindow = NULL;
        _plfwInputWindowFocus(window, PLFW_FALSE);
    }

    window->null.visible = PLFW_FALSE;
}

void _plfwFocusWindowNull(_PLFWwindow* window)
{
    _PLFWwindow* previous;

    if (_plfw.null.focusedWindow == window)
        return;

    if (!window->null.visible)
        return;

    previous = _plfw.null.focusedWindow;
    _plfw.null.focusedWindow = window;

    if (previous)
    {
        _plfwInputWindowFocus(previous, PLFW_FALSE);
        if (previous->monitor && previous->autoIconify)
            _plfwIconifyWindowNull(previous);
    }

    _plfwInputWindowFocus(window, PLFW_TRUE);
}

PLFWbool _plfwWindowFocusedNull(_PLFWwindow* window)
{
    return _plfw.null.focusedWindow == window;
}

PLFWbool _plfwWindowIconifiedNull(_PLFWwindow* window)
{
    return window->null.iconified;
}

PLFWbool _plfwWindowVisibleNull(_PLFWwindow* window)
{
    return window->null.visible;
}

void _plfwPollEventsNull(void)
{
}

void _plfwWaitEventsNull(void)
{
}

void _plfwWaitEventsTimeoutNull(double timeout)
{
}

void _plfwPostEmptyEventNull(void)
{
}

void _plfwGetCursorPosNull(_PLFWwindow* window, double* xpos, double* ypos)
{
    if (xpos)
        *xpos = _plfw.null.xcursor - window->null.xpos;
    if (ypos)
        *ypos = _plfw.null.ycursor - window->null.ypos;
}

void _plfwSetCursorPosNull(_PLFWwindow* window, double x, double y)
{
    _plfw.null.xcursor = window->null.xpos + (int) x;
    _plfw.null.ycursor = window->null.ypos + (int) y;
}

void _plfwSetCursorModeNull(_PLFWwindow* window, int mode)
{
}

PLFWbool _plfwCreateCursorNull(_PLFWcursor* cursor,
                               const PLFWimage* image,
                               int xhot, int yhot)
{
    return PLFW_TRUE;
}

PLFWbool _plfwCreateStandardCursorNull(_PLFWcursor* cursor, int shape)
{
    return PLFW_TRUE;
}

void _plfwDestroyCursorNull(_PLFWcursor* cursor)
{
}

void _plfwSetCursorNull(_PLFWwindow* window, _PLFWcursor* cursor)
{
}

void _plfwSetClipboardStringNull(const char* string)
{
    char* copy = _plfw_strdup(string);
    _plfw_free(_plfw.null.clipboardString);
    _plfw.null.clipboardString = copy;
}

const char* _plfwGetClipboardStringNull(void)
{
    return _plfw.null.clipboardString;
}

const char* _plfwGetScancodeNameNull(int scancode)
{
    if (scancode < PLFW_NULL_SC_FIRST || scancode > PLFW_NULL_SC_LAST)
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid scancode %i", scancode);
        return NULL;
    }

    switch (scancode)
    {
        case PLFW_NULL_SC_APOSTROPHE:
            return "'";
        case PLFW_NULL_SC_COMMA:
            return ",";
        case PLFW_NULL_SC_MINUS:
        case PLFW_NULL_SC_KP_SUBTRACT:
            return "-";
        case PLFW_NULL_SC_PERIOD:
        case PLFW_NULL_SC_KP_DECIMAL:
            return ".";
        case PLFW_NULL_SC_SLASH:
        case PLFW_NULL_SC_KP_DIVIDE:
            return "/";
        case PLFW_NULL_SC_SEMICOLON:
            return ";";
        case PLFW_NULL_SC_EQUAL:
        case PLFW_NULL_SC_KP_EQUAL:
            return "=";
        case PLFW_NULL_SC_LEFT_BRACKET:
            return "[";
        case PLFW_NULL_SC_RIGHT_BRACKET:
            return "]";
        case PLFW_NULL_SC_KP_MULTIPLY:
            return "*";
        case PLFW_NULL_SC_KP_ADD:
            return "+";
        case PLFW_NULL_SC_BACKSLASH:
        case PLFW_NULL_SC_WORLD_1:
        case PLFW_NULL_SC_WORLD_2:
            return "\\";
        case PLFW_NULL_SC_0:
        case PLFW_NULL_SC_KP_0:
            return "0";
        case PLFW_NULL_SC_1:
        case PLFW_NULL_SC_KP_1:
            return "1";
        case PLFW_NULL_SC_2:
        case PLFW_NULL_SC_KP_2:
            return "2";
        case PLFW_NULL_SC_3:
        case PLFW_NULL_SC_KP_3:
            return "3";
        case PLFW_NULL_SC_4:
        case PLFW_NULL_SC_KP_4:
            return "4";
        case PLFW_NULL_SC_5:
        case PLFW_NULL_SC_KP_5:
            return "5";
        case PLFW_NULL_SC_6:
        case PLFW_NULL_SC_KP_6:
            return "6";
        case PLFW_NULL_SC_7:
        case PLFW_NULL_SC_KP_7:
            return "7";
        case PLFW_NULL_SC_8:
        case PLFW_NULL_SC_KP_8:
            return "8";
        case PLFW_NULL_SC_9:
        case PLFW_NULL_SC_KP_9:
            return "9";
        case PLFW_NULL_SC_A:
            return "a";
        case PLFW_NULL_SC_B:
            return "b";
        case PLFW_NULL_SC_C:
            return "c";
        case PLFW_NULL_SC_D:
            return "d";
        case PLFW_NULL_SC_E:
            return "e";
        case PLFW_NULL_SC_F:
            return "f";
        case PLFW_NULL_SC_G:
            return "g";
        case PLFW_NULL_SC_H:
            return "h";
        case PLFW_NULL_SC_I:
            return "i";
        case PLFW_NULL_SC_J:
            return "j";
        case PLFW_NULL_SC_K:
            return "k";
        case PLFW_NULL_SC_L:
            return "l";
        case PLFW_NULL_SC_M:
            return "m";
        case PLFW_NULL_SC_N:
            return "n";
        case PLFW_NULL_SC_O:
            return "o";
        case PLFW_NULL_SC_P:
            return "p";
        case PLFW_NULL_SC_Q:
            return "q";
        case PLFW_NULL_SC_R:
            return "r";
        case PLFW_NULL_SC_S:
            return "s";
        case PLFW_NULL_SC_T:
            return "t";
        case PLFW_NULL_SC_U:
            return "u";
        case PLFW_NULL_SC_V:
            return "v";
        case PLFW_NULL_SC_W:
            return "w";
        case PLFW_NULL_SC_X:
            return "x";
        case PLFW_NULL_SC_Y:
            return "y";
        case PLFW_NULL_SC_Z:
            return "z";
    }

    return NULL;
}

int _plfwGetKeyScancodeNull(int key)
{
    return _plfw.null.scancodes[key];
}
