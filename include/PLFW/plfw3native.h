/*************************************************************************
 * PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
 * A library for OpenGL, window and input
 *------------------------------------------------------------------------
 * Copyright (c) 2002-2006 Marcus Geelnard
 * Copyright (c) 2006-2018 Camilla Löwy <elmindreda@glfw.org>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would
 *    be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 *    distribution.
 *
 *************************************************************************/

#ifndef _plfw3_native_h_
#define _plfw3_native_h_

#ifdef __cplusplus
extern "C" {
#endif


/*************************************************************************
 * Doxygen documentation
 *************************************************************************/

/*! @file plfw3native.h
 *  @brief The header of the native access functions.
 *
 *  This is the header file of the native access functions.  See @ref native for
 *  more information.
 */
/*! @defgroup native Native access
 *  @brief Functions related to accessing native handles.
 *
 *  **By using the native access functions you assert that you know what you're
 *  doing and how to fix problems caused by using them.  If you don't, you
 *  shouldn't be using them.**
 *
 *  Before the inclusion of @ref plfw3native.h, you may define zero or more
 *  window system API macros.
 *
 *  The chosen backends must match those the library was compiled for.  Failure
 *  to do this will cause a link-time error.
 *
 *  The available window API macros are:
 *  * `PLFW_EXPOSE_NATIVE_WIN32`
 *  * `PLFW_EXPOSE_NATIVE_WAYLAND`
 *
 *  These macros select which of the native access functions that are declared
 *  and which platform-specific headers to include.  It is then up your (by
 *  definition platform-specific) code to handle which of these should be
 *  defined.
 *
 *  If you do not want the platform-specific headers to be included, define
 *  `PLFW_NATIVE_INCLUDE_NONE` before including the @ref plfw3native.h header.
 *
 *  @code
 *  #define PLFW_EXPOSE_NATIVE_WIN32
 *  #define PLFW_NATIVE_INCLUDE_NONE
 *  #include <PLFW/plfw3native.h>
 *  @endcode
 */


/*************************************************************************
 * System headers and types
 *************************************************************************/

#if !defined(PLFW_NATIVE_INCLUDE_NONE)

 #if defined(PLFW_EXPOSE_NATIVE_WIN32)
  /* This is a workaround for the fact that plfw3.h needs to export APIENTRY (for
   * example to allow applications to correctly declare a GL_KHR_debug callback)
   * but windows.h assumes no one will define APIENTRY before it does
   */
  #if defined(PLFW_APIENTRY_DEFINED)
   #undef APIENTRY
   #undef PLFW_APIENTRY_DEFINED
  #endif
  #include <windows.h>
 #endif

 #if defined(PLFW_EXPOSE_NATIVE_WAYLAND)
  #include <wayland-client.h>
 #endif

#endif /*PLFW_NATIVE_INCLUDE_NONE*/


/*************************************************************************
 * Functions
 *************************************************************************/

#if defined(PLFW_EXPOSE_NATIVE_WIN32)
/*! @brief Returns the adapter device name of the specified monitor.
 *
 *  @return The UTF-8 encoded adapter device name (for example `\\.\DISPLAY1`)
 *  of the specified monitor, or `NULL` if an [error](@ref error_handling)
 *  occurred.
 *
 *  @errors Possible errors include @ref PLFW_NOT_INITIALIZED and @ref
 *  PLFW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.1.
 *
 *  @ingroup native
 */
PLFWAPI const char* plfwGetWin32Adapter(PLFWmonitor* monitor);

/*! @brief Returns the display device name of the specified monitor.
 *
 *  @return The UTF-8 encoded display device name (for example
 *  `\\.\DISPLAY1\Monitor0`) of the specified monitor, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref PLFW_NOT_INITIALIZED and @ref
 *  PLFW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.1.
 *
 *  @ingroup native
 */
PLFWAPI const char* plfwGetWin32Monitor(PLFWmonitor* monitor);

/*! @brief Returns the `HWND` of the specified window.
 *
 *  @return The `HWND` of the specified window, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref PLFW_NOT_INITIALIZED and @ref
 *  PLFW_PLATFORM_UNAVAILABLE.
 *
 *  @remark The `HDC` associated with the window can be queried with the
 *  [GetDC](https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getdc)
 *  function.
 *  @code
 *  HDC dc = GetDC(plfwGetWin32Window(window));
 *  @endcode
 *  This DC is private and does not need to be released.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.0.
 *
 *  @ingroup native
 */
PLFWAPI HWND plfwGetWin32Window(PLFWwindow* window);
#endif

#if defined(PLFW_EXPOSE_NATIVE_WAYLAND)
/*! @brief Returns the `struct wl_display*` used by PLFW.
 *
 *  @return The `struct wl_display*` used by PLFW, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref PLFW_NOT_INITIALIZED and @ref
 *  PLFW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.2.
 *
 *  @ingroup native
 */
PLFWAPI struct wl_display* plfwGetWaylandDisplay(void);

/*! @brief Returns the `struct wl_output*` of the specified monitor.
 *
 *  @return The `struct wl_output*` of the specified monitor, or `NULL` if an
 *  [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref PLFW_NOT_INITIALIZED and @ref
 *  PLFW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.2.
 *
 *  @ingroup native
 */
PLFWAPI struct wl_output* plfwGetWaylandMonitor(PLFWmonitor* monitor);

/*! @brief Returns the main `struct wl_surface*` of the specified window.
 *
 *  @return The main `struct wl_surface*` of the specified window, or `NULL` if
 *  an [error](@ref error_handling) occurred.
 *
 *  @errors Possible errors include @ref PLFW_NOT_INITIALIZED and @ref
 *  PLFW_PLATFORM_UNAVAILABLE.
 *
 *  @thread_safety This function may be called from any thread.  Access is not
 *  synchronized.
 *
 *  @since Added in version 3.2.
 *
 *  @ingroup native
 */
PLFWAPI struct wl_surface* plfwGetWaylandWindow(PLFWwindow* window);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _plfw3_native_h_ */

