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
#include "mappings.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Internal key state used for sticky keys
#define _PLFW_STICK 3

// Internal constants for gamepad mapping source types
#define _PLFW_JOYSTICK_AXIS     1
#define _PLFW_JOYSTICK_BUTTON   2
#define _PLFW_JOYSTICK_HATBIT   3

#define PLFW_MOD_MASK (PLFW_MOD_SHIFT | \
                       PLFW_MOD_CONTROL | \
                       PLFW_MOD_ALT | \
                       PLFW_MOD_SUPER | \
                       PLFW_MOD_CAPS_LOCK | \
                       PLFW_MOD_NUM_LOCK)

// Initializes the platform joystick API if it has not been already
//
static PLFWbool initJoysticks(void)
{
    if (!_plfw.joysticksInitialized)
    {
        if (!_plfw.platform.initJoysticks())
        {
            _plfw.platform.terminateJoysticks();
            return PLFW_FALSE;
        }
    }

    return _plfw.joysticksInitialized = PLFW_TRUE;
}

// Finds a mapping based on joystick GUID
//
static _PLFWmapping* findMapping(const char* guid)
{
    int i;

    for (i = 0;  i < _plfw.mappingCount;  i++)
    {
        if (strcmp(_plfw.mappings[i].guid, guid) == 0)
            return _plfw.mappings + i;
    }

    return NULL;
}

// Checks whether a gamepad mapping element is present in the hardware
//
static PLFWbool isValidElementForJoystick(const _PLFWmapelement* e,
                                          const _PLFWjoystick* js)
{
    if (e->type == _PLFW_JOYSTICK_HATBIT && (e->index >> 4) >= js->hatCount)
        return PLFW_FALSE;
    else if (e->type == _PLFW_JOYSTICK_BUTTON && e->index >= js->buttonCount)
        return PLFW_FALSE;
    else if (e->type == _PLFW_JOYSTICK_AXIS && e->index >= js->axisCount)
        return PLFW_FALSE;

    return PLFW_TRUE;
}

// Finds a mapping based on joystick GUID and verifies element indices
//
static _PLFWmapping* findValidMapping(const _PLFWjoystick* js)
{
    _PLFWmapping* mapping = findMapping(js->guid);
    if (mapping)
    {
        int i;

        for (i = 0;  i <= PLFW_GAMEPAD_BUTTON_LAST;  i++)
        {
            if (!isValidElementForJoystick(mapping->buttons + i, js))
                return NULL;
        }

        for (i = 0;  i <= PLFW_GAMEPAD_AXIS_LAST;  i++)
        {
            if (!isValidElementForJoystick(mapping->axes + i, js))
                return NULL;
        }
    }

    return mapping;
}

