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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

#include "wayland-client-protocol.h"


static void outputHandleGeometry(void* userData,
                                 struct wl_output* output,
                                 int32_t x,
                                 int32_t y,
                                 int32_t physicalWidth,
                                 int32_t physicalHeight,
                                 int32_t subpixel,
                                 const char* make,
                                 const char* model,
                                 int32_t transform)
{
    struct _PLFWmonitor* monitor = userData;

    monitor->wl.x = x;
    monitor->wl.y = y;
    monitor->widthMM = physicalWidth;
    monitor->heightMM = physicalHeight;

    if (strlen(monitor->name) == 0)
        snprintf(monitor->name, sizeof(monitor->name), "%s %s", make, model);
}

static void outputHandleMode(void* userData,
                             struct wl_output* output,
                             uint32_t flags,
                             int32_t width,
                             int32_t height,
                             int32_t refresh)
{
    struct _PLFWmonitor* monitor = userData;
    PLFWvidmode mode;

    mode.width = width;
    mode.height = height;
    mode.redBits = 8;
    mode.greenBits = 8;
    mode.blueBits = 8;
    mode.refreshRate = (int) round(refresh / 1000.0);

    monitor->modeCount++;
    monitor->modes =
        _plfw_realloc(monitor->modes, monitor->modeCount * sizeof(PLFWvidmode));
    monitor->modes[monitor->modeCount - 1] = mode;

    if (flags & WL_OUTPUT_MODE_CURRENT)
        monitor->wl.currentMode = monitor->modeCount - 1;
}

static void outputHandleDone(void* userData, struct wl_output* output)
{
    struct _PLFWmonitor* monitor = userData;

    if (monitor->widthMM <= 0 || monitor->heightMM <= 0)
    {
        // If Wayland does not provide a physical size, assume the default 96 DPI
        const PLFWvidmode* mode = &monitor->modes[monitor->wl.currentMode];
        monitor->widthMM  = (int) (mode->width * 25.4f / 96.f);
        monitor->heightMM = (int) (mode->height * 25.4f / 96.f);
    }

    for (int i = 0; i < _plfw.monitorCount; i++)
    {
        if (_plfw.monitors[i] == monitor)
            return;
    }

    _plfwInputMonitor(monitor, PLFW_CONNECTED, _PLFW_INSERT_LAST);
}

static void outputHandleScale(void* userData,
                              struct wl_output* output,
                              int32_t factor)
{
    struct _PLFWmonitor* monitor = userData;

    monitor->wl.scale = factor;

    for (_PLFWwindow* window = _plfw.windowListHead; window; window = window->next)
    {
        for (size_t i = 0; i < window->wl.outputScaleCount; i++)
        {
            if (window->wl.outputScales[i].output == monitor->wl.output)
            {
                window->wl.outputScales[i].factor = monitor->wl.scale;
                _plfwUpdateBufferScaleFromOutputsWayland(window);
                break;
            }
        }
    }
}

void outputHandleName(void* userData, struct wl_output* wl_output, const char* name)
{
    struct _PLFWmonitor* monitor = userData;

    strncpy(monitor->name, name, sizeof(monitor->name) - 1);
}

void outputHandleDescription(void* userData,
                             struct wl_output* wl_output,
                             const char* description)
{
}

static const struct wl_output_listener outputListener =
{
    outputHandleGeometry,
    outputHandleMode,
    outputHandleDone,
    outputHandleScale,
    outputHandleName,
    outputHandleDescription,
};


//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

void _plfwAddOutputWayland(uint32_t name, uint32_t version)
{
    if (version < 2)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Unsupported output interface version");
        return;
    }

    version = _plfw_min(version, WL_OUTPUT_NAME_SINCE_VERSION);

    struct wl_output* output = wl_registry_bind(_plfw.wl.registry,
                                                name,
                                                &wl_output_interface,
                                                version);
    if (!output)
        return;

    // The actual name of this output will be set in the geometry handler
    _PLFWmonitor* monitor = _plfwAllocMonitor("", 0, 0);
    monitor->wl.scale = 1;
    monitor->wl.output = output;
    monitor->wl.name = name;

    wl_proxy_set_tag((struct wl_proxy*) output, &_plfw.wl.tag);
    wl_output_add_listener(output, &outputListener, monitor);
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _plfwFreeMonitorWayland(_PLFWmonitor* monitor)
{
    if (monitor->wl.output)
        wl_output_destroy(monitor->wl.output);
}

void _plfwGetMonitorPosWayland(_PLFWmonitor* monitor, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = monitor->wl.x;
    if (ypos)
        *ypos = monitor->wl.y;
}

void _plfwGetMonitorContentScaleWayland(_PLFWmonitor* monitor,
                                        float* xscale, float* yscale)
{
    if (xscale)
        *xscale = (float) monitor->wl.scale;
    if (yscale)
        *yscale = (float) monitor->wl.scale;
}

void _plfwGetMonitorWorkareaWayland(_PLFWmonitor* monitor,
                                    int* xpos, int* ypos,
                                    int* width, int* height)
{
    if (xpos)
        *xpos = monitor->wl.x;
    if (ypos)
        *ypos = monitor->wl.y;
    if (width)
        *width = monitor->modes[monitor->wl.currentMode].width;
    if (height)
        *height = monitor->modes[monitor->wl.currentMode].height;
}

PLFWvidmode* _plfwGetVideoModesWayland(_PLFWmonitor* monitor, int* found)
{
    *found = monitor->modeCount;
    return monitor->modes;
}

PLFWbool _plfwGetVideoModeWayland(_PLFWmonitor* monitor, PLFWvidmode* mode)
{
    *mode = monitor->modes[monitor->wl.currentMode];
    return PLFW_TRUE;
}

PLFWbool _plfwGetGammaRampWayland(_PLFWmonitor* monitor, PLFWgammaramp* ramp)
{
    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: Gamma ramp access is not available");
    return PLFW_FALSE;
}

void _plfwSetGammaRampWayland(_PLFWmonitor* monitor, const PLFWgammaramp* ramp)
{
    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: Gamma ramp access is not available");
}


//////////////////////////////////////////////////////////////////////////
//////                        PLFW native API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI struct wl_output* plfwGetWaylandMonitor(PLFWmonitor* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_plfw.platform.platformID != PLFW_PLATFORM_WAYLAND)
    {
        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE, "Wayland: Platform not initialized");
        return NULL;
    }

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    return monitor->wl.output;
}

#endif // _PLFW_WAYLAND

