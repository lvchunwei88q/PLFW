//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
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

#include <assert.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>


// Lexically compare video modes, used by qsort
//
static int compareVideoModes(const void* fp, const void* sp)
{
    const PLFWvidmode* fm = fp;
    const PLFWvidmode* sm = sp;
    const int fbpp = fm->redBits + fm->greenBits + fm->blueBits;
    const int sbpp = sm->redBits + sm->greenBits + sm->blueBits;
    const int farea = fm->width * fm->height;
    const int sarea = sm->width * sm->height;

    // First sort on color bits per pixel
    if (fbpp != sbpp)
        return fbpp - sbpp;

    // Then sort on screen area
    if (farea != sarea)
        return farea - sarea;

    // Then sort on width
    if (fm->width != sm->width)
        return fm->width - sm->width;

    // Lastly sort on refresh rate
    return fm->refreshRate - sm->refreshRate;
}

// Retrieves the available modes for the specified monitor
//
static PLFWbool refreshVideoModes(_PLFWmonitor* monitor)
{
    int modeCount;
    PLFWvidmode* modes;

    if (monitor->modes)
        return PLFW_TRUE;

    modes = _plfw.platform.getVideoModes(monitor, &modeCount);
    if (!modes)
        return PLFW_FALSE;

    qsort(modes, modeCount, sizeof(PLFWvidmode), compareVideoModes);

    _plfw_free(monitor->modes);
    monitor->modes = modes;
    monitor->modeCount = modeCount;

    return PLFW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                         PLFW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code of a monitor connection or disconnection
//
void _plfwInputMonitor(_PLFWmonitor* monitor, int action, int placement)
{
    assert(monitor != NULL);
    assert(action == PLFW_CONNECTED || action == PLFW_DISCONNECTED);
    assert(placement == _PLFW_INSERT_FIRST || placement == _PLFW_INSERT_LAST);

    if (action == PLFW_CONNECTED)
    {
        _plfw.monitorCount++;
        _plfw.monitors =
            _plfw_realloc(_plfw.monitors,
                          sizeof(_PLFWmonitor*) * _plfw.monitorCount);

        if (placement == _PLFW_INSERT_FIRST)
        {
            memmove(_plfw.monitors + 1,
                    _plfw.monitors,
                    ((size_t) _plfw.monitorCount - 1) * sizeof(_PLFWmonitor*));
            _plfw.monitors[0] = monitor;
        }
        else
            _plfw.monitors[_plfw.monitorCount - 1] = monitor;
    }
    else if (action == PLFW_DISCONNECTED)
    {
        int i;
        _PLFWwindow* window;

        for (window = _plfw.windowListHead;  window;  window = window->next)
        {
            if (window->monitor == monitor)
            {
                int width, height, xoff, yoff;
                _plfw.platform.getWindowSize(window, &width, &height);
                _plfw.platform.setWindowMonitor(window, NULL, 0, 0, width, height, 0);
                _plfw.platform.getWindowFrameSize(window, &xoff, &yoff, NULL, NULL);
                _plfw.platform.setWindowPos(window, xoff, yoff);
            }
        }

        for (i = 0;  i < _plfw.monitorCount;  i++)
        {
            if (_plfw.monitors[i] == monitor)
            {
                _plfw.monitorCount--;
                memmove(_plfw.monitors + i,
                        _plfw.monitors + i + 1,
                        ((size_t) _plfw.monitorCount - i) * sizeof(_PLFWmonitor*));
                break;
            }
        }
    }

    if (_plfw.callbacks.monitor)
        _plfw.callbacks.monitor((PLFWmonitor*) monitor, action);

    if (action == PLFW_DISCONNECTED)
        _plfwFreeMonitor(monitor);
}

// Notifies shared code that a full screen window has acquired or released
// a monitor
//
void _plfwInputMonitorWindow(_PLFWmonitor* monitor, _PLFWwindow* window)
{
    assert(monitor != NULL);
    monitor->window = window;
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Allocates and returns a monitor object with the specified name and dimensions
//
_PLFWmonitor* _plfwAllocMonitor(const char* name, int widthMM, int heightMM)
{
    _PLFWmonitor* monitor = _plfw_calloc(1, sizeof(_PLFWmonitor));
    monitor->widthMM = widthMM;
    monitor->heightMM = heightMM;

    strncpy(monitor->name, name, sizeof(monitor->name) - 1);

    return monitor;
}

// Frees a monitor object and any data associated with it
//
void _plfwFreeMonitor(_PLFWmonitor* monitor)
{
    if (monitor == NULL)
        return;

    _plfw.platform.freeMonitor(monitor);

    _plfwFreeGammaArrays(&monitor->originalRamp);
    _plfwFreeGammaArrays(&monitor->currentRamp);

    _plfw_free(monitor->modes);
    _plfw_free(monitor);
}

// Allocates red, green and blue value arrays of the specified size
//
void _plfwAllocGammaArrays(PLFWgammaramp* ramp, unsigned int size)
{
    ramp->red = _plfw_calloc(size, sizeof(unsigned short));
    ramp->green = _plfw_calloc(size, sizeof(unsigned short));
    ramp->blue = _plfw_calloc(size, sizeof(unsigned short));
    ramp->size = size;
}

// Frees the red, green and blue value arrays and clears the struct
//
void _plfwFreeGammaArrays(PLFWgammaramp* ramp)
{
    _plfw_free(ramp->red);
    _plfw_free(ramp->green);
    _plfw_free(ramp->blue);

    memset(ramp, 0, sizeof(PLFWgammaramp));
}

// Chooses the video mode most closely matching the desired one
//
const PLFWvidmode* _plfwChooseVideoMode(_PLFWmonitor* monitor,
                                        const PLFWvidmode* desired)
{
    int i;
    unsigned int sizeDiff, leastSizeDiff = UINT_MAX;
    unsigned int rateDiff, leastRateDiff = UINT_MAX;
    unsigned int colorDiff, leastColorDiff = UINT_MAX;
    const PLFWvidmode* current;
    const PLFWvidmode* closest = NULL;

    if (!refreshVideoModes(monitor))
        return NULL;

    for (i = 0;  i < monitor->modeCount;  i++)
    {
        current = monitor->modes + i;

        colorDiff = 0;

        if (desired->redBits != PLFW_DONT_CARE)
            colorDiff += abs(current->redBits - desired->redBits);
        if (desired->greenBits != PLFW_DONT_CARE)
            colorDiff += abs(current->greenBits - desired->greenBits);
        if (desired->blueBits != PLFW_DONT_CARE)
            colorDiff += abs(current->blueBits - desired->blueBits);

        sizeDiff = abs((current->width - desired->width) *
                       (current->width - desired->width) +
                       (current->height - desired->height) *
                       (current->height - desired->height));

        if (desired->refreshRate != PLFW_DONT_CARE)
            rateDiff = abs(current->refreshRate - desired->refreshRate);
        else
            rateDiff = UINT_MAX - current->refreshRate;

        if ((colorDiff < leastColorDiff) ||
            (colorDiff == leastColorDiff && sizeDiff < leastSizeDiff) ||
            (colorDiff == leastColorDiff && sizeDiff == leastSizeDiff && rateDiff < leastRateDiff))
        {
            closest = current;
            leastSizeDiff = sizeDiff;
            leastRateDiff = rateDiff;
            leastColorDiff = colorDiff;
        }
    }

    return closest;
}

// Performs lexical comparison between two @ref PLFWvidmode structures
//
int _plfwCompareVideoModes(const PLFWvidmode* fm, const PLFWvidmode* sm)
{
    return compareVideoModes(fm, sm);
}

// Splits a color depth into red, green and blue bit depths
//
void _plfwSplitBPP(int bpp, int* red, int* green, int* blue)
{
    int delta;

    // We assume that by 32 the user really meant 24
    if (bpp == 32)
        bpp = 24;

    // Convert "bits per pixel" to red, green & blue sizes

    *red = *green = *blue = bpp / 3;
    delta = bpp - (*red * 3);
    if (delta >= 1)
        *green = *green + 1;

    if (delta == 2)
        *red = *red + 1;
}


//////////////////////////////////////////////////////////////////////////
//////                        PLFW public API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI PLFWmonitor** plfwGetMonitors(int* count)
{
    assert(count != NULL);

    *count = 0;

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    *count = _plfw.monitorCount;
    return (PLFWmonitor**) _plfw.monitors;
}

PLFWAPI PLFWmonitor* plfwGetPrimaryMonitor(void)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!_plfw.monitorCount)
        return NULL;

    return (PLFWmonitor*) _plfw.monitors[0];
}