// Parses an SDL_GameControllerDB line and adds it to the mapping list
//
static PLFWbool parseMapping(_PLFWmapping* mapping, const char* string)
{
    const char* c = string;
    size_t i, length;
    struct
    {
        const char* name;
        _PLFWmapelement* element;
    } fields[] =
    {
        { "platform",      NULL },
        { "a",             mapping->buttons + PLFW_GAMEPAD_BUTTON_A },
        { "b",             mapping->buttons + PLFW_GAMEPAD_BUTTON_B },
        { "x",             mapping->buttons + PLFW_GAMEPAD_BUTTON_X },
        { "y",             mapping->buttons + PLFW_GAMEPAD_BUTTON_Y },
        { "back",          mapping->buttons + PLFW_GAMEPAD_BUTTON_BACK },
        { "start",         mapping->buttons + PLFW_GAMEPAD_BUTTON_START },
        { "guide",         mapping->buttons + PLFW_GAMEPAD_BUTTON_GUIDE },
        { "leftshoulder",  mapping->buttons + PLFW_GAMEPAD_BUTTON_LEFT_BUMPER },
        { "rightshoulder", mapping->buttons + PLFW_GAMEPAD_BUTTON_RIGHT_BUMPER },
        { "leftstick",     mapping->buttons + PLFW_GAMEPAD_BUTTON_LEFT_THUMB },
        { "rightstick",    mapping->buttons + PLFW_GAMEPAD_BUTTON_RIGHT_THUMB },
        { "dpup",          mapping->buttons + PLFW_GAMEPAD_BUTTON_DPAD_UP },
        { "dpright",       mapping->buttons + PLFW_GAMEPAD_BUTTON_DPAD_RIGHT },
        { "dpdown",        mapping->buttons + PLFW_GAMEPAD_BUTTON_DPAD_DOWN },
        { "dpleft",        mapping->buttons + PLFW_GAMEPAD_BUTTON_DPAD_LEFT },
        { "lefttrigger",   mapping->axes + PLFW_GAMEPAD_AXIS_LEFT_TRIGGER },
        { "righttrigger",  mapping->axes + PLFW_GAMEPAD_AXIS_RIGHT_TRIGGER },
        { "leftx",         mapping->axes + PLFW_GAMEPAD_AXIS_LEFT_X },
        { "lefty",         mapping->axes + PLFW_GAMEPAD_AXIS_LEFT_Y },
        { "rightx",        mapping->axes + PLFW_GAMEPAD_AXIS_RIGHT_X },
        { "righty",        mapping->axes + PLFW_GAMEPAD_AXIS_RIGHT_Y }
    };

    length = strcspn(c, ",");
    if (length != 32 || c[length] != ',')
    {
        _plfwInputError(PLFW_INVALID_VALUE, NULL);
        return PLFW_FALSE;
    }

    memcpy(mapping->guid, c, length);
    c += length + 1;

    length = strcspn(c, ",");
    if (length >= sizeof(mapping->name) || c[length] != ',')
    {
        _plfwInputError(PLFW_INVALID_VALUE, NULL);
        return PLFW_FALSE;
    }

    memcpy(mapping->name, c, length);
    c += length + 1;

    while (*c)
    {
        // TODO: Implement output modifiers
        if (*c == '+' || *c == '-')
            return PLFW_FALSE;

        for (i = 0;  i < sizeof(fields) / sizeof(fields[0]);  i++)
        {
            length = strlen(fields[i].name);
            if (strncmp(c, fields[i].name, length) != 0 || c[length] != ':')
                continue;

            c += length + 1;

            if (fields[i].element)
            {
                _PLFWmapelement* e = fields[i].element;
                int8_t minimum = -1;
                int8_t maximum = 1;

                if (*c == '+')
                {
                    minimum = 0;
                    c += 1;
                }
                else if (*c == '-')
                {
                    maximum = 0;
                    c += 1;
                }

                if (*c == 'a')
                    e->type = _PLFW_JOYSTICK_AXIS;
                else if (*c == 'b')
                    e->type = _PLFW_JOYSTICK_BUTTON;
                else if (*c == 'h')
                    e->type = _PLFW_JOYSTICK_HATBIT;
                else
                    break;

                if (e->type == _PLFW_JOYSTICK_HATBIT)
                {
                    const unsigned long hat = strtoul(c + 1, (char**) &c, 10);
                    const unsigned long bit = strtoul(c + 1, (char**) &c, 10);
                    e->index = (uint8_t) ((hat << 4) | bit);
                }
                else
                    e->index = (uint8_t) strtoul(c + 1, (char**) &c, 10);

                if (e->type == _PLFW_JOYSTICK_AXIS)
                {
                    e->axisScale = 2 / (maximum - minimum);
                    e->axisOffset = -(maximum + minimum);

                    if (*c == '~')
                    {
                        e->axisScale = -e->axisScale;
                        e->axisOffset = -e->axisOffset;
                    }
                }
            }
            else
            {
                const char* name = _plfw.platform.getMappingName();
                length = strlen(name);
                if (strncmp(c, name, length) != 0)
                    return PLFW_FALSE;
            }

            break;
        }

        c += strcspn(c, ",");
        c += strspn(c, ",");
    }

    for (i = 0;  i < 32;  i++)
    {
        if (mapping->guid[i] >= 'A' && mapping->guid[i] <= 'F')
            mapping->guid[i] += 'a' - 'A';
    }

    _plfw.platform.updateGamepadGUID(mapping->guid);
    return PLFW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                         PLFW event API                       //////
//////////////////////////////////////////////////////////////////////////

// Notifies shared code of a physical key event
//
void _plfwInputKey(_PLFWwindow* window, int key, int scancode, int action, int mods)
{
    assert(window != NULL);
    assert(key >= 0 || key == PLFW_KEY_UNKNOWN);
    assert(key <= PLFW_KEY_LAST);
    assert(action == PLFW_PRESS || action == PLFW_RELEASE);
    assert(mods == (mods & PLFW_MOD_MASK));

    if (key >= 0 && key <= PLFW_KEY_LAST)
    {
        PLFWbool repeated = PLFW_FALSE;

        if (action == PLFW_RELEASE && window->keys[key] == PLFW_RELEASE)
            return;

        if (action == PLFW_PRESS && window->keys[key] == PLFW_PRESS)
            repeated = PLFW_TRUE;

        if (action == PLFW_RELEASE && window->stickyKeys)
            window->keys[key] = _PLFW_STICK;
        else
            window->keys[key] = (char) action;

        if (repeated)
            action = PLFW_REPEAT;
    }

    if (!window->lockKeyMods)
        mods &= ~(PLFW_MOD_CAPS_LOCK | PLFW_MOD_NUM_LOCK);

    if (window->callbacks.key)
        window->callbacks.key((PLFWwindow*) window, key, scancode, action, mods);
}

// Notifies shared code of a Unicode codepoint input event
// The 'plain' parameter determines whether to emit a regular character event
//
void _plfwInputChar(_PLFWwindow* window, uint32_t codepoint, int mods, PLFWbool plain)
{
    assert(window != NULL);
    assert(mods == (mods & PLFW_MOD_MASK));
    assert(plain == PLFW_TRUE || plain == PLFW_FALSE);

    if (codepoint < 32 || (codepoint > 126 && codepoint < 160))
        return;

    if (!window->lockKeyMods)
        mods &= ~(PLFW_MOD_CAPS_LOCK | PLFW_MOD_NUM_LOCK);

    if (window->callbacks.charmods)
        window->callbacks.charmods((PLFWwindow*) window, codepoint, mods);

    if (plain)
    {
        if (window->callbacks.character)
            window->callbacks.character((PLFWwindow*) window, codepoint);
    }
}

// Notifies shared code of a scroll event
//
void _plfwInputScroll(_PLFWwindow* window, double xoffset, double yoffset)
{
    assert(window != NULL);
    assert(isfinite(xoffset));
    assert(isfinite(yoffset));

    if (window->callbacks.scroll)
        window->callbacks.scroll((PLFWwindow*) window, xoffset, yoffset);
}

// Notifies shared code of a mouse button click event
//
void _plfwInputMouseClick(_PLFWwindow* window, int button, int action, int mods)
{
    assert(window != NULL);
    assert(button >= 0);
    assert(action == PLFW_PRESS || action == PLFW_RELEASE);
    assert(mods == (mods & PLFW_MOD_MASK));

    if (button < 0 || (!window->disableMouseButtonLimit && button > PLFW_MOUSE_BUTTON_LAST))
        return;

    if (!window->lockKeyMods)
        mods &= ~(PLFW_MOD_CAPS_LOCK | PLFW_MOD_NUM_LOCK);

    if (button <= PLFW_MOUSE_BUTTON_LAST)
    {
        if (action == PLFW_RELEASE && window->stickyMouseButtons)
            window->mouseButtons[button] = _PLFW_STICK;
        else
            window->mouseButtons[button] = (char) action;
    }

    if (window->callbacks.mouseButton)
        window->callbacks.mouseButton((PLFWwindow*) window, button, action, mods);
}

// Notifies shared code of a cursor motion event
// The position is specified in content area relative screen coordinates
//
void _plfwInputCursorPos(_PLFWwindow* window, double xpos, double ypos)
{
    assert(window != NULL);
    assert(isfinite(xpos));
    assert(isfinite(ypos));

    if (window->virtualCursorPosX == xpos && window->virtualCursorPosY == ypos)
        return;

    window->virtualCursorPosX = xpos;
    window->virtualCursorPosY = ypos;

    if (window->callbacks.cursorPos)
        window->callbacks.cursorPos((PLFWwindow*) window, xpos, ypos);
}

// Notifies shared code of a cursor enter/leave event
//
void _plfwInputCursorEnter(_PLFWwindow* window, PLFWbool entered)
{
    assert(window != NULL);
    assert(entered == PLFW_TRUE || entered == PLFW_FALSE);

    if (window->callbacks.cursorEnter)
        window->callbacks.cursorEnter((PLFWwindow*) window, entered);
}

// Notifies shared code of files or directories dropped on a window
//
void _plfwInputDrop(_PLFWwindow* window, int count, const char** paths)
{
    assert(window != NULL);
    assert(count > 0);
    assert(paths != NULL);

    if (window->callbacks.drop)
        window->callbacks.drop((PLFWwindow*) window, count, paths);
}

// Notifies shared code of a joystick connection or disconnection
//
void _plfwInputJoystick(_PLFWjoystick* js, int event)
{
    assert(js != NULL);
    assert(event == PLFW_CONNECTED || event == PLFW_DISCONNECTED);

    if (event == PLFW_CONNECTED)
        js->connected = PLFW_TRUE;
    else if (event == PLFW_DISCONNECTED)
        js->connected = PLFW_FALSE;

    if (_plfw.callbacks.joystick)
        _plfw.callbacks.joystick((int) (js - _plfw.joysticks), event);
}

// Notifies shared code of the new value of a joystick axis
//
void _plfwInputJoystickAxis(_PLFWjoystick* js, int axis, float value)
{
    assert(js != NULL);
    assert(axis >= 0);
    assert(axis < js->axisCount);
    assert(isfinite(value));

    js->axes[axis] = value;
}

// Notifies shared code of the new value of a joystick button
//
void _plfwInputJoystickButton(_PLFWjoystick* js, int button, char value)
{
    assert(js != NULL);
    assert(button >= 0);
    assert(button < js->buttonCount);
    assert(value == PLFW_PRESS || value == PLFW_RELEASE);

    js->buttons[button] = value;
}

// Notifies shared code of the new value of a joystick hat
//
void _plfwInputJoystickHat(_PLFWjoystick* js, int hat, char value)
{
    int base;

    assert(js != NULL);
    assert(hat >= 0);
    assert(hat < js->hatCount);

    // Valid hat values only use the least significant nibble
    assert((value & 0xf0) == 0);
    // Valid hat values do not have both bits of an axis set
    assert((value & PLFW_HAT_LEFT) == 0 || (value & PLFW_HAT_RIGHT) == 0);
    assert((value & PLFW_HAT_UP) == 0 || (value & PLFW_HAT_DOWN) == 0);

    base = js->buttonCount + hat * 4;

    js->buttons[base + 0] = (value & 0x01) ? PLFW_PRESS : PLFW_RELEASE;
    js->buttons[base + 1] = (value & 0x02) ? PLFW_PRESS : PLFW_RELEASE;
    js->buttons[base + 2] = (value & 0x04) ? PLFW_PRESS : PLFW_RELEASE;
    js->buttons[base + 3] = (value & 0x08) ? PLFW_PRESS : PLFW_RELEASE;

    js->hats[hat] = value;
}


//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

// Adds the built-in set of gamepad mappings
//
void _plfwInitGamepadMappings(void)
{
    size_t i;
    const size_t count = sizeof(_plfwDefaultMappings) / sizeof(char*);
    _plfw.mappings = _plfw_calloc(count, sizeof(_PLFWmapping));

    for (i = 0;  i < count;  i++)
    {
        if (parseMapping(&_plfw.mappings[_plfw.mappingCount], _plfwDefaultMappings[i]))
            _plfw.mappingCount++;
    }
}

// Returns an available joystick object with arrays and name allocated
//
_PLFWjoystick* _plfwAllocJoystick(const char* name,
                                  const char* guid,
                                  int axisCount,
                                  int buttonCount,
                                  int hatCount)
{
    int jid;
    _PLFWjoystick* js;

    for (jid = 0;  jid <= PLFW_JOYSTICK_LAST;  jid++)
    {
        if (!_plfw.joysticks[jid].allocated)
            break;
    }

    if (jid > PLFW_JOYSTICK_LAST)
        return NULL;

    js = _plfw.joysticks + jid;
    js->allocated   = PLFW_TRUE;
    js->axes        = _plfw_calloc(axisCount, sizeof(float));
    js->buttons     = _plfw_calloc(buttonCount + (size_t) hatCount * 4, 1);
    js->hats        = _plfw_calloc(hatCount, 1);
    js->axisCount   = axisCount;
    js->buttonCount = buttonCount;
    js->hatCount    = hatCount;

    strncpy(js->name, name, sizeof(js->name) - 1);
    strncpy(js->guid, guid, sizeof(js->guid) - 1);
    js->mapping = findValidMapping(js);

    return js;
}

// Frees arrays and name and flags the joystick object as unused
//
void _plfwFreeJoystick(_PLFWjoystick* js)
{
    _plfw_free(js->axes);
    _plfw_free(js->buttons);
    _plfw_free(js->hats);
    memset(js, 0, sizeof(_PLFWjoystick));
}

// Center the cursor in the content area of the specified window
//
void _plfwCenterCursorInContentArea(_PLFWwindow* window)
{
    int width, height;

    _plfw.platform.getWindowSize(window, &width, &height);
    _plfw.platform.setCursorPos(window, width / 2.0, height / 2.0);
}


//////////////////////////////////////////////////////////////////////////
//////                        PLFW public API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI int plfwGetInputMode(PLFWwindow* handle, int mode)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    switch (mode)
    {
        case PLFW_CURSOR:
            return window->cursorMode;
        case PLFW_STICKY_KEYS:
            return window->stickyKeys;
        case PLFW_STICKY_MOUSE_BUTTONS:
            return window->stickyMouseButtons;
        case PLFW_LOCK_KEY_MODS:
            return window->lockKeyMods;
        case PLFW_RAW_MOUSE_MOTION:
            return window->rawMouseMotion;
        case PLFW_UNLIMITED_MOUSE_BUTTONS:
            return window->disableMouseButtonLimit;
    }

    _plfwInputError(PLFW_INVALID_ENUM, "Invalid input mode 0x%08X", mode);
    return 0;
}

