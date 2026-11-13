//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2019 Camilla Löwy <elmindreda@glfw.org>
// Copyright (c) 2012 Torsten Walluhn <tw@mad-cad.net>
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>

//////////////////////////////////////////////////////////////////////////
//////                         PLFW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code that a window has lost or received input focus
//
void _plfwInputWindowFocus(_PLFWwindow* window, PLFWbool focused)
{
    assert(window != NULL);
    assert(focused == PLFW_TRUE || focused == PLFW_FALSE);

    if (window->callbacks.focus)
        window->callbacks.focus((PLFWwindow*) window, focused);

    if (!focused)
    {
        int key, button;

        for (key = 0;  key <= PLFW_KEY_LAST;  key++)
        {
            if (window->keys[key] == PLFW_PRESS)
            {
                const int scancode = _plfw.platform.getKeyScancode(key);
                _plfwInputKey(window, key, scancode, PLFW_RELEASE, 0);
            }
        }

        for (button = 0;  button <= PLFW_MOUSE_BUTTON_LAST;  button++)
        {
            if (window->mouseButtons[button] == PLFW_PRESS)
                _plfwInputMouseClick(window, button, PLFW_RELEASE, 0);
        }
    }
}

// Notifies shared code that a window has moved
// The position is specified in content area relative screen coordinates
//
void _plfwInputWindowPos(_PLFWwindow* window, int x, int y)
{
    assert(window != NULL);

    if (window->callbacks.pos)
        window->callbacks.pos((PLFWwindow*) window, x, y);
}

// Notifies shared code that a window has been resized
// The size is specified in screen coordinates
//
void _plfwInputWindowSize(_PLFWwindow* window, int width, int height)
{
    assert(window != NULL);
    assert(width >= 0);
    assert(height >= 0);

    if (window->callbacks.size)
        window->callbacks.size((PLFWwindow*) window, width, height);
}

// Notifies shared code that a window has been iconified or restored
//
void _plfwInputWindowIconify(_PLFWwindow* window, PLFWbool iconified)
{
    assert(window != NULL);
    assert(iconified == PLFW_TRUE || iconified == PLFW_FALSE);

    if (window->callbacks.iconify)
        window->callbacks.iconify((PLFWwindow*) window, iconified);
}

// Notifies shared code that a window has been maximized or restored
//
void _plfwInputWindowMaximize(_PLFWwindow* window, PLFWbool maximized)
{
    assert(window != NULL);
    assert(maximized == PLFW_TRUE || maximized == PLFW_FALSE);

    if (window->callbacks.maximize)
        window->callbacks.maximize((PLFWwindow*) window, maximized);
}

// Notifies shared code that a window framebuffer has been resized
// The size is specified in pixels
//
void _plfwInputFramebufferSize(_PLFWwindow* window, int width, int height)
{
    assert(window != NULL);
    assert(width >= 0);
    assert(height >= 0);

    if (window->callbacks.fbsize)
        window->callbacks.fbsize((PLFWwindow*) window, width, height);
}

// Notifies shared code that a window content scale has changed
// The scale is specified as the ratio between the current and default DPI
//
void _plfwInputWindowContentScale(_PLFWwindow* window, float xscale, float yscale)
{
    assert(window != NULL);
    assert(xscale > 0.f);
    assert(isfinite(xscale));
    assert(yscale > 0.f);
    assert(isfinite(yscale));

    if (window->callbacks.scale)
        window->callbacks.scale((PLFWwindow*) window, xscale, yscale);
}

// Notifies shared code that the window contents needs updating
//
void _plfwInputWindowDamage(_PLFWwindow* window)
{
    assert(window != NULL);

    if (window->callbacks.refresh)
        window->callbacks.refresh((PLFWwindow*) window);
}

// Notifies shared code that the user wishes to close a window
//
void _plfwInputWindowCloseRequest(_PLFWwindow* window)
{
    assert(window != NULL);

    window->shouldClose = PLFW_TRUE;

    if (window->callbacks.close)
        window->callbacks.close((PLFWwindow*) window);
}

