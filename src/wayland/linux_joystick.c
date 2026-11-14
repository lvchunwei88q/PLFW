//========================================================================
// PLFW 3.5 Linux - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2017 Camilla Löwy <elmindreda@glfw.org>
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

#if defined(PLFW_BUILD_LINUX_JOYSTICK)

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef SYN_DROPPED // < v2.6.39 kernel headers
// Workaround for CentOS-6, which is supported till 2020-11-30, but still on v2.6.32
#define SYN_DROPPED 3
#endif

// Apply an EV_KEY event to the specified joystick
//
static void handleKeyEvent(_PLFWjoystick* js, int code, int value)
{
    _plfwInputJoystickButton(js,
                             js->linjs.keyMap[code - BTN_MISC],
                             value ? PLFW_PRESS : PLFW_RELEASE);
}

// Apply an EV_ABS event to the specified joystick
//
static void handleAbsEvent(_PLFWjoystick* js, int code, int value)
{
    const int index = js->linjs.absMap[code];

    if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
    {
        static const char stateMap[3][3] =
        {
            { PLFW_HAT_CENTERED, PLFW_HAT_UP,       PLFW_HAT_DOWN },
            { PLFW_HAT_LEFT,     PLFW_HAT_LEFT_UP,  PLFW_HAT_LEFT_DOWN },
            { PLFW_HAT_RIGHT,    PLFW_HAT_RIGHT_UP, PLFW_HAT_RIGHT_DOWN },
        };

        const int hat = (code - ABS_HAT0X) / 2;
        const int axis = (code - ABS_HAT0X) % 2;
        int* state = js->linjs.hats[hat];

        // NOTE: Looking at several input drivers, it seems all hat events use
        //       -1 for left / up, 0 for centered and 1 for right / down
        if (value == 0)
            state[axis] = 0;
        else if (value < 0)
            state[axis] = 1;
        else if (value > 0)
            state[axis] = 2;

        _plfwInputJoystickHat(js, index, stateMap[state[0]][state[1]]);
    }
    else
    {
        const struct input_absinfo* info = &js->linjs.absInfo[code];
        float normalized = value;

        const int range = info->maximum - info->minimum;
        if (range)
        {
            // Normalize to 0.0 -> 1.0
            normalized = (normalized - info->minimum) / range;
            // Normalize to -1.0 -> 1.0
            normalized = normalized * 2.0f - 1.0f;
        }

        _plfwInputJoystickAxis(js, index, normalized);
    }
}

// Poll state of absolute axes
//
static void pollAbsState(_PLFWjoystick* js)
{
    for (int code = 0;  code < ABS_CNT;  code++)
    {
        if (js->linjs.absMap[code] < 0)
            continue;

        struct input_absinfo* info = &js->linjs.absInfo[code];

        if (ioctl(js->linjs.fd, EVIOCGABS(code), info) < 0)
            continue;

        handleAbsEvent(js, code, info->value);
    }
}

#define isBitSet(bit, arr) (arr[(bit) / 8] & (1 << ((bit) % 8)))

// Attempt to open the specified joystick device
//
static PLFWbool openJoystickDevice(const char* path)
{
    for (int jid = 0;  jid <= PLFW_JOYSTICK_LAST;  jid++)
    {
        if (!_plfw.joysticks[jid].connected)
            continue;
        if (strcmp(_plfw.joysticks[jid].linjs.path, path) == 0)
            return PLFW_FALSE;
    }

    _PLFWjoystickLinux linjs = {0};
    linjs.fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (linjs.fd == -1)
        return PLFW_FALSE;

    char evBits[(EV_CNT + 7) / 8] = {0};
    char keyBits[(KEY_CNT + 7) / 8] = {0};
    char absBits[(ABS_CNT + 7) / 8] = {0};
    struct input_id id;

    if (ioctl(linjs.fd, EVIOCGBIT(0, sizeof(evBits)), evBits) < 0 ||
        ioctl(linjs.fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0 ||
        ioctl(linjs.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0 ||
        ioctl(linjs.fd, EVIOCGID, &id) < 0)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Linux: Failed to query input device: %s",
                        strerror(errno));
        close(linjs.fd);
        return PLFW_FALSE;
    }

    // Ensure this device supports the events expected of a joystick
    if (!isBitSet(EV_ABS, evBits))
    {
        close(linjs.fd);
        return PLFW_FALSE;
    }

    char name[256] = "";

    if (ioctl(linjs.fd, EVIOCGNAME(sizeof(name)), name) < 0)
        strncpy(name, "Unknown", sizeof(name));

    char guid[33] = "";

    // Generate a joystick GUID that matches the SDL 2.0.5+ one
    if (id.vendor && id.product && id.version)
    {
        sprintf(guid, "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000",
                id.bustype & 0xff, id.bustype >> 8,
                id.vendor & 0xff,  id.vendor >> 8,
                id.product & 0xff, id.product >> 8,
                id.version & 0xff, id.version >> 8);
    }
    else
    {
        sprintf(guid, "%02x%02x0000%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x00",
                id.bustype & 0xff, id.bustype >> 8,
                name[0], name[1], name[2], name[3],
                name[4], name[5], name[6], name[7],
                name[8], name[9], name[10]);
    }

    int axisCount = 0, buttonCount = 0, hatCount = 0;

    for (int code = BTN_MISC;  code < KEY_CNT;  code++)
    {
        if (!isBitSet(code, keyBits))
            continue;

        linjs.keyMap[code - BTN_MISC] = buttonCount;
        buttonCount++;
    }

    for (int code = 0;  code < ABS_CNT;  code++)
    {
        linjs.absMap[code] = -1;
        if (!isBitSet(code, absBits))
            continue;

        if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
        {
            linjs.absMap[code] = hatCount;
            hatCount++;
            // Skip the Y axis
            code++;
        }
        else
        {
            if (ioctl(linjs.fd, EVIOCGABS(code), &linjs.absInfo[code]) < 0)
                continue;

            linjs.absMap[code] = axisCount;
            axisCount++;
        }
    }

    _PLFWjoystick* js =
        _plfwAllocJoystick(name, guid, axisCount, buttonCount, hatCount);
    if (!js)
    {
        close(linjs.fd);
        return PLFW_FALSE;
    }

    strncpy(linjs.path, path, sizeof(linjs.path) - 1);
    memcpy(&js->linjs, &linjs, sizeof(linjs));

    pollAbsState(js);

    _plfwInputJoystick(js, PLFW_CONNECTED);
    return PLFW_TRUE;
}