PLFWAPI void plfwSetInputMode(PLFWwindow* handle, int mode, int value)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    switch (mode)
    {
        case PLFW_CURSOR:
        {
            if (value != PLFW_CURSOR_NORMAL &&
                value != PLFW_CURSOR_HIDDEN &&
                value != PLFW_CURSOR_DISABLED &&
                value != PLFW_CURSOR_CAPTURED)
            {
                _plfwInputError(PLFW_INVALID_ENUM,
                                "Invalid cursor mode 0x%08X",
                                value);
                return;
            }

            if (window->cursorMode == value)
                return;

            window->cursorMode = value;

            _plfw.platform.getCursorPos(window,
                                        &window->virtualCursorPosX,
                                        &window->virtualCursorPosY);
            _plfw.platform.setCursorMode(window, value);
            return;
        }

        case PLFW_STICKY_KEYS:
        {
            value = value ? PLFW_TRUE : PLFW_FALSE;
            if (window->stickyKeys == value)
                return;

            if (!value)
            {
                int i;

                // Release all sticky keys
                for (i = 0;  i <= PLFW_KEY_LAST;  i++)
                {
                    if (window->keys[i] == _PLFW_STICK)
                        window->keys[i] = PLFW_RELEASE;
                }
            }

            window->stickyKeys = value;
            return;
        }

        case PLFW_STICKY_MOUSE_BUTTONS:
        {
            value = value ? PLFW_TRUE : PLFW_FALSE;
            if (window->stickyMouseButtons == value)
                return;

            if (!value)
            {
                int i;

                // Release all sticky mouse buttons
                for (i = 0;  i <= PLFW_MOUSE_BUTTON_LAST;  i++)
                {
                    if (window->mouseButtons[i] == _PLFW_STICK)
                        window->mouseButtons[i] = PLFW_RELEASE;
                }
            }

            window->stickyMouseButtons = value;
            return;
        }

        case PLFW_LOCK_KEY_MODS:
        {
            window->lockKeyMods = value ? PLFW_TRUE : PLFW_FALSE;
            return;
        }

        case PLFW_RAW_MOUSE_MOTION:
        {
            if (!_plfw.platform.rawMouseMotionSupported())
            {
                _plfwInputError(PLFW_PLATFORM_ERROR,
                                "Raw mouse motion is not supported on this system");
                return;
            }

            value = value ? PLFW_TRUE : PLFW_FALSE;
            if (window->rawMouseMotion == value)
                return;

            window->rawMouseMotion = value;
            _plfw.platform.setRawMouseMotion(window, value);
            return;
        }

        case PLFW_UNLIMITED_MOUSE_BUTTONS:
        {
            window->disableMouseButtonLimit = value ? PLFW_TRUE : PLFW_FALSE;
            return;
        }
    }

    _plfwInputError(PLFW_INVALID_ENUM, "Invalid input mode 0x%08X", mode);
}