// Notifies shared code that a window has changed its desired monitor
//
void _plfwInputWindowMonitor(_PLFWwindow* window, _PLFWmonitor* monitor)
{
    assert(window != NULL);
    window->monitor = monitor;
}

//////////////////////////////////////////////////////////////////////////
//////                        PLFW public API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI PLFWwindow* plfwCreateWindow(int width, int height,
                                     const char* title,
                                     PLFWmonitor* monitor,
                                     PLFWwindow* share)
{
    _PLFWfbconfig fbconfig;
    _PLFWwndconfig wndconfig;
    _PLFWwindow* window;

    assert(title != NULL);
    assert(width >= 0);
    assert(height >= 0);

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (width <= 0 || height <= 0)
    {
        _plfwInputError(PLFW_INVALID_VALUE,
                        "Invalid window size %ix%i",
                        width, height);

        return NULL;
    }

    // No rasterizer context is created for the window, so there is nothing to
    // share resources with.  The parameter is kept for call signature
    // compatibility with the PLFW API.
    (void) share;

    fbconfig  = _plfw.hints.framebuffer;
    wndconfig = _plfw.hints.window;

    wndconfig.width   = width;
    wndconfig.height  = height;

    window = _plfw_calloc(1, sizeof(_PLFWwindow));
    window->next = _plfw.windowListHead;
    _plfw.windowListHead = window;

    window->videoMode.width       = width;
    window->videoMode.height      = height;
    window->videoMode.redBits     = fbconfig.redBits;
    window->videoMode.greenBits   = fbconfig.greenBits;
    window->videoMode.blueBits    = fbconfig.blueBits;
    window->videoMode.refreshRate = _plfw.hints.refreshRate;

    window->monitor          = (_PLFWmonitor*) monitor;
    window->resizable        = wndconfig.resizable;
    window->decorated        = wndconfig.decorated;
    window->autoIconify      = wndconfig.autoIconify;
    window->floating         = wndconfig.floating;
    window->focusOnShow      = wndconfig.focusOnShow;
    window->mousePassthrough = wndconfig.mousePassthrough;
    window->cursorMode       = PLFW_CURSOR_NORMAL;

    window->minwidth    = PLFW_DONT_CARE;
    window->minheight   = PLFW_DONT_CARE;
    window->maxwidth    = PLFW_DONT_CARE;
    window->maxheight   = PLFW_DONT_CARE;
    window->numer       = PLFW_DONT_CARE;
    window->denom       = PLFW_DONT_CARE;
    window->title       = _plfw_strdup(title);

    if (!_plfw.platform.createWindow(window, &wndconfig, &fbconfig))
    {
        plfwDestroyWindow((PLFWwindow*) window);
        return NULL;
    }

    return (PLFWwindow*) window;
}

void plfwDefaultWindowHints(void)
{
    _PLFW_REQUIRE_INIT();

    // The default is a focused, visible, resizable window with decorations
    memset(&_plfw.hints.window, 0, sizeof(_plfw.hints.window));
    _plfw.hints.window.resizable    = true;
    _plfw.hints.window.visible      = true;
    _plfw.hints.window.decorated    = true;
    _plfw.hints.window.focused      = true;
    _plfw.hints.window.autoIconify  = true;
    _plfw.hints.window.centerCursor = true;
    _plfw.hints.window.focusOnShow  = true;
    _plfw.hints.window.xpos         = PLFW_ANY_POSITION;
    _plfw.hints.window.ypos         = PLFW_ANY_POSITION;
    _plfw.hints.window.scaleFramebuffer = true;

    // No pixel format is requested from the window system.  The bit depths
    // below are only nominal values used to report the window's video mode.
    memset(&_plfw.hints.framebuffer, 0, sizeof(_plfw.hints.framebuffer));
    _plfw.hints.framebuffer.redBits   = 8;
    _plfw.hints.framebuffer.greenBits = 8;
    _plfw.hints.framebuffer.blueBits  = 8;

    // The default is to select the highest available refresh rate
    _plfw.hints.refreshRate = PLFW_DONT_CARE;
}

