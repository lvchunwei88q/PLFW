//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2018 Camilla Löwy <elmindreda@glfw.org>
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>


// NOTE: The global variables below comprise all mutable global data in PLFW
//       Any other mutable global variable is a bug

// This contains all mutable state shared between compilation units of PLFW
//
_PLFWlibrary _plfw = { PLFW_FALSE };

// These are outside of _plfw so they can be used before initialization and
// after termination without special handling when _plfw is cleared to zero
//
static _PLFWerror _plfwMainThreadError;
static PLFWerrorfun _plfwErrorCallback;
static PLFWallocator _plfwInitAllocator;
static _PLFWinitconfig _plfwInitHints =
{
    .hatButtons = true,
    .platformID = PLFW_ANY_PLATFORM,
    .wl =
    {
        .libdecorMode = PLFW_WAYLAND_PREFER_LIBDECOR
    },
};

// The allocation function used when no custom allocator is set
//
static void* defaultAllocate(size_t size, void* user)
{
    return malloc(size);
}

// The deallocation function used when no custom allocator is set
//
static void defaultDeallocate(void* block, void* user)
{
    free(block);
}

// The reallocation function used when no custom allocator is set
//
static void* defaultReallocate(void* block, size_t size, void* user)
{
    return realloc(block, size);
}