PLFWAPI int plfwRawMouseMotionSupported(void)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(PLFW_FALSE);
    return _plfw.platform.rawMouseMotionSupported();
}

PLFWAPI const char* plfwGetKeyName(int key, int scancode)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (key != PLFW_KEY_UNKNOWN)
    {
        if (key < PLFW_KEY_SPACE || key > PLFW_KEY_LAST)
        {
            _plfwInputError(PLFW_INVALID_ENUM, "Invalid key %i", key);
            return NULL;
        }

        if (key != PLFW_KEY_KP_EQUAL &&
            (key < PLFW_KEY_KP_0 || key > PLFW_KEY_KP_ADD) &&
            (key < PLFW_KEY_APOSTROPHE || key > PLFW_KEY_WORLD_2))
        {
            return NULL;
        }

        scancode = _plfw.platform.getKeyScancode(key);
    }

    return _plfw.platform.getScancodeName(scancode);
}

PLFWAPI int plfwGetKeyScancode(int key)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0);

    if (key < PLFW_KEY_SPACE || key > PLFW_KEY_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid key %i", key);
        return -1;
    }

    return _plfw.platform.getKeyScancode(key);
}

PLFWAPI int plfwGetKey(PLFWwindow* handle, int key)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(PLFW_RELEASE);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (key < PLFW_KEY_SPACE || key > PLFW_KEY_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid key %i", key);
        return PLFW_RELEASE;
    }

    if (window->keys[key] == _PLFW_STICK)
    {
        // Sticky mode: release key now
        window->keys[key] = PLFW_RELEASE;
        return PLFW_PRESS;
    }

    return (int) window->keys[key];
}

