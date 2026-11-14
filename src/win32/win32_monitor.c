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
#include <string.h>
#include <limits.h>
#include <wchar.h>
#include <assert.h>


// Callback for EnumDisplayMonitors in createMonitor
//
static BOOL CALLBACK monitorCallback(HMONITOR handle,
                                     HDC dc,
                                     RECT* rect,
                                     LPARAM data)
{
    MONITORINFOEXW mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);

    if (GetMonitorInfoW(handle, (MONITORINFO*) &mi))
    {
        _PLFWmonitor* monitor = (_PLFWmonitor*) data;
        if (wcscmp(mi.szDevice, monitor->win32.adapterName) == 0)
            monitor->win32.handle = handle;
    }

    return TRUE;
}

// Create monitor from an adapter and (optionally) a display
//
static _PLFWmonitor* createMonitor(DISPLAY_DEVICEW* adapter,
                                   DISPLAY_DEVICEW* display)
{
    _PLFWmonitor* monitor;
    int widthMM, heightMM;
    char* name;
    HDC dc;
    DEVMODEW dm;
    RECT rect;

    if (display)
        name = _plfwCreateUTF8FromWideStringWin32(display->DeviceString);
    else
        name = _plfwCreateUTF8FromWideStringWin32(adapter->DeviceString);
    if (!name)
        return NULL;

    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    EnumDisplaySettingsW(adapter->DeviceName, ENUM_CURRENT_SETTINGS, &dm);

    dc = CreateDCW(L"DISPLAY", adapter->DeviceName, NULL, NULL);

    if (IsWindows8Point1OrGreater())
    {
        widthMM  = GetDeviceCaps(dc, HORZSIZE);
        heightMM = GetDeviceCaps(dc, VERTSIZE);
    }
    else
    {
        widthMM  = (int) (dm.dmPelsWidth * 25.4f / GetDeviceCaps(dc, LOGPIXELSX));
        heightMM = (int) (dm.dmPelsHeight * 25.4f / GetDeviceCaps(dc, LOGPIXELSY));
    }

    DeleteDC(dc);

    monitor = _plfwAllocMonitor(name, widthMM, heightMM);
    _plfw_free(name);

    if (adapter->StateFlags & DISPLAY_DEVICE_MODESPRUNED)
        monitor->win32.modesPruned = PLFW_TRUE;

    wcscpy(monitor->win32.adapterName, adapter->DeviceName);
    WideCharToMultiByte(CP_UTF8, 0,
                        adapter->DeviceName, -1,
                        monitor->win32.publicAdapterName,
                        sizeof(monitor->win32.publicAdapterName),
                        NULL, NULL);

    if (display)
    {
        wcscpy(monitor->win32.displayName, display->DeviceName);
        WideCharToMultiByte(CP_UTF8, 0,
                            display->DeviceName, -1,
                            monitor->win32.publicDisplayName,
                            sizeof(monitor->win32.publicDisplayName),
                            NULL, NULL);
    }

    rect.left   = dm.dmPosition.x;
    rect.top    = dm.dmPosition.y;
    rect.right  = dm.dmPosition.x + dm.dmPelsWidth;
    rect.bottom = dm.dmPosition.y + dm.dmPelsHeight;

    EnumDisplayMonitors(NULL, &rect, monitorCallback, (LPARAM) monitor);
    return monitor;
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Poll for changes in the set of connected monitors
//
void _plfwPollMonitorsWin32(void)
{
    int i, disconnectedCount;
    _PLFWmonitor** disconnected = NULL;
    DWORD adapterIndex, displayIndex;
    DISPLAY_DEVICEW adapter, display;
    _PLFWmonitor* monitor;

    disconnectedCount = _plfw.monitorCount;
    if (disconnectedCount)
    {
        disconnected = _plfw_calloc(_plfw.monitorCount, sizeof(_PLFWmonitor*));
        memcpy(disconnected,
               _plfw.monitors,
               _plfw.monitorCount * sizeof(_PLFWmonitor*));
    }

    for (adapterIndex = 0;  ;  adapterIndex++)
    {
        int type = _PLFW_INSERT_LAST;

        ZeroMemory(&adapter, sizeof(adapter));
        adapter.cb = sizeof(adapter);

        if (!EnumDisplayDevicesW(NULL, adapterIndex, &adapter, 0))
            break;

        if (!(adapter.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;

        if (adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            type = _PLFW_INSERT_FIRST;

        for (displayIndex = 0;  ;  displayIndex++)
        {
            ZeroMemory(&display, sizeof(display));
            display.cb = sizeof(display);

            if (!EnumDisplayDevicesW(adapter.DeviceName, displayIndex, &display, 0))
                break;

            if (!(display.StateFlags & DISPLAY_DEVICE_ACTIVE))
                continue;

            for (i = 0;  i < disconnectedCount;  i++)
            {
                if (disconnected[i] &&
                    wcscmp(disconnected[i]->win32.displayName,
                           display.DeviceName) == 0)
                {
                    disconnected[i] = NULL;
                    // handle may have changed, update
                    EnumDisplayMonitors(NULL, NULL, monitorCallback, (LPARAM) _plfw.monitors[i]);
                    break;
                }
            }

            if (i < disconnectedCount)
                continue;

            monitor = createMonitor(&adapter, &display);
            if (!monitor)
            {
                _plfw_free(disconnected);
                return;
            }

            _plfwInputMonitor(monitor, PLFW_CONNECTED, type);

            type = _PLFW_INSERT_LAST;
        }

        // HACK: If an active adapter does not have any display devices
        //       (as sometimes happens), add it directly as a monitor
        if (displayIndex == 0)
        {
            for (i = 0;  i < disconnectedCount;  i++)
            {
                if (disconnected[i] &&
                    wcscmp(disconnected[i]->win32.adapterName,
                           adapter.DeviceName) == 0)
                {
                    disconnected[i] = NULL;
                    break;
                }
            }

            if (i < disconnectedCount)
                continue;

            monitor = createMonitor(&adapter, NULL);
            if (!monitor)
            {
                _plfw_free(disconnected);
                return;
            }

            _plfwInputMonitor(monitor, PLFW_CONNECTED, type);
        }
    }

    for (i = 0;  i < disconnectedCount;  i++)
    {
        if (disconnected[i])
            _plfwInputMonitor(disconnected[i], PLFW_DISCONNECTED, 0);
    }

    _plfw_free(disconnected);
}

// Change the current video mode
//
void _plfwSetVideoModeWin32(_PLFWmonitor* monitor, const PLFWvidmode* desired)
{
    PLFWvidmode current;
    const PLFWvidmode* best;
    DEVMODEW dm;
    LONG result;

    best = _plfwChooseVideoMode(monitor, desired);
    _plfwGetVideoModeWin32(monitor, &current);
    if (_plfwCompareVideoModes(&current, best) == 0)
        return;

    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);
    dm.dmFields           = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
                            DM_DISPLAYFREQUENCY;
    dm.dmPelsWidth        = best->width;
    dm.dmPelsHeight       = best->height;
    dm.dmBitsPerPel       = best->redBits + best->greenBits + best->blueBits;
    dm.dmDisplayFrequency = best->refreshRate;

    if (dm.dmBitsPerPel < 15 || dm.dmBitsPerPel >= 24)
        dm.dmBitsPerPel = 32;

    result = ChangeDisplaySettingsExW(monitor->win32.adapterName,
                                      &dm,
                                      NULL,
                                      CDS_FULLSCREEN,
                                      NULL);
    if (result == DISP_CHANGE_SUCCESSFUL)
        monitor->win32.modeChanged = PLFW_TRUE;
    else
    {
        const char* description = "Unknown error";

        if (result == DISP_CHANGE_BADDUALVIEW)
            description = "The system uses DualView";
        else if (result == DISP_CHANGE_BADFLAGS)
            description = "Invalid flags";
        else if (result == DISP_CHANGE_BADMODE)
            description = "Graphics mode not supported";
        else if (result == DISP_CHANGE_BADPARAM)
            description = "Invalid parameter";
        else if (result == DISP_CHANGE_FAILED)
            description = "Graphics mode failed";
        else if (result == DISP_CHANGE_NOTUPDATED)
            description = "Failed to write to registry";
        else if (result == DISP_CHANGE_RESTART)
            description = "Computer restart required";

        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Win32: Failed to set video mode: %s",
                        description);
    }
}

// Restore the previously saved (original) video mode
//
void _plfwRestoreVideoModeWin32(_PLFWmonitor* monitor)
{
    if (monitor->win32.modeChanged)
    {
        ChangeDisplaySettingsExW(monitor->win32.adapterName,
                                 NULL, NULL, CDS_FULLSCREEN, NULL);
        monitor->win32.modeChanged = PLFW_FALSE;
    }
}

void _plfwGetHMONITORContentScaleWin32(HMONITOR handle, float* xscale, float* yscale)
{
    UINT xdpi, ydpi;

    if (xscale)
        *xscale = 0.f;
    if (yscale)
        *yscale = 0.f;

    if (IsWindows8Point1OrGreater())
    {
        if (GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &xdpi, &ydpi) != S_OK)
        {
            _plfwInputError(PLFW_PLATFORM_ERROR, "Win32: Failed to query monitor DPI");
            return;
        }
    }
    else
    {
        const HDC dc = GetDC(NULL);
        xdpi = GetDeviceCaps(dc, LOGPIXELSX);
        ydpi = GetDeviceCaps(dc, LOGPIXELSY);
        ReleaseDC(NULL, dc);
    }

    if (xscale)
        *xscale = xdpi / (float) USER_DEFAULT_SCREEN_DPI;
    if (yscale)
        *yscale = ydpi / (float) USER_DEFAULT_SCREEN_DPI;
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _plfwFreeMonitorWin32(_PLFWmonitor* monitor)
{
}

void _plfwGetMonitorPosWin32(_PLFWmonitor* monitor, int* xpos, int* ypos)
{
    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);

    EnumDisplaySettingsExW(monitor->win32.adapterName,
                           ENUM_CURRENT_SETTINGS,
                           &dm,
                           EDS_ROTATEDMODE);

    if (xpos)
        *xpos = dm.dmPosition.x;
    if (ypos)
        *ypos = dm.dmPosition.y;
}

void _plfwGetMonitorContentScaleWin32(_PLFWmonitor* monitor,
                                      float* xscale, float* yscale)
{
    _plfwGetHMONITORContentScaleWin32(monitor->win32.handle, xscale, yscale);
}

void _plfwGetMonitorWorkareaWin32(_PLFWmonitor* monitor,
                                  int* xpos, int* ypos,
                                  int* width, int* height)
{
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(monitor->win32.handle, &mi);

    if (xpos)
        *xpos = mi.rcWork.left;
    if (ypos)
        *ypos = mi.rcWork.top;
    if (width)
        *width = mi.rcWork.right - mi.rcWork.left;
    if (height)
        *height = mi.rcWork.bottom - mi.rcWork.top;
}

PLFWvidmode* _plfwGetVideoModesWin32(_PLFWmonitor* monitor, int* count)
{
    int modeIndex = 0, size = 0;
    PLFWvidmode* result = NULL;

    *count = 0;

    for (;;)
    {
        int i;
        PLFWvidmode mode;
        DEVMODEW dm;

        ZeroMemory(&dm, sizeof(dm));
        dm.dmSize = sizeof(dm);

        if (!EnumDisplaySettingsW(monitor->win32.adapterName, modeIndex, &dm))
            break;

        modeIndex++;

        // Skip modes with less than 15 BPP
        if (dm.dmBitsPerPel < 15)
            continue;

        mode.width  = dm.dmPelsWidth;
        mode.height = dm.dmPelsHeight;
        mode.refreshRate = dm.dmDisplayFrequency;
        _plfwSplitBPP(dm.dmBitsPerPel,
                      &mode.redBits,
                      &mode.greenBits,
                      &mode.blueBits);

        for (i = 0;  i < *count;  i++)
        {
            if (_plfwCompareVideoModes(result + i, &mode) == 0)
                break;
        }

        // Skip duplicate modes
        if (i < *count)
            continue;

        if (monitor->win32.modesPruned)
        {
            // Skip modes not supported by the connected displays
            if (ChangeDisplaySettingsExW(monitor->win32.adapterName,
                                         &dm,
                                         NULL,
                                         CDS_TEST,
                                         NULL) != DISP_CHANGE_SUCCESSFUL)
            {
                continue;
            }
        }

        if (*count == size)
        {
            size += 128;
            result = (PLFWvidmode*) _plfw_realloc(result, size * sizeof(PLFWvidmode));
        }

        (*count)++;
        result[*count - 1] = mode;
    }

    if (!*count)
    {
        // HACK: Report the current mode if no valid modes were found
        result = _plfw_calloc(1, sizeof(PLFWvidmode));
        _plfwGetVideoModeWin32(monitor, result);
        *count = 1;
    }

    return result;
}

PLFWbool _plfwGetVideoModeWin32(_PLFWmonitor* monitor, PLFWvidmode* mode)
{
    DEVMODEW dm;
    ZeroMemory(&dm, sizeof(dm));
    dm.dmSize = sizeof(dm);

    if (!EnumDisplaySettingsW(monitor->win32.adapterName, ENUM_CURRENT_SETTINGS, &dm))
    {
        _plfwInputError(PLFW_PLATFORM_ERROR, "Win32: Failed to query display settings");
        return PLFW_FALSE;
    }

    mode->width  = dm.dmPelsWidth;
    mode->height = dm.dmPelsHeight;
    mode->refreshRate = dm.dmDisplayFrequency;
    _plfwSplitBPP(dm.dmBitsPerPel,
                  &mode->redBits,
                  &mode->greenBits,
                  &mode->blueBits);

    return PLFW_TRUE;
}

PLFWbool _plfwGetGammaRampWin32(_PLFWmonitor* monitor, PLFWgammaramp* ramp)
{
    HDC dc;
    WORD values[3][256];

    dc = CreateDCW(L"DISPLAY", monitor->win32.adapterName, NULL, NULL);
    GetDeviceGammaRamp(dc, values);
    DeleteDC(dc);

    _plfwAllocGammaArrays(ramp, 256);

    memcpy(ramp->red,   values[0], sizeof(values[0]));
    memcpy(ramp->green, values[1], sizeof(values[1]));
    memcpy(ramp->blue,  values[2], sizeof(values[2]));

    return PLFW_TRUE;
}

void _plfwSetGammaRampWin32(_PLFWmonitor* monitor, const PLFWgammaramp* ramp)
{
    HDC dc;
    WORD values[3][256];

    if (ramp->size != 256)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Win32: Gamma ramp size must be 256");
        return;
    }

    memcpy(values[0], ramp->red,   sizeof(values[0]));
    memcpy(values[1], ramp->green, sizeof(values[1]));
    memcpy(values[2], ramp->blue,  sizeof(values[2]));

    dc = CreateDCW(L"DISPLAY", monitor->win32.adapterName, NULL, NULL);
    SetDeviceGammaRamp(dc, values);
    DeleteDC(dc);
}


//////////////////////////////////////////////////////////////////////////
//////                        PLFW native API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI const char* plfwGetWin32Adapter(PLFWmonitor* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_plfw.platform.platformID != PLFW_PLATFORM_WIN32)
    {
        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE, "Win32: Platform not initialized");
        return NULL;
    }

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    return monitor->win32.publicAdapterName;
}

PLFWAPI const char* plfwGetWin32Monitor(PLFWmonitor* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_plfw.platform.platformID != PLFW_PLATFORM_WIN32)
    {
        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE, "Win32: Platform not initialized");
        return NULL;
    }

    _PLFWmonitor* monitor = (_PLFWmonitor*) handle;
    assert(monitor != NULL);

    return monitor->win32.publicDisplayName;
}

#endif // _PLFW_WIN32