// Terminate the library
//
static void terminate(void)
{
    int i;

    memset(&_plfw.callbacks, 0, sizeof(_plfw.callbacks));

    while (_plfw.windowListHead)
        plfwDestroyWindow((PLFWwindow*) _plfw.windowListHead);

    while (_plfw.cursorListHead)
        plfwDestroyCursor((PLFWcursor*) _plfw.cursorListHead);

    for (i = 0;  i < _plfw.monitorCount;  i++)
    {
        _PLFWmonitor* monitor = _plfw.monitors[i];
        if (monitor->originalRamp.size)
            _plfw.platform.setGammaRamp(monitor, &monitor->originalRamp);
        _plfwFreeMonitor(monitor);
    }

    _plfw_free(_plfw.monitors);
    _plfw.monitors = NULL;
    _plfw.monitorCount = 0;

    _plfw_free(_plfw.mappings);
    _plfw.mappings = NULL;
    _plfw.mappingCount = 0;

    _plfw.platform.terminateJoysticks();
    _plfw.platform.terminate();

    _plfw.initialized = PLFW_FALSE;

    while (_plfw.errorListHead)
    {
        _PLFWerror* error = _plfw.errorListHead;
        _plfw.errorListHead = error->next;
        _plfw_free(error);
    }

    _plfwPlatformDestroyTls(&_plfw.errorSlot);
    _plfwPlatformDestroyMutex(&_plfw.errorLock);

    memset(&_plfw, 0, sizeof(_plfw));
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Encode a Unicode code point to a UTF-8 stream
// Based on cutef8 by Jeff Bezanson (Public Domain)
//
size_t _plfwEncodeUTF8(char* s, uint32_t codepoint)
{
    size_t count = 0;

    if (codepoint < 0x80)
        s[count++] = (char) codepoint;
    else if (codepoint < 0x800)
    {
        s[count++] = (codepoint >> 6) | 0xc0;
        s[count++] = (codepoint & 0x3f) | 0x80;
    }
    else if (codepoint < 0x10000)
    {
        s[count++] = (codepoint >> 12) | 0xe0;
        s[count++] = ((codepoint >> 6) & 0x3f) | 0x80;
        s[count++] = (codepoint & 0x3f) | 0x80;
    }
    else if (codepoint < 0x110000)
    {
        s[count++] = (codepoint >> 18) | 0xf0;
        s[count++] = ((codepoint >> 12) & 0x3f) | 0x80;
        s[count++] = ((codepoint >> 6) & 0x3f) | 0x80;
        s[count++] = (codepoint & 0x3f) | 0x80;
    }

    return count;
}

// Splits and translates a text/uri-list into separate file paths
// NOTE: This function destroys the provided string
//
char** _plfwParseUriList(char* text, int* count)
{
    const char* prefix = "file://";
    char** paths = NULL;
    char* line;

    *count = 0;

    while ((line = strtok(text, "\r\n")))
    {
        char* path;

        text = NULL;

        if (line[0] == '#')
            continue;

        if (strncmp(line, prefix, strlen(prefix)) == 0)
        {
            line += strlen(prefix);
            // TODO: Validate hostname
            while (*line != '/')
                line++;
        }

        (*count)++;

        path = _plfw_calloc(strlen(line) + 1, 1);
        paths = _plfw_realloc(paths, *count * sizeof(char*));
        paths[*count - 1] = path;

        while (*line)
        {
            if (line[0] == '%' && line[1] && line[2])
            {
                const char digits[3] = { line[1], line[2], '\0' };
                *path = (char) strtol(digits, NULL, 16);
                line += 2;
            }
            else
                *path = *line;

            path++;
            line++;
        }
    }

    return paths;
}

char* _plfw_strdup(const char* source)
{
    const size_t length = strlen(source);
    char* result = _plfw_calloc(length + 1, 1);
    strcpy(result, source);
    return result;
}

int _plfw_min(int a, int b)
{
    return a < b ? a : b;
}

int _plfw_max(int a, int b)
{
    return a > b ? a : b;
}

void* _plfw_calloc(size_t count, size_t size)
{
    if (count && size)
    {
        void* block;

        if (count > SIZE_MAX / size)
        {
            _plfwInputError(PLFW_INVALID_VALUE, "Allocation size overflow");
            return NULL;
        }

        block = _plfw.allocator.allocate(count * size, _plfw.allocator.user);
        if (block)
            return memset(block, 0, count * size);
        else
        {
            _plfwInputError(PLFW_OUT_OF_MEMORY, NULL);
            return NULL;
        }
    }
    else
        return NULL;
}

void* _plfw_realloc(void* block, size_t size)
{
    if (block && size)
    {
        void* resized = _plfw.allocator.reallocate(block, size, _plfw.allocator.user);
        if (resized)
            return resized;
        else
        {
            _plfwInputError(PLFW_OUT_OF_MEMORY, NULL);
            return NULL;
        }
    }
    else if (block)
    {
        _plfw_free(block);
        return NULL;
    }
    else
        return _plfw_calloc(1, size);
}

void _plfw_free(void* block)
{
    if (block)
        _plfw.allocator.deallocate(block, _plfw.allocator.user);
}


//////////////////////////////////////////////////////////////////////////
//////                         PLFW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code of an error
//
void _plfwInputError(int code, const char* format, ...)
{
    _PLFWerror* error;
    char description[_PLFW_MESSAGE_SIZE];

    if (format)
    {
        va_list vl;

        va_start(vl, format);
        vsnprintf(description, sizeof(description), format, vl);
        va_end(vl);

        description[sizeof(description) - 1] = '\0';
    }
    else
    {
        if (code == PLFW_NOT_INITIALIZED)
            strcpy(description, "The PLFW library is not initialized");
        else if (code == PLFW_NO_CURRENT_CONTEXT)
            strcpy(description, "There is no current context");
        else if (code == PLFW_INVALID_ENUM)
            strcpy(description, "Invalid argument for enum parameter");
        else if (code == PLFW_INVALID_VALUE)
            strcpy(description, "Invalid value for parameter");
        else if (code == PLFW_OUT_OF_MEMORY)
            strcpy(description, "Out of memory");
        else if (code == PLFW_API_UNAVAILABLE)
            strcpy(description, "The requested API is unavailable");
        else if (code == PLFW_VERSION_UNAVAILABLE)
            strcpy(description, "The requested API version is unavailable");
        else if (code == PLFW_PLATFORM_ERROR)
            strcpy(description, "A platform-specific error occurred");
        else if (code == PLFW_FORMAT_UNAVAILABLE)
            strcpy(description, "The requested format is unavailable");
        else if (code == PLFW_NO_WINDOW_CONTEXT)
            strcpy(description, "The specified window has no context");
        else if (code == PLFW_CURSOR_UNAVAILABLE)
            strcpy(description, "The specified cursor shape is unavailable");
        else if (code == PLFW_FEATURE_UNAVAILABLE)
            strcpy(description, "The requested feature cannot be implemented for this platform");
        else if (code == PLFW_FEATURE_UNIMPLEMENTED)
            strcpy(description, "The requested feature has not yet been implemented for this platform");
        else if (code == PLFW_PLATFORM_UNAVAILABLE)
            strcpy(description, "The requested platform is unavailable");
        else
            strcpy(description, "ERROR: UNKNOWN PLFW ERROR");
    }

    if (_plfw.initialized)
    {
        error = _plfwPlatformGetTls(&_plfw.errorSlot);
        if (!error)
        {
            error = _plfw_calloc(1, sizeof(_PLFWerror));
            _plfwPlatformSetTls(&_plfw.errorSlot, error);
            _plfwPlatformLockMutex(&_plfw.errorLock);
            error->next = _plfw.errorListHead;
            _plfw.errorListHead = error;
            _plfwPlatformUnlockMutex(&_plfw.errorLock);
        }
    }
    else
        error = &_plfwMainThreadError;

    error->code = code;
    strcpy(error->description, description);

    if (_plfwErrorCallback)
        _plfwErrorCallback(code, description);
}


//////////////////////////////////////////////////////////////////////////
//////                        PLFW public API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI int plfwInit(void)
{
    if (_plfw.initialized)
        return PLFW_TRUE;

    memset(&_plfw, 0, sizeof(_plfw));
    _plfw.hints.init = _plfwInitHints;

    _plfw.allocator = _plfwInitAllocator;
    if (!_plfw.allocator.allocate)
    {
        _plfw.allocator.allocate   = defaultAllocate;
        _plfw.allocator.reallocate = defaultReallocate;
        _plfw.allocator.deallocate = defaultDeallocate;
    }

    if (!_plfwSelectPlatform(_plfw.hints.init.platformID, &_plfw.platform))
        return PLFW_FALSE;

    if (!_plfw.platform.init())
    {
        terminate();
        return PLFW_FALSE;
    }

    if (!_plfwPlatformCreateMutex(&_plfw.errorLock) ||
        !_plfwPlatformCreateTls(&_plfw.errorSlot))
    {
        terminate();
        return PLFW_FALSE;
    }

    _plfwPlatformSetTls(&_plfw.errorSlot, &_plfwMainThreadError);

    _plfwInitGamepadMappings();

    _plfwPlatformInitTimer();
    _plfw.timer.offset = _plfwPlatformGetTimerValue();

    _plfw.initialized = PLFW_TRUE;

    plfwDefaultWindowHints();
    return PLFW_TRUE;
}

PLFWAPI void plfwTerminate(void)
{
    if (!_plfw.initialized)
        return;

    terminate();
}

PLFWAPI void plfwInitHint(int hint, int value)
{
    switch (hint)
    {
        case PLFW_JOYSTICK_HAT_BUTTONS:
            _plfwInitHints.hatButtons = value;
            return;
        case PLFW_PLATFORM:
            _plfwInitHints.platformID = value;
            return;
        case PLFW_WAYLAND_LIBDECOR:
            _plfwInitHints.wl.libdecorMode = value;
            return;
    }

    _plfwInputError(PLFW_INVALID_ENUM,
                    "Invalid init hint 0x%08X", hint);
}

PLFWAPI void plfwInitAllocator(const PLFWallocator* allocator)
{
    if (allocator)
    {
        if (allocator->allocate && allocator->reallocate && allocator->deallocate)
            _plfwInitAllocator = *allocator;
        else
            _plfwInputError(PLFW_INVALID_VALUE, "Missing function in allocator");
    }
    else
        memset(&_plfwInitAllocator, 0, sizeof(PLFWallocator));
}

PLFWAPI int plfwGetError(const char** description)
{
    _PLFWerror* error;
    int code = PLFW_NO_ERROR;

    if (description)
        *description = NULL;

    if (_plfw.initialized)
        error = _plfwPlatformGetTls(&_plfw.errorSlot);
    else
        error = &_plfwMainThreadError;

    if (error)
    {
        code = error->code;
        error->code = PLFW_NO_ERROR;
        if (description && code)
            *description = error->description;
    }

    return code;
}

PLFWAPI PLFWerrorfun plfwSetErrorCallback(PLFWerrorfun cbfun)
{
    _PLFW_SWAP(PLFWerrorfun, _plfwErrorCallback, cbfun);
    return cbfun;
}