PLFWAPI void plfwWindowHint(int hint, int value)
{
    _PLFW_REQUIRE_INIT();

    switch (hint)
    {
        case PLFW_TRANSPARENT_FRAMEBUFFER:
            _plfw.hints.framebuffer.transparent = value;
            return;
        case PLFW_RESIZABLE:
            _plfw.hints.window.resizable = value;
            return;
        case PLFW_DECORATED:
            _plfw.hints.window.decorated = value;
            return;
        case PLFW_FOCUSED:
            _plfw.hints.window.focused = value;
            return;
        case PLFW_AUTO_ICONIFY:
            _plfw.hints.window.autoIconify = value;
            return;
        case PLFW_FLOATING:
            _plfw.hints.window.floating = value;
            return;
        case PLFW_MAXIMIZED:
            _plfw.hints.window.maximized = value;
            return;
        case PLFW_VISIBLE:
            _plfw.hints.window.visible = value;
            return;
        case PLFW_POSITION_X:
            _plfw.hints.window.xpos = value;
            return;
        case PLFW_POSITION_Y:
            _plfw.hints.window.ypos = value;
            return;
        case PLFW_WIN32_KEYBOARD_MENU:
            _plfw.hints.window.win32.keymenu = value;
            return;
        case PLFW_WIN32_SHOWDEFAULT:
            _plfw.hints.window.win32.showDefault = value;
            return;
        case PLFW_SCALE_TO_MONITOR:
            _plfw.hints.window.scaleToMonitor = value;
            return;
        case PLFW_SCALE_FRAMEBUFFER:
            _plfw.hints.window.scaleFramebuffer = value;
            return;
        case PLFW_CENTER_CURSOR:
            _plfw.hints.window.centerCursor = value;
            return;
        case PLFW_FOCUS_ON_SHOW:
            _plfw.hints.window.focusOnShow = value;
            return;
        case PLFW_MOUSE_PASSTHROUGH:
            _plfw.hints.window.mousePassthrough = value;
            return;
        case PLFW_REFRESH_RATE:
            _plfw.hints.refreshRate = value;
            return;
    }

    _plfwInputError(PLFW_INVALID_ENUM, "Invalid window hint 0x%08X", hint);
}

PLFWAPI void plfwWindowHintString(int hint, const char* value)
{
    assert(value != NULL);

    _PLFW_REQUIRE_INIT();

    switch (hint)
    {
        case PLFW_WAYLAND_APP_ID:
            strncpy(_plfw.hints.window.wl.appId, value,
                    sizeof(_plfw.hints.window.wl.appId) - 1);
            return;
    }

    _plfwInputError(PLFW_INVALID_ENUM, "Invalid window hint string 0x%08X", hint);
}

PLFWAPI void plfwDestroyWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;

    // Allow closing of NULL (to match the behavior of free)
    if (window == NULL)
        return;

    // Clear all callbacks to avoid exposing a half torn-down window object
    memset(&window->callbacks, 0, sizeof(window->callbacks));

    _plfw.platform.destroyWindow(window);

    // Unlink window from global linked list
    {
        _PLFWwindow** prev = &_plfw.windowListHead;

        while (*prev != window)
            prev = &((*prev)->next);

        *prev = window->next;
    }

    _plfw_free(window->title);
    _plfw_free(window);
}

PLFWAPI int plfwWindowShouldClose(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    return window->shouldClose;
}

PLFWAPI void plfwSetWindowShouldClose(PLFWwindow* handle, int value)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    window->shouldClose = value;
}

PLFWAPI const char* plfwGetWindowTitle(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    return window->title;
}

PLFWAPI void plfwSetWindowTitle(PLFWwindow* handle, const char* title)
{
    assert(title != NULL);

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    char* prev = window->title;
    window->title = _plfw_strdup(title);

    _plfw.platform.setWindowTitle(window, title);
    _plfw_free(prev);
}

