//========================================================================
// PLFW 3.5 Win32 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
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

#define PLFW_WIN32_JOYSTICK_STATE         _PLFWjoystickWin32 win32;
#define PLFW_WIN32_LIBRARY_JOYSTICK_STATE

// Joystick element (axis, button or slider)
//
typedef struct _PLFWjoyobjectWin32
{
    int                     offset;
    int                     type;
} _PLFWjoyobjectWin32;

// Win32-specific per-joystick data
//
typedef struct _PLFWjoystickWin32
{
    _PLFWjoyobjectWin32*    objects;
    int                     objectCount;
    IDirectInputDevice8W*   device;
    DWORD                   index;
    GUID                    guid;
} _PLFWjoystickWin32;

void _plfwDetectJoystickConnectionWin32(void);
void _plfwDetectJoystickDisconnectionWin32(void);