PLFWAPI int plfwGetMouseButton(PLFWwindow* handle, int button)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(PLFW_RELEASE);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (button < PLFW_MOUSE_BUTTON_1 || button > PLFW_MOUSE_BUTTON_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid mouse button %i", button);
        return PLFW_RELEASE;
    }

    if (window->mouseButtons[button] == _PLFW_STICK)
    {
        // Sticky mode: release mouse button now
        window->mouseButtons[button] = PLFW_RELEASE;
        return PLFW_PRESS;
    }

    return (int) window->mouseButtons[button];
}

PLFWAPI void plfwGetCursorPos(PLFWwindow* handle, double* xpos, double* ypos)
{
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (window->cursorMode == PLFW_CURSOR_DISABLED)
    {
        if (xpos)
            *xpos = window->virtualCursorPosX;
        if (ypos)
            *ypos = window->virtualCursorPosY;
    }
    else
        _plfw.platform.getCursorPos(window, xpos, ypos);
}

PLFWAPI void plfwSetCursorPos(PLFWwindow* handle, double xpos, double ypos)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    if (!isfinite(xpos) || !isfinite(ypos))
    {
        _plfwInputError(PLFW_INVALID_VALUE,
                        "Invalid cursor position %f %f",
                        xpos, ypos);
        return;
    }

    if (!_plfw.platform.windowFocused(window))
        return;

    if (window->cursorMode == PLFW_CURSOR_DISABLED)
    {
        // Only update the accumulated position if the cursor is disabled
        window->virtualCursorPosX = xpos;
        window->virtualCursorPosY = ypos;
    }
    else
    {
        // Update system cursor position
        _plfw.platform.setCursorPos(window, xpos, ypos);
    }
}