PLFWAPI void plfwSetWindowIcon(PLFWwindow* handle,
                               int count, const PLFWimage* images)
{
    int i;

    assert(count >= 0);
    assert(count == 0 || images != NULL);

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (count < 0)
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid image count for window icon");
        return;
    }

    for (i = 0; i < count; i++)
    {
        assert(images[i].pixels != NULL);

        if (images[i].width <= 0 || images[i].height <= 0)
        {
            _plfwInputError(PLFW_INVALID_VALUE,
                            "Invalid image dimensions for window icon");
            return;
        }
    }

    _plfw.platform.setWindowIcon(window, count, images);
}

PLFWAPI void plfwGetWindowPos(PLFWwindow* handle, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.getWindowPos(window, xpos, ypos);
}

PLFWAPI void plfwSetWindowPos(PLFWwindow* handle, int xpos, int ypos)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (window->monitor)
        return;

    _plfw.platform.setWindowPos(window, xpos, ypos);
}

PLFWAPI void plfwGetWindowSize(PLFWwindow* handle, int* width, int* height)
{
    if (width)
        *width = 0;
    if (height)
        *height = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.getWindowSize(window, width, height);
}

PLFWAPI void plfwSetWindowSize(PLFWwindow* handle, int width, int height)
{
    assert(width >= 0);
    assert(height >= 0);

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    window->videoMode.width  = width;
    window->videoMode.height = height;

    _plfw.platform.setWindowSize(window, width, height);
}

PLFWAPI void plfwSetWindowSizeLimits(PLFWwindow* handle,
                                     int minwidth, int minheight,
                                     int maxwidth, int maxheight)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (minwidth != PLFW_DONT_CARE && minheight != PLFW_DONT_CARE)
    {
        if (minwidth < 0 || minheight < 0)
        {
            _plfwInputError(PLFW_INVALID_VALUE,
                            "Invalid window minimum size %ix%i",
                            minwidth, minheight);
            return;
        }
    }

    if (maxwidth != PLFW_DONT_CARE && maxheight != PLFW_DONT_CARE)
    {
        if (maxwidth < 0 || maxheight < 0 ||
            maxwidth < minwidth || maxheight < minheight)
        {
            _plfwInputError(PLFW_INVALID_VALUE,
                            "Invalid window maximum size %ix%i",
                            maxwidth, maxheight);
            return;
        }
    }

    window->minwidth  = minwidth;
    window->minheight = minheight;
    window->maxwidth  = maxwidth;
    window->maxheight = maxheight;

    if (window->monitor || !window->resizable)
        return;

    _plfw.platform.setWindowSizeLimits(window,
                                       minwidth, minheight,
                                       maxwidth, maxheight);
}

PLFWAPI void plfwSetWindowAspectRatio(PLFWwindow* handle, int numer, int denom)
{
    assert(numer != 0);
    assert(denom != 0);

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (numer != PLFW_DONT_CARE && denom != PLFW_DONT_CARE)
    {
        if (numer <= 0 || denom <= 0)
        {
            _plfwInputError(PLFW_INVALID_VALUE,
                            "Invalid window aspect ratio %i:%i",
                            numer, denom);
            return;
        }
    }

    window->numer = numer;
    window->denom = denom;

    if (window->monitor || !window->resizable)
        return;

    _plfw.platform.setWindowAspectRatio(window, numer, denom);
}

PLFWAPI void plfwGetFramebufferSize(PLFWwindow* handle, int* width, int* height)
{
    if (width)
        *width = 0;
    if (height)
        *height = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.getFramebufferSize(window, width, height);
}

PLFWAPI void plfwGetWindowFrameSize(PLFWwindow* handle,
                                    int* left, int* top,
                                    int* right, int* bottom)
{
    if (left)
        *left = 0;
    if (top)
        *top = 0;
    if (right)
        *right = 0;
    if (bottom)
        *bottom = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.getWindowFrameSize(window, left, top, right, bottom);
}

PLFWAPI void plfwGetWindowContentScale(PLFWwindow* handle,
                                       float* xscale, float* yscale)
{
    if (xscale)
        *xscale = 0.f;
    if (yscale)
        *yscale = 0.f;

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.getWindowContentScale(window, xscale, yscale);
}

PLFWAPI float plfwGetWindowOpacity(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0.f);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    return _plfw.platform.getWindowOpacity(window);
}