#undef isBitSet

// Frees all resources associated with the specified joystick
//
static void closeJoystick(_PLFWjoystick* js)
{
    _plfwInputJoystick(js, PLFW_DISCONNECTED);
    close(js->linjs.fd);
    _plfwFreeJoystick(js);
}

// Lexically compare joysticks by name; used by qsort
//
static int compareJoysticks(const void* fp, const void* sp)
{
    const _PLFWjoystick* fj = fp;
    const _PLFWjoystick* sj = sp;
    return strcmp(fj->linjs.path, sj->linjs.path);
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

void _plfwDetectJoystickConnectionLinux(void)
{
    if (_plfw.linjs.inotify <= 0)
        return;

    ssize_t offset = 0;
    char buffer[16384];
    const ssize_t size = read(_plfw.linjs.inotify, buffer, sizeof(buffer));

    while (size > offset)
    {
        regmatch_t match;
        const struct inotify_event* e = (struct inotify_event*) (buffer + offset);

        offset += sizeof(struct inotify_event) + e->len;

        if (regexec(&_plfw.linjs.regex, e->name, 1, &match, 0) != 0)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "/dev/input/%s", e->name);

        if (e->mask & (IN_CREATE | IN_ATTRIB))
            openJoystickDevice(path);
        else if (e->mask & IN_DELETE)
        {
            for (int jid = 0;  jid <= PLFW_JOYSTICK_LAST;  jid++)
            {
                if (strcmp(_plfw.joysticks[jid].linjs.path, path) == 0)
                {
                    closeJoystick(_plfw.joysticks + jid);
                    break;
                }
            }
        }
    }
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwInitJoysticksLinux(void)
{
    const char* dirname = "/dev/input";

    _plfw.linjs.inotify = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (_plfw.linjs.inotify > 0)
    {
        // HACK: Register for IN_ATTRIB to get notified when udev is done
        //       This works well in practice but the true way is libudev

        _plfw.linjs.watch = inotify_add_watch(_plfw.linjs.inotify,
                                              dirname,
                                              IN_CREATE | IN_ATTRIB | IN_DELETE);
    }

    // Continue without device connection notifications if inotify fails

    _plfw.linjs.regexCompiled = (regcomp(&_plfw.linjs.regex, "^event[0-9]\\+$", 0) == 0);
    if (!_plfw.linjs.regexCompiled)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR, "Linux: Failed to compile regex");
        return PLFW_FALSE;
    }

    int count = 0;

    DIR* dir = opendir(dirname);
    if (dir)
    {
        struct dirent* entry;

        while ((entry = readdir(dir)))
        {
            regmatch_t match;

            if (regexec(&_plfw.linjs.regex, entry->d_name, 1, &match, 0) != 0)
                continue;

            char path[PATH_MAX];

            snprintf(path, sizeof(path), "%s/%s", dirname, entry->d_name);

            if (openJoystickDevice(path))
                count++;
        }

        closedir(dir);
    }

    // Continue with no joysticks if enumeration fails

    qsort(_plfw.joysticks, count, sizeof(_PLFWjoystick), compareJoysticks);
    return PLFW_TRUE;
}

void _plfwTerminateJoysticksLinux(void)
{
    for (int jid = 0;  jid <= PLFW_JOYSTICK_LAST;  jid++)
    {
        _PLFWjoystick* js = _plfw.joysticks + jid;
        if (js->connected)
            closeJoystick(js);
    }

    if (_plfw.linjs.inotify > 0)
    {
        if (_plfw.linjs.watch > 0)
            inotify_rm_watch(_plfw.linjs.inotify, _plfw.linjs.watch);

        close(_plfw.linjs.inotify);
    }

    if (_plfw.linjs.regexCompiled)
        regfree(&_plfw.linjs.regex);
}

PLFWbool _plfwPollJoystickLinux(_PLFWjoystick* js, int mode)
{
    // Read all queued events (non-blocking)
    for (;;)
    {
        struct input_event e;

        errno = 0;
        if (read(js->linjs.fd, &e, sizeof(e)) < 0)
        {
            // Reset the joystick slot if the device was disconnected
            if (errno == ENODEV)
                closeJoystick(js);

            break;
        }

        if (e.type == EV_SYN)
        {
            if (e.code == SYN_DROPPED)
                _plfw.linjs.dropped = PLFW_TRUE;
            else if (e.code == SYN_REPORT)
            {
                _plfw.linjs.dropped = PLFW_FALSE;
                pollAbsState(js);
            }
        }

        if (_plfw.linjs.dropped)
            continue;

        if (e.type == EV_KEY)
            handleKeyEvent(js, e.code, e.value);
        else if (e.type == EV_ABS)
            handleAbsEvent(js, e.code, e.value);
    }

    return js->connected;
}

const char* _plfwGetMappingNameLinux(void)
{
    return "Linux";
}

void _plfwUpdateGamepadGUIDLinux(char* guid)
{
}

#endif // PLFW_BUILD_LINUX_JOYSTICK

