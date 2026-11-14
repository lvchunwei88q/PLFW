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

#if defined(PLFW_BUILD_WIN32_TIMER) || \
    defined(PLFW_BUILD_WIN32_MODULE) || \
    defined(PLFW_BUILD_WIN32_THREAD) || \
    defined(PLFW_BUILD_POSIX_TIMER) || \
    defined(PLFW_BUILD_POSIX_MODULE) || \
    defined(PLFW_BUILD_POSIX_THREAD) || \
    defined(PLFW_BUILD_POSIX_POLL) || \
    defined(PLFW_BUILD_LINUX_JOYSTICK)
 #error "You must not define these; define zero or more _PLFW_<platform> macros instead"
#endif

#include "null/null_platform.h"

#if defined(_PLFW_WIN32)
 #include "win32/win32_platform.h"
 #define PLFW_EXPOSE_NATIVE_WIN32
#else
 #define PLFW_WIN32_WINDOW_STATE
 #define PLFW_WIN32_MONITOR_STATE
 #define PLFW_WIN32_CURSOR_STATE
 #define PLFW_WIN32_LIBRARY_WINDOW_STATE
#endif

#if defined(_PLFW_WAYLAND)
 #include "wayland/wl_platform.h"
 #define PLFW_EXPOSE_NATIVE_WAYLAND
#else
 #define PLFW_WAYLAND_WINDOW_STATE
 #define PLFW_WAYLAND_MONITOR_STATE
 #define PLFW_WAYLAND_CURSOR_STATE
 #define PLFW_WAYLAND_LIBRARY_WINDOW_STATE
#endif

#include "null/null_joystick.h"

#if defined(_PLFW_WIN32)
 #include "win32/win32_joystick.h"
#else
 #define PLFW_WIN32_JOYSTICK_STATE
 #define PLFW_WIN32_LIBRARY_JOYSTICK_STATE
#endif

#if defined(_PLFW_WAYLAND) && defined(__linux__)
 #define PLFW_BUILD_LINUX_JOYSTICK
#endif

#if defined(PLFW_BUILD_LINUX_JOYSTICK)
 #include "wayland/linux_joystick.h"
#else
 #define PLFW_LINUX_JOYSTICK_STATE
 #define PLFW_LINUX_LIBRARY_JOYSTICK_STATE
#endif

#define PLFW_PLATFORM_WINDOW_STATE \
        PLFW_WIN32_WINDOW_STATE \
        PLFW_WAYLAND_WINDOW_STATE \
        PLFW_NULL_WINDOW_STATE \

#define PLFW_PLATFORM_MONITOR_STATE \
        PLFW_WIN32_MONITOR_STATE \
        PLFW_WAYLAND_MONITOR_STATE \
        PLFW_NULL_MONITOR_STATE \

#define PLFW_PLATFORM_CURSOR_STATE \
        PLFW_WIN32_CURSOR_STATE \
        PLFW_WAYLAND_CURSOR_STATE \
        PLFW_NULL_CURSOR_STATE \

#define PLFW_PLATFORM_JOYSTICK_STATE \
        PLFW_WIN32_JOYSTICK_STATE \
        PLFW_LINUX_JOYSTICK_STATE

#define PLFW_PLATFORM_LIBRARY_WINDOW_STATE \
        PLFW_WIN32_LIBRARY_WINDOW_STATE \
        PLFW_WAYLAND_LIBRARY_WINDOW_STATE \
        PLFW_NULL_LIBRARY_WINDOW_STATE \

#define PLFW_PLATFORM_LIBRARY_JOYSTICK_STATE \
        PLFW_WIN32_LIBRARY_JOYSTICK_STATE \
        PLFW_LINUX_LIBRARY_JOYSTICK_STATE

#if defined(_WIN32)
 #define PLFW_BUILD_WIN32_THREAD
#else
 #define PLFW_BUILD_POSIX_THREAD
#endif

#if defined(PLFW_BUILD_WIN32_THREAD)
 #include "win32/win32_thread.h"
 #define PLFW_PLATFORM_TLS_STATE    PLFW_WIN32_TLS_STATE
 #define PLFW_PLATFORM_MUTEX_STATE  PLFW_WIN32_MUTEX_STATE
#elif defined(PLFW_BUILD_POSIX_THREAD)
 #include "posix/posix_thread.h"
 #define PLFW_PLATFORM_TLS_STATE    PLFW_POSIX_TLS_STATE
 #define PLFW_PLATFORM_MUTEX_STATE  PLFW_POSIX_MUTEX_STATE
#endif

#if defined(_WIN32)
 #define PLFW_BUILD_WIN32_TIMER
#else
 #define PLFW_BUILD_POSIX_TIMER
#endif

#if defined(PLFW_BUILD_WIN32_TIMER)
 #include "win32/win32_time.h"
 #define PLFW_PLATFORM_LIBRARY_TIMER_STATE  PLFW_WIN32_LIBRARY_TIMER_STATE
#elif defined(PLFW_BUILD_POSIX_TIMER)
 #include "posix/posix_time.h"
 #define PLFW_PLATFORM_LIBRARY_TIMER_STATE  PLFW_POSIX_LIBRARY_TIMER_STATE
#endif

#if defined(_WIN32)
 #define PLFW_BUILD_WIN32_MODULE
#else
 #define PLFW_BUILD_POSIX_MODULE
#endif

#if defined(_PLFW_WAYLAND)
 #include "posix/posix_poll.h"
 #define PLFW_BUILD_POSIX_POLL
#endif