PLFWAPI void plfwSetWindowOpacity(PLFWwindow* handle, float opacity)
{
    assert(isfinite(opacity));
    assert(opacity >= 0.f);
    assert(opacity <= 1.f);

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (!isfinite(opacity) || opacity < 0.f || opacity > 1.f)
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid window opacity %f", opacity);
        return;
    }

    _plfw.platform.setWindowOpacity(window, opacity);
}

PLFWAPI void plfwIconifyWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.iconifyWindow(window);
}

PLFWAPI void plfwRestoreWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.restoreWindow(window);
}

PLFWAPI void plfwMaximizeWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (window->monitor)
        return;

    _plfw.platform.maximizeWindow(window);
}

PLFWAPI void plfwShowWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (window->monitor)
        return;

    _plfw.platform.showWindow(window);

    if (window->focusOnShow)
        _plfw.platform.focusWindow(window);
}

PLFWAPI void plfwRequestWindowAttention(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.requestWindowAttention(window);
}

PLFWAPI void plfwHideWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (window->monitor)
        return;

    _plfw.platform.hideWindow(window);
}

PLFWAPI void plfwFocusWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _plfw.platform.focusWindow(window);
}

PLFWAPI int plfwGetWindowAttrib(PLFWwindow* handle, int attrib)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    switch (attrib)
    {
        case PLFW_FOCUSED:
            return _plfw.platform.windowFocused(window);
        case PLFW_ICONIFIED:
            return _plfw.platform.windowIconified(window);
        case PLFW_VISIBLE:
            return _plfw.platform.windowVisible(window);
        case PLFW_MAXIMIZED:
            return _plfw.platform.windowMaximized(window);
        case PLFW_HOVERED:
            return _plfw.platform.windowHovered(window);
        case PLFW_FOCUS_ON_SHOW:
            return window->focusOnShow;
        case PLFW_MOUSE_PASSTHROUGH:
            return window->mousePassthrough;
        case PLFW_TRANSPARENT_FRAMEBUFFER:
            return _plfw.platform.framebufferTransparent(window);
        case PLFW_RESIZABLE:
            return window->resizable;
        case PLFW_DECORATED:
            return window->decorated;
        case PLFW_FLOATING:
            return window->floating;
        case PLFW_AUTO_ICONIFY:
            return window->autoIconify;
    }

    _plfwInputError(PLFW_INVALID_ENUM, "Invalid window attribute 0x%08X", attrib);
    return 0;
}

PLFWAPI void plfwSetWindowAttrib(PLFWwindow* handle, int attrib, int value)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    value = value ? PLFW_TRUE : PLFW_FALSE;

    switch (attrib)
    {
        case PLFW_AUTO_ICONIFY:
            window->autoIconify = value;
            return;

        case PLFW_RESIZABLE:
            window->resizable = value;
            if (!window->monitor)
                _plfw.platform.setWindowResizable(window, value);
            return;

        case PLFW_DECORATED:
            window->decorated = value;
            if (!window->monitor)
                _plfw.platform.setWindowDecorated(window, value);
            return;

        case PLFW_FLOATING:
            window->floating = value;
            if (!window->monitor)
                _plfw.platform.setWindowFloating(window, value);
            return;

        case PLFW_FOCUS_ON_SHOW:
            window->focusOnShow = value;
            return;

        case PLFW_MOUSE_PASSTHROUGH:
            window->mousePassthrough = value;
            _plfw.platform.setWindowMousePassthrough(window, value);
            return;
    }

    _plfwInputError(PLFW_INVALID_ENUM, "Invalid window attribute 0x%08X", attrib);
}

PLFWAPI PLFWmonitor* plfwGetWindowMonitor(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    return (PLFWmonitor*) window->monitor;
}