PLFWAPI void plfwGetMonitorPos(PLFWmonitor* handle, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    _plfw.platform.getMonitorPos(monitor, xpos, ypos);
}

PLFWAPI void plfwGetMonitorWorkarea(PLFWmonitor* handle,
                                    int* xpos, int* ypos,
                                    int* width, int* height)
{
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;
    if (width)
        *width = 0;
    if (height)
        *height = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    _plfw.platform.getMonitorWorkarea(monitor, xpos, ypos, width, height);
}

PLFWAPI void plfwGetMonitorPhysicalSize(PLFWmonitor* handle, int* widthMM, int* heightMM)
{
    if (widthMM)
        *widthMM = 0;
    if (heightMM)
        *heightMM = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    if (widthMM)
        *widthMM = monitor->widthMM;
    if (heightMM)
        *heightMM = monitor->heightMM;
}

PLFWAPI void plfwGetMonitorContentScale(PLFWmonitor* handle,
                                        float* xscale, float* yscale)
{
    if (xscale)
        *xscale = 0.f;
    if (yscale)
        *yscale = 0.f;

    _PLFW_REQUIRE_INIT();

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    _plfw.platform.getMonitorContentScale(monitor, xscale, yscale);
}

PLFWAPI const char* plfwGetMonitorName(PLFWmonitor* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    return monitor->name;
}