PLFWAPI PLFWcursor* plfwCreateCursor(const PLFWimage* image, int xhot, int yhot)
{
    _PLFWcursor* cursor;

    assert(image != NULL);
    assert(image->pixels != NULL);

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (image->width <= 0 || image->height <= 0)
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid image dimensions for cursor");
        return NULL;
    }

    cursor = _plfw_calloc(1, sizeof(_PLFWcursor));
    cursor->next = _plfw.cursorListHead;
    _plfw.cursorListHead = cursor;

    if (!_plfw.platform.createCursor(cursor, image, xhot, yhot))
    {
        plfwDestroyCursor((PLFWcursor*) cursor);
        return NULL;
    }

    return (PLFWcursor*) cursor;
}

PLFWAPI PLFWcursor* plfwCreateStandardCursor(int shape)
{
    _PLFWcursor* cursor;

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (shape != PLFW_ARROW_CURSOR &&
        shape != PLFW_IBEAM_CURSOR &&
        shape != PLFW_CROSSHAIR_CURSOR &&
        shape != PLFW_POINTING_HAND_CURSOR &&
        shape != PLFW_RESIZE_EW_CURSOR &&
        shape != PLFW_RESIZE_NS_CURSOR &&
        shape != PLFW_RESIZE_NWSE_CURSOR &&
        shape != PLFW_RESIZE_NESW_CURSOR &&
        shape != PLFW_RESIZE_ALL_CURSOR &&
        shape != PLFW_NOT_ALLOWED_CURSOR)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid standard cursor 0x%08X", shape);
        return NULL;
    }

    cursor = _plfw_calloc(1, sizeof(_PLFWcursor));
    cursor->next = _plfw.cursorListHead;
    _plfw.cursorListHead = cursor;

    if (!_plfw.platform.createStandardCursor(cursor, shape))
    {
        plfwDestroyCursor((PLFWcursor*) cursor);
        return NULL;
    }

    return (PLFWcursor*) cursor;
}

PLFWAPI void plfwDestroyCursor(PLFWcursor* handle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWcursor* cursor = (_PLFWcursor*) handle;

    if (cursor == NULL)
        return;

    // Make sure the cursor is not being used by any window
    {
        _PLFWwindow* window;

        for (window = _plfw.windowListHead;  window;  window = window->next)
        {
            if (window->cursor == cursor)
                plfwSetCursor((PLFWwindow*) window, NULL);
        }
    }

    _plfw.platform.destroyCursor(cursor);

    // Unlink cursor from global linked list
    {
        _PLFWcursor** prev = &_plfw.cursorListHead;

        while (*prev != cursor)
            prev = &((*prev)->next);

        *prev = cursor->next;
    }

    _plfw_free(cursor);
}

PLFWAPI void plfwSetCursor(PLFWwindow* windowHandle, PLFWcursor* cursorHandle)
{
    _PLFW_REQUIRE_INIT();

    _PLFWwindow* window = (_PLFWwindow*) windowHandle;
    _PLFWcursor* cursor = (_PLFWcursor*) cursorHandle;
    assert(window != NULL);

    window->cursor = cursor;

    _plfw.platform.setCursor(window, cursor);
}

PLFWAPI PLFWkeyfun plfwSetKeyCallback(PLFWwindow* handle, PLFWkeyfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWkeyfun, window->callbacks.key, cbfun);
    return cbfun;
}

PLFWAPI PLFWcharfun plfwSetCharCallback(PLFWwindow* handle, PLFWcharfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWcharfun, window->callbacks.character, cbfun);
    return cbfun;
}

PLFWAPI PLFWcharmodsfun plfwSetCharModsCallback(PLFWwindow* handle, PLFWcharmodsfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWcharmodsfun, window->callbacks.charmods, cbfun);
    return cbfun;
}

PLFWAPI PLFWmousebuttonfun plfwSetMouseButtonCallback(PLFWwindow* handle,
                                                      PLFWmousebuttonfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWmousebuttonfun, window->callbacks.mouseButton, cbfun);
    return cbfun;
}