PLFWAPI void plfwSetWindowMonitor(PLFWwindow* wh,
                                  PLFWmonitor* mh,
                                  int xpos, int ypos,
                                  int width, int height,
                                  int refreshRate)
{
    assert(width >= 0);
    assert(height >= 0);

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) wh;
    _PLFWmonitor* monitor = (_PLFWmonitor*) mh;
    assert(window != NULL);

    if (width <= 0 || height <= 0)
    {
        _plfwInputError(PLFW_INVALID_VALUE,
                        "Invalid window size %ix%i",
                        width, height);
        return;
    }

    if (refreshRate < 0 && refreshRate != PLFW_DONT_CARE)
    {
        _plfwInputError(PLFW_INVALID_VALUE,
                        "Invalid refresh rate %i",
                        refreshRate);
        return;
    }

    window->videoMode.width       = width;
    window->videoMode.height      = height;
    window->videoMode.refreshRate = refreshRate;

    _plfw.platform.setWindowMonitor(window, monitor,
                                    xpos, ypos, width, height,
                                    refreshRate);
}

PLFWAPI void plfwSetWindowUserPointer(PLFWwindow* handle, void* pointer)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    window->userPointer = pointer;
}

PLFWAPI void* plfwGetWindowUserPointer(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    return window->userPointer;
}

PLFWAPI PLFWwindowposfun plfwSetWindowPosCallback(PLFWwindow* handle,
                                                  PLFWwindowposfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowposfun, window->callbacks.pos, cbfun);
    return cbfun;
}

PLFWAPI PLFWwindowsizefun plfwSetWindowSizeCallback(PLFWwindow* handle,
                                                    PLFWwindowsizefun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowsizefun, window->callbacks.size, cbfun);
    return cbfun;
}

PLFWAPI PLFWwindowclosefun plfwSetWindowCloseCallback(PLFWwindow* handle,
                                                      PLFWwindowclosefun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowclosefun, window->callbacks.close, cbfun);
    return cbfun;
}

PLFWAPI PLFWwindowrefreshfun plfwSetWindowRefreshCallback(PLFWwindow* handle,
                                                          PLFWwindowrefreshfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowrefreshfun, window->callbacks.refresh, cbfun);
    return cbfun;
}

PLFWAPI PLFWwindowfocusfun plfwSetWindowFocusCallback(PLFWwindow* handle,
                                                      PLFWwindowfocusfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowfocusfun, window->callbacks.focus, cbfun);
    return cbfun;
}

PLFWAPI PLFWwindowiconifyfun plfwSetWindowIconifyCallback(PLFWwindow* handle,
                                                          PLFWwindowiconifyfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowiconifyfun, window->callbacks.iconify, cbfun);
    return cbfun;
}

PLFWAPI PLFWwindowmaximizefun plfwSetWindowMaximizeCallback(PLFWwindow* handle,
                                                            PLFWwindowmaximizefun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowmaximizefun, window->callbacks.maximize, cbfun);
    return cbfun;
}

PLFWAPI PLFWframebuffersizefun plfwSetFramebufferSizeCallback(PLFWwindow* handle,
                                                              PLFWframebuffersizefun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWframebuffersizefun, window->callbacks.fbsize, cbfun);
    return cbfun;
}

PLFWAPI PLFWwindowcontentscalefun plfwSetWindowContentScaleCallback(PLFWwindow* handle,
                                                                    PLFWwindowcontentscalefun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWwindowcontentscalefun, window->callbacks.scale, cbfun);
    return cbfun;
}

PLFWAPI void plfwPollEvents(void)
{
    _PLFW_REQUIRE_INIT();
    _plfw.platform.pollEvents();
}

PLFWAPI void plfwWaitEvents(void)
{
    _PLFW_REQUIRE_INIT();
    _plfw.platform.waitEvents();
}

PLFWAPI void plfwWaitEventsTimeout(double timeout)
{
    _PLFW_REQUIRE_INIT();
    assert(isfinite(timeout));
    assert(timeout >= 0.0);

    if (!isfinite(timeout) || timeout < 0.0)
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid time %f", timeout);
        return;
    }

    _plfw.platform.waitEventsTimeout(timeout);
}

PLFWAPI void plfwPostEmptyEvent(void)
{
    _PLFW_REQUIRE_INIT();
    _plfw.platform.postEmptyEvent();
}