PLFWAPI void plfwSetMonitorUserPointer(PLFWmonitor* handle, void* pointer)
{
    _PLFW_REQUIRE_INIT();

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    monitor->userPointer = pointer;
}

PLFWAPI void* plfwGetMonitorUserPointer(PLFWmonitor* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    return monitor->userPointer;
}

PLFWAPI PLFWmonitorfun plfwSetMonitorCallback(PLFWmonitorfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);
    _PLFW_SWAP(PLFWmonitorfun, _plfw.callbacks.monitor, cbfun);
    return cbfun;
}

PLFWAPI const PLFWvidmode* plfwGetVideoModes(PLFWmonitor* handle, int* count)
{
    assert(count != NULL);

    *count = 0;

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    if (!refreshVideoModes(monitor))
        return NULL;

    *count = monitor->modeCount;
    return monitor->modes;
}

PLFWAPI const PLFWvidmode* plfwGetVideoMode(PLFWmonitor* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    if (!_plfw.platform.getVideoMode(monitor, &monitor->currentMode))
        return NULL;

    return &monitor->currentMode;
}

PLFWAPI void plfwSetGamma(PLFWmonitor* handle, float gamma)
{
    unsigned int i;
    unsigned short* values;
    PLFWgammaramp ramp;
    const PLFWgammaramp* original;

    assert(gamma > 0.f);
    assert(isfinite(gamma));

    _PLFW_REQUIRE_INIT();

    assert(handle != NULL);

    if (!isfinite(gamma) || gamma <= 0.f)
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid gamma value %f", gamma);
        return;
    }

    original = plfwGetGammaRamp(handle);
    if (!original)
        return;

    values = _plfw_calloc(original->size, sizeof(unsigned short));

    for (i = 0;  i < original->size;  i++)
    {
        float value;

        // Calculate intensity
        value = i / (float) (original->size - 1);
        // Apply gamma curve
        value = powf(value, 1.f / gamma) * 65535.f + 0.5f;
        // Clamp to value range
        value = fminf(value, 65535.f);

        values[i] = (unsigned short) value;
    }

    ramp.red = values;
    ramp.green = values;
    ramp.blue = values;
    ramp.size = original->size;

    plfwSetGammaRamp(handle, &ramp);
    _plfw_free(values);
}

PLFWAPI const PLFWgammaramp* plfwGetGammaRamp(PLFWmonitor* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    _plfwFreeGammaArrays(&monitor->currentRamp);
    if (!_plfw.platform.getGammaRamp(monitor, &monitor->currentRamp))
        return NULL;

    return &monitor->currentRamp;
}

PLFWAPI void plfwSetGammaRamp(PLFWmonitor* handle, const PLFWgammaramp* ramp)
{
    assert(ramp != NULL);
    assert(ramp->size > 0);
    assert(ramp->red != NULL);
    assert(ramp->green != NULL);
    assert(ramp->blue != NULL);

    _PLFW_REQUIRE_INIT();

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    if (ramp->size <= 0)
    {
        _plfwInputError(PLFW_INVALID_VALUE,
                        "Invalid gamma ramp size %i",
                        ramp->size);
        return;
    }

    if (!monitor->originalRamp.size)
    {
        if (!_plfw.platform.getGammaRamp(monitor, &monitor->originalRamp))
            return;
    }

    _plfw.platform.setGammaRamp(monitor, ramp);
}