PLFWAPI PLFWcursorposfun plfwSetCursorPosCallback(PLFWwindow* handle,
                                                  PLFWcursorposfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWcursorposfun, window->callbacks.cursorPos, cbfun);
    return cbfun;
}

PLFWAPI PLFWcursorenterfun plfwSetCursorEnterCallback(PLFWwindow* handle,
                                                      PLFWcursorenterfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWcursorenterfun, window->callbacks.cursorEnter, cbfun);
    return cbfun;
}

PLFWAPI PLFWscrollfun plfwSetScrollCallback(PLFWwindow* handle,
                                            PLFWscrollfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWscrollfun, window->callbacks.scroll, cbfun);
    return cbfun;
}

PLFWAPI PLFWdropfun plfwSetDropCallback(PLFWwindow* handle, PLFWdropfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    _PLFW_SWAP(PLFWdropfun, window->callbacks.drop, cbfun);
    return cbfun;
}

PLFWAPI int plfwJoystickPresent(int jid)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);

    _PLFW_REQUIRE_INIT_OR_RETURN(PLFW_FALSE);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return PLFW_FALSE;
    }

    if (!initJoysticks())
        return PLFW_FALSE;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return PLFW_FALSE;

    return _plfw.platform.pollJoystick(js, _PLFW_POLL_PRESENCE);
}

PLFWAPI const float* plfwGetJoystickAxes(int jid, int* count)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);
    assert(count != NULL);

    *count = 0;

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_AXES))
        return NULL;

    *count = js->axisCount;
    return js->axes;
}

PLFWAPI const unsigned char* plfwGetJoystickButtons(int jid, int* count)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);
    assert(count != NULL);

    *count = 0;

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_BUTTONS))
        return NULL;

    if (_plfw.hints.init.hatButtons)
        *count = js->buttonCount + js->hatCount * 4;
    else
        *count = js->buttonCount;

    return js->buttons;
}

PLFWAPI const unsigned char* plfwGetJoystickHats(int jid, int* count)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);
    assert(count != NULL);

    *count = 0;

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_BUTTONS))
        return NULL;

    *count = js->hatCount;
    return js->hats;
}

PLFWAPI const char* plfwGetJoystickName(int jid)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_PRESENCE))
        return NULL;

    return js->name;
}

PLFWAPI const char* plfwGetJoystickGUID(int jid)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_PRESENCE))
        return NULL;

    return js->guid;
}

PLFWAPI void plfwSetJoystickUserPointer(int jid, void* pointer)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);

    _PLFW_REQUIRE_INIT();

    js = _plfw.joysticks + jid;
    if (!js->allocated)
        return;

    js->userPointer = pointer;
}

PLFWAPI void* plfwGetJoystickUserPointer(int jid)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    js = _plfw.joysticks + jid;
    if (!js->allocated)
        return NULL;

    return js->userPointer;
}

PLFWAPI PLFWjoystickfun plfwSetJoystickCallback(PLFWjoystickfun cbfun)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (!initJoysticks())
        return NULL;

    _PLFW_SWAP(PLFWjoystickfun, _plfw.callbacks.joystick, cbfun);
    return cbfun;
}

PLFWAPI int plfwUpdateGamepadMappings(const char* string)
{
    int jid;
    const char* c = string;

    assert(string != NULL);

    _PLFW_REQUIRE_INIT_OR_RETURN(PLFW_FALSE);

    while (*c)
    {
        if ((*c >= '0' && *c <= '9') ||
            (*c >= 'a' && *c <= 'f') ||
            (*c >= 'A' && *c <= 'F'))
        {
            char line[1024];

            const size_t length = strcspn(c, "\r\n");
            if (length < sizeof(line))
            {
                _PLFWmapping mapping = {{0}};

                memcpy(line, c, length);
                line[length] = '\0';

                if (parseMapping(&mapping, line))
                {
                    _PLFWmapping* previous = findMapping(mapping.guid);
                    if (previous)
                        *previous = mapping;
                    else
                    {
                        _plfw.mappingCount++;
                        _plfw.mappings =
                            _plfw_realloc(_plfw.mappings,
                                          sizeof(_PLFWmapping) * _plfw.mappingCount);
                        _plfw.mappings[_plfw.mappingCount - 1] = mapping;
                    }
                }
            }

            c += length;
        }
        else
        {
            c += strcspn(c, "\r\n");
            c += strspn(c, "\r\n");
        }
    }

    for (jid = 0;  jid <= PLFW_JOYSTICK_LAST;  jid++)
    {
        _PLFWjoystick* js = _plfw.joysticks + jid;
        if (js->connected)
            js->mapping = findValidMapping(js);
    }

    return PLFW_TRUE;
}

PLFWAPI int plfwJoystickIsGamepad(int jid)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);

    _PLFW_REQUIRE_INIT_OR_RETURN(PLFW_FALSE);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return PLFW_FALSE;
    }

    if (!initJoysticks())
        return PLFW_FALSE;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return PLFW_FALSE;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_PRESENCE))
        return PLFW_FALSE;

    return js->mapping != NULL;
}

PLFWAPI const char* plfwGetGamepadName(int jid)
{
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);

    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return NULL;
    }

    if (!initJoysticks())
        return NULL;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return NULL;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_PRESENCE))
        return NULL;

    if (!js->mapping)
        return NULL;

    return js->mapping->name;
}

PLFWAPI int plfwGetGamepadState(int jid, PLFWgamepadstate* state)
{
    int i;
    _PLFWjoystick* js;

    assert(jid >= PLFW_JOYSTICK_1);
    assert(jid <= PLFW_JOYSTICK_LAST);
    assert(state != NULL);

    memset(state, 0, sizeof(PLFWgamepadstate));

    _PLFW_REQUIRE_INIT_OR_RETURN(PLFW_FALSE);

    if (jid < 0 || jid > PLFW_JOYSTICK_LAST)
    {
        _plfwInputError(PLFW_INVALID_ENUM, "Invalid joystick ID %i", jid);
        return PLFW_FALSE;
    }

    if (!initJoysticks())
        return PLFW_FALSE;

    js = _plfw.joysticks + jid;
    if (!js->connected)
        return PLFW_FALSE;

    if (!_plfw.platform.pollJoystick(js, _PLFW_POLL_ALL))
        return PLFW_FALSE;

    if (!js->mapping)
        return PLFW_FALSE;

    for (i = 0;  i <= PLFW_GAMEPAD_BUTTON_LAST;  i++)
    {
        const _PLFWmapelement* e = js->mapping->buttons + i;
        if (e->type == _PLFW_JOYSTICK_AXIS)
        {
            const float value = js->axes[e->index] * e->axisScale + e->axisOffset;
            // HACK: This should be baked into the value transform
            // TODO: Bake into transform when implementing output modifiers
            if (e->axisOffset < 0 || (e->axisOffset == 0 && e->axisScale > 0))
            {
                if (value >= 0.f)
                    state->buttons[i] = PLFW_PRESS;
            }
            else
            {
                if (value <= 0.f)
                    state->buttons[i] = PLFW_PRESS;
            }
        }
        else if (e->type == _PLFW_JOYSTICK_HATBIT)
        {
            const unsigned int hat = e->index >> 4;
            const unsigned int bit = e->index & 0xf;
            if (js->hats[hat] & bit)
                state->buttons[i] = PLFW_PRESS;
        }
        else if (e->type == _PLFW_JOYSTICK_BUTTON)
            state->buttons[i] = js->buttons[e->index];
    }

    for (i = 0;  i <= PLFW_GAMEPAD_AXIS_LAST;  i++)
    {
        const _PLFWmapelement* e = js->mapping->axes + i;
        if (e->type == _PLFW_JOYSTICK_AXIS)
        {
            const float value = js->axes[e->index] * e->axisScale + e->axisOffset;
            state->axes[i] = fminf(fmaxf(value, -1.f), 1.f);
        }
        else if (e->type == _PLFW_JOYSTICK_HATBIT)
        {
            const unsigned int hat = e->index >> 4;
            const unsigned int bit = e->index & 0xf;
            if (js->hats[hat] & bit)
                state->axes[i] = 1.f;
            else
                state->axes[i] = -1.f;
        }
        else if (e->type == _PLFW_JOYSTICK_BUTTON)
            state->axes[i] = js->buttons[e->index] * 2.f - 1.f;
    }

    return PLFW_TRUE;
}

PLFWAPI void plfwSetClipboardString(PLFWwindow* handle, const char* string)
{
    assert(string != NULL);

    _PLFW_REQUIRE_INIT();
    _plfw.platform.setClipboardString(string);
}

PLFWAPI const char* plfwGetClipboardString(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);
    return _plfw.platform.getClipboardString();
}

PLFWAPI double plfwGetTime(void)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0.0);
    return (double) (_plfwPlatformGetTimerValue() - _plfw.timer.offset) /
        _plfwPlatformGetTimerFrequency();
}

PLFWAPI void plfwSetTime(double time)
{
    _PLFW_REQUIRE_INIT();

    if (!isfinite(time) || time < 0.0 || time > 18446744073.0)
    {
        _plfwInputError(PLFW_INVALID_VALUE, "Invalid time %f", time);
        return;
    }

    _plfw.timer.offset = _plfwPlatformGetTimerValue() -
        (uint64_t) (time * _plfwPlatformGetTimerFrequency());
}

PLFWAPI uint64_t plfwGetTimerValue(void)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0);
    return _plfwPlatformGetTimerValue();
}

PLFWAPI uint64_t plfwGetTimerFrequency(void)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(0);
    return _plfwPlatformGetTimerFrequency();
}

