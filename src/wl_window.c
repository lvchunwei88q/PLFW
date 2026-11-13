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

#if !defined(_GNU_SOURCE)
 #define _GNU_SOURCE
#endif

#include "internal.h"

#if defined(_PLFW_WAYLAND)

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <poll.h>
#include <linux/input-event-codes.h>

#include "wayland-client-protocol.h"
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"
#include "idle-inhibit-unstable-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"

#define PLFW_BORDER_SIZE    4
#define PLFW_CAPTION_HEIGHT 24

#define PLFW_PENDING_SURFACE    1
#define PLFW_PENDING_BUTTON     2
#define PLFW_PENDING_MOTION     4
#define PLFW_PENDING_SCROLL     8
#define PLFW_PENDING_DISCRETE   16

static int createTmpfileCloexec(char* tmpname)
{
    int fd;

    fd = mkostemp(tmpname, O_CLOEXEC);
    if (fd >= 0)
        unlink(tmpname);

    return fd;
}

/*
 * Create a new, unique, anonymous file of the given size, and
 * return the file descriptor for it. The file descriptor is set
 * CLOEXEC. The file is immediately suitable for mmap()'ing
 * the given size at offset zero.
 *
 * The file should not have a permanent backing store like a disk,
 * but may have if XDG_RUNTIME_DIR is not properly implemented in OS.
 *
 * The file name is deleted from the file system.
 *
 * The file is suitable for buffer sharing between processes by
 * transmitting the file descriptor over Unix sockets using the
 * SCM_RIGHTS methods.
 *
 * posix_fallocate() is used to guarantee that disk space is available
 * for the file at the given size. If disk space is insufficient, errno
 * is set to ENOSPC. If posix_fallocate() is not supported, program may
 * receive SIGBUS on accessing mmap()'ed file contents instead.
 */
static int createAnonymousFile(off_t size)
{
    static const char template[] = "/plfw-shared-XXXXXX";
    const char* path;
    char* name;
    int fd;
    int ret;

#ifdef HAVE_MEMFD_CREATE
    fd = memfd_create("plfw-shared", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd >= 0)
    {
        // We can add this seal before calling posix_fallocate(), as the file
        // is currently zero-sized anyway.
        //
        // There is also no need to check for the return value, we couldn’t do
        // anything with it anyway.
        fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_SEAL);
    }
    else
#elif defined(SHM_ANON)
    fd = shm_open(SHM_ANON, O_RDWR | O_CLOEXEC, 0600);
    if (fd < 0)
#endif
    {
        path = getenv("XDG_RUNTIME_DIR");
        if (!path)
        {
            errno = ENOENT;
            return -1;
        }

        name = _plfw_calloc(strlen(path) + sizeof(template), 1);
        strcpy(name, path);
        strcat(name, template);

        fd = createTmpfileCloexec(name);
        _plfw_free(name);
        if (fd < 0)
            return -1;
    }

#if defined(SHM_ANON)
    // posix_fallocate does not work on SHM descriptors
    ret = ftruncate(fd, size);
#else
    ret = posix_fallocate(fd, 0, size);
#endif
    if (ret != 0)
    {
        close(fd);
        errno = ret;
        return -1;
    }
    return fd;
}

static struct wl_buffer* createShmBuffer(const PLFWimage* image)
{
    const int stride = image->width * 4;
    const int length = image->width * image->height * 4;

    const int fd = createAnonymousFile(length);
    if (fd < 0)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create buffer file of size %d: %s",
                        length, strerror(errno));
        return NULL;
    }

    void* data = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to map file: %s", strerror(errno));
        close(fd);
        return NULL;
    }

    struct wl_shm_pool* pool = wl_shm_create_pool(_plfw.wl.shm, fd, length);

    close(fd);

    unsigned char* source = (unsigned char*) image->pixels;
    unsigned char* target = data;
    for (int i = 0;  i < image->width * image->height;  i++, source += 4)
    {
        unsigned int alpha = source[3];

        *target++ = (unsigned char) ((source[2] * alpha) / 255);
        *target++ = (unsigned char) ((source[1] * alpha) / 255);
        *target++ = (unsigned char) ((source[0] * alpha) / 255);
        *target++ = (unsigned char) alpha;
    }

    struct wl_buffer* buffer =
        wl_shm_pool_create_buffer(pool, 0,
                                  image->width,
                                  image->height,
                                  stride, WL_SHM_FORMAT_ARGB8888);
    munmap(data, length);
    wl_shm_pool_destroy(pool);

    return buffer;
}

// A Wayland surface is only mapped once a buffer has been attached and committed,
// but this build has no graphics context and therefore never draws.  Without a
// buffer the compositor has nothing to show, the window stays unmapped and it can
// never receive pointer or keyboard focus.  Paint a flat opaque surface instead,
// which is what a windowing-only build should show.
static struct wl_buffer* createBlankBuffer(int width, int height)
{
    const int stride = width * 4;
    const int length = stride * height;

    const int fd = createAnonymousFile(length);
    if (fd < 0)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create window buffer file of size %d: %s",
                        length, strerror(errno));
        return NULL;
    }

    void* data = mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to map window buffer file: %s", strerror(errno));
        close(fd);
        return NULL;
    }

    struct wl_shm_pool* pool = wl_shm_create_pool(_plfw.wl.shm, fd, length);

    close(fd);

    // Opaque dark gray, premultiplied ARGB8888 (blue, green, red, alpha)
    unsigned char* target = data;
    for (int i = 0;  i < width * height;  i++)
    {
        *target++ = 0x20;
        *target++ = 0x20;
        *target++ = 0x20;
        *target++ = 0xff;
    }

    struct wl_buffer* buffer =
        wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_ARGB8888);
    munmap(data, length);
    wl_shm_pool_destroy(pool);

    return buffer;
}

// Attach (or keep attached) the blank buffer that makes the window exist on screen.
static void attachBlankBuffer(_PLFWwindow* window)
{
    if (!window->wl.surface || !_plfw.wl.shm || !window->wl.visible)
        return;

    const int scale = window->wl.bufferScale > 0 ? window->wl.bufferScale : 1;
    const int width  = window->wl.width * scale;
    const int height = window->wl.height * scale;

    if (width <= 0 || height <= 0)
        return;

    if (!window->wl.blankBuffer ||
        window->wl.blankWidth != width ||
        window->wl.blankHeight != height)
    {
        if (window->wl.blankBuffer)
            wl_buffer_destroy(window->wl.blankBuffer);

        window->wl.blankBuffer = createBlankBuffer(width, height);
        window->wl.blankWidth = width;
        window->wl.blankHeight = height;
    }

    if (!window->wl.blankBuffer)
        return;

    wl_surface_attach(window->wl.surface, window->wl.blankBuffer, 0, 0);
    wl_surface_damage(window->wl.surface, 0, 0, window->wl.width, window->wl.height);
    wl_surface_commit(window->wl.surface);
}

static void callbackHandleDone(void* userData, struct wl_callback* callback, uint32_t data)
{
    wl_callback_destroy(callback);
}
static const struct wl_callback_listener noopCallbackListener =
{
    callbackHandleDone
};

static void createFallbackEdge(_PLFWwindow* window,
                               _PLFWfallbackEdgeWayland* edge,
                               struct wl_surface* parent,
                               struct wl_buffer* buffer,
                               int x, int y,
                               int width, int height)
{
    edge->surface = wl_compositor_create_surface(_plfw.wl.compositor);
    wl_surface_set_user_data(edge->surface, window);
    wl_proxy_set_tag((struct wl_proxy*) edge->surface, &_plfw.wl.tag);
    edge->subsurface = wl_subcompositor_get_subsurface(_plfw.wl.subcompositor,
                                                       edge->surface, parent);
    wl_subsurface_set_position(edge->subsurface, x, y);
    edge->viewport = wp_viewporter_get_viewport(_plfw.wl.viewporter,
                                                edge->surface);
    wp_viewport_set_destination(edge->viewport, width, height);
    wl_surface_attach(edge->surface, buffer, 0, 0);

    struct wl_region* region = wl_compositor_create_region(_plfw.wl.compositor);
    wl_region_add(region, 0, 0, width, height);
    wl_surface_set_opaque_region(edge->surface, region);
    wl_surface_commit(edge->surface);
    wl_region_destroy(region);
}

static void createFallbackDecorations(_PLFWwindow* window)
{
    unsigned char data[] = { 224, 224, 224, 255 };
    const PLFWimage image = { 1, 1, data };

    if (!_plfw.wl.viewporter)
        return;

    if (!window->wl.fallback.buffer)
        window->wl.fallback.buffer = createShmBuffer(&image);
    if (!window->wl.fallback.buffer)
        return;

    createFallbackEdge(window, &window->wl.fallback.top, window->wl.surface,
                       window->wl.fallback.buffer,
                       0, -PLFW_CAPTION_HEIGHT,
                       window->wl.width, PLFW_CAPTION_HEIGHT);
    createFallbackEdge(window, &window->wl.fallback.left, window->wl.surface,
                       window->wl.fallback.buffer,
                       -PLFW_BORDER_SIZE, -PLFW_CAPTION_HEIGHT,
                       PLFW_BORDER_SIZE, window->wl.height + PLFW_CAPTION_HEIGHT);
    createFallbackEdge(window, &window->wl.fallback.right, window->wl.surface,
                       window->wl.fallback.buffer,
                       window->wl.width, -PLFW_CAPTION_HEIGHT,
                       PLFW_BORDER_SIZE, window->wl.height + PLFW_CAPTION_HEIGHT);
    createFallbackEdge(window, &window->wl.fallback.bottom, window->wl.surface,
                       window->wl.fallback.buffer,
                       -PLFW_BORDER_SIZE, window->wl.height,
                       window->wl.width + PLFW_BORDER_SIZE * 2, PLFW_BORDER_SIZE);

    window->wl.fallback.decorations = PLFW_TRUE;
}

static void destroyFallbackEdge(_PLFWfallbackEdgeWayland* edge)
{
    if (edge->surface == _plfw.wl.pointerSurface)
        _plfw.wl.pointerSurface = NULL;

    if (edge->subsurface)
        wl_subsurface_destroy(edge->subsurface);
    if (edge->surface)
        wl_surface_destroy(edge->surface);
    if (edge->viewport)
        wp_viewport_destroy(edge->viewport);

    edge->surface = NULL;
    edge->subsurface = NULL;
    edge->viewport = NULL;
}

static void destroyFallbackDecorations(_PLFWwindow* window)
{
    window->wl.fallback.decorations = PLFW_FALSE;

    destroyFallbackEdge(&window->wl.fallback.top);
    destroyFallbackEdge(&window->wl.fallback.left);
    destroyFallbackEdge(&window->wl.fallback.right);
    destroyFallbackEdge(&window->wl.fallback.bottom);
}

static void updateFallbackDecorationCursor(_PLFWwindow* window, double xpos, double ypos)
{
    window->wl.fallback.pointerX = xpos;
    window->wl.fallback.pointerY = ypos;

    const char* cursorName = "left_ptr";

    if (window->resizable)
    {
        if (_plfw.wl.pointerSurface == window->wl.fallback.top.surface)
        {
            if (ypos < PLFW_BORDER_SIZE)
                cursorName = "n-resize";
        }
        else if (_plfw.wl.pointerSurface == window->wl.fallback.left.surface)
        {
            if (ypos < PLFW_BORDER_SIZE)
                cursorName = "nw-resize";
            else
                cursorName = "w-resize";
        }
        else if (_plfw.wl.pointerSurface == window->wl.fallback.right.surface)
        {
            if (ypos < PLFW_BORDER_SIZE)
                cursorName = "ne-resize";
            else
                cursorName = "e-resize";
        }
        else if (_plfw.wl.pointerSurface == window->wl.fallback.bottom.surface)
        {
            if (xpos < PLFW_BORDER_SIZE)
                cursorName = "sw-resize";
            else if (xpos > window->wl.width + PLFW_BORDER_SIZE)
                cursorName = "se-resize";
            else
                cursorName = "s-resize";
        }
    }

    if (window->wl.fallback.cursorName != cursorName)
    {
        struct wl_surface* surface = _plfw.wl.cursorSurface;
        struct wl_cursor_theme* theme = _plfw.wl.cursorTheme;
        int scale = 1;

        if (window->wl.bufferScale > 1 && _plfw.wl.cursorThemeHiDPI)
        {
            // We only support up to scale=2 for now, since libwayland-cursor
            // requires us to load a different theme for each size.
            scale = 2;
            theme = _plfw.wl.cursorThemeHiDPI;
        }

        struct wl_cursor* cursor = wl_cursor_theme_get_cursor(theme, cursorName);
        if (!cursor)
            return;

        // TODO: handle animated cursors too.
        struct wl_cursor_image* image = cursor->images[0];
        if (!image)
            return;

        struct wl_buffer* buffer = wl_cursor_image_get_buffer(image);
        if (!buffer)
            return;

        wl_pointer_set_cursor(_plfw.wl.pointer, _plfw.wl.pointerEnterSerial,
                              surface,
                              image->hotspot_x / scale,
                              image->hotspot_y / scale);
        wl_surface_set_buffer_scale(surface, scale);
        wl_surface_attach(surface, buffer, 0, 0);
        wl_surface_damage(surface, 0, 0, image->width, image->height);
        wl_surface_commit(surface);

        window->wl.fallback.cursorName = cursorName;
    }
}

static void handleFallbackDecorationButton(_PLFWwindow* window, int button, int action)
{
    if (action != PLFW_PRESS)
        return;

    const double xpos = window->wl.fallback.pointerX;
    const double ypos = window->wl.fallback.pointerY;
    const uint32_t serial = window->wl.fallback.buttonPressSerial;

    if (button == PLFW_MOUSE_BUTTON_LEFT)
    {
        uint32_t edges = XDG_TOPLEVEL_RESIZE_EDGE_NONE;

        if (_plfw.wl.pointerSurface == window->wl.fallback.top.surface)
        {
            if (ypos < PLFW_BORDER_SIZE)
                edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP;
            else
                xdg_toplevel_move(window->wl.xdg.toplevel, _plfw.wl.seat, serial);
        }
        else if (_plfw.wl.pointerSurface == window->wl.fallback.left.surface)
        {
            if (ypos < PLFW_BORDER_SIZE)
                edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_LEFT;
            else
                edges = XDG_TOPLEVEL_RESIZE_EDGE_LEFT;
        }
        else if (_plfw.wl.pointerSurface == window->wl.fallback.right.surface)
        {
            if (ypos < PLFW_BORDER_SIZE)
                edges = XDG_TOPLEVEL_RESIZE_EDGE_TOP_RIGHT;
            else
                edges = XDG_TOPLEVEL_RESIZE_EDGE_RIGHT;
        }
        else if (_plfw.wl.pointerSurface == window->wl.fallback.bottom.surface)
        {
            if (xpos < PLFW_BORDER_SIZE)
                edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_LEFT;
            else if (xpos > window->wl.width + PLFW_BORDER_SIZE)
                edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM_RIGHT;
            else
                edges = XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM;
        }

        if (edges != XDG_TOPLEVEL_RESIZE_EDGE_NONE)
            xdg_toplevel_resize(window->wl.xdg.toplevel, _plfw.wl.seat, serial, edges);
    }
    else if (button == PLFW_MOUSE_BUTTON_RIGHT)
    {
        if (!window->wl.xdg.toplevel)
            return;

        if (_plfw.wl.pointerSurface != window->wl.fallback.top.surface)
            return;

        if (ypos < PLFW_BORDER_SIZE)
            return;

        xdg_toplevel_show_window_menu(window->wl.xdg.toplevel, _plfw.wl.seat, serial,
                                      xpos, ypos - PLFW_CAPTION_HEIGHT - PLFW_BORDER_SIZE);
    }
}

static void xdgDecorationHandleConfigure(void* userData,
                                         struct zxdg_toplevel_decoration_v1* decoration,
                                         uint32_t mode)
{
    _PLFWwindow* window = userData;

    window->wl.xdg.decorationMode = mode;

    if (mode == ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE)
    {
        if (window->decorated && !window->monitor)
            createFallbackDecorations(window);
    }
    else
        destroyFallbackDecorations(window);
}

static const struct zxdg_toplevel_decoration_v1_listener xdgDecorationListener =
{
    xdgDecorationHandleConfigure,
};

// Makes the surface considered as XRGB instead of ARGB.
static void setContentAreaOpaque(_PLFWwindow* window)
{
    struct wl_region* region;

    region = wl_compositor_create_region(_plfw.wl.compositor);
    if (!region)
        return;

    wl_region_add(region, 0, 0, window->wl.width, window->wl.height);
    wl_surface_set_opaque_region(window->wl.surface, region);
    wl_region_destroy(region);
}

static void resizeFramebuffer(_PLFWwindow* window)
{
    if (window->wl.fractionalScale)
    {
        // Round halfway away from zero per fractional-scale-v1 protocol spec
        window->wl.fbWidth = (window->wl.width * window->wl.scalingNumerator + 60) / 120;
        window->wl.fbHeight = (window->wl.height * window->wl.scalingNumerator + 60) / 120;
    }
    else
    {
        window->wl.fbWidth = window->wl.width * window->wl.bufferScale;
        window->wl.fbHeight = window->wl.height * window->wl.bufferScale;
    }

    setContentAreaOpaque(window);

    _plfwInputFramebufferSize(window, window->wl.fbWidth, window->wl.fbHeight);
}

static PLFWbool resizeWindow(_PLFWwindow* window, int width, int height)
{
    width = _plfw_max(width, 1);
    height = _plfw_max(height, 1);

    if (width == window->wl.width && height == window->wl.height)
        return PLFW_FALSE;

    window->wl.width = width;
    window->wl.height = height;

    resizeFramebuffer(window);

    if (window->wl.scalingViewport)
    {
        wp_viewport_set_destination(window->wl.scalingViewport,
                                    window->wl.width,
                                    window->wl.height);
    }

    if (window->wl.fallback.decorations)
    {
        wp_viewport_set_destination(window->wl.fallback.top.viewport,
                                    window->wl.width,
                                    PLFW_CAPTION_HEIGHT);
        wl_surface_commit(window->wl.fallback.top.surface);

        wp_viewport_set_destination(window->wl.fallback.left.viewport,
                                    PLFW_BORDER_SIZE,
                                    window->wl.height + PLFW_CAPTION_HEIGHT);
        wl_surface_commit(window->wl.fallback.left.surface);

        wl_subsurface_set_position(window->wl.fallback.right.subsurface,
                                window->wl.width, -PLFW_CAPTION_HEIGHT);
        wp_viewport_set_destination(window->wl.fallback.right.viewport,
                                    PLFW_BORDER_SIZE,
                                    window->wl.height + PLFW_CAPTION_HEIGHT);
        wl_surface_commit(window->wl.fallback.right.surface);

        wl_subsurface_set_position(window->wl.fallback.bottom.subsurface,
                                -PLFW_BORDER_SIZE, window->wl.height);
        wp_viewport_set_destination(window->wl.fallback.bottom.viewport,
                                    window->wl.width + PLFW_BORDER_SIZE * 2,
                                    PLFW_BORDER_SIZE);
        wl_surface_commit(window->wl.fallback.bottom.surface);
    }

    return PLFW_TRUE;
}

void _plfwUpdateBufferScaleFromOutputsWayland(_PLFWwindow* window)
{
    if (wl_compositor_get_version(_plfw.wl.compositor) <
        WL_SURFACE_SET_BUFFER_SCALE_SINCE_VERSION)
    {
        return;
    }

    if (!window->wl.scaleFramebuffer)
        return;

    // When using fractional scaling, the buffer scale should remain at 1
    if (window->wl.fractionalScale)
        return;

    // Get the scale factor from the highest scale monitor.
    int32_t maxScale = 1;

    for (size_t i = 0; i < window->wl.outputScaleCount; i++)
        maxScale = _plfw_max(window->wl.outputScales[i].factor, maxScale);

    // Only change the framebuffer size if the scale changed.
    if (window->wl.bufferScale != maxScale)
    {
        window->wl.bufferScale = maxScale;
        wl_surface_set_buffer_scale(window->wl.surface, maxScale);
        _plfwInputWindowContentScale(window, maxScale, maxScale);
        resizeFramebuffer(window);

        if (window->wl.visible)
            _plfwInputWindowDamage(window);
    }
}

static void surfaceHandleEnter(void* userData,
                               struct wl_surface* surface,
                               struct wl_output* output)
{
    if (wl_proxy_get_tag((struct wl_proxy*) output) != &_plfw.wl.tag)
        return;

    _PLFWwindow* window = userData;
    _PLFWmonitor* monitor = wl_output_get_user_data(output);
    if (!window || !monitor)
        return;

    if (window->wl.outputScaleCount + 1 > window->wl.outputScaleSize)
    {
        window->wl.outputScaleSize++;
        window->wl.outputScales =
            _plfw_realloc(window->wl.outputScales,
                          window->wl.outputScaleSize * sizeof(_PLFWscaleWayland));
    }

    window->wl.outputScaleCount++;
    window->wl.outputScales[window->wl.outputScaleCount - 1] =
        (_PLFWscaleWayland) { output, monitor->wl.scale };

    _plfwUpdateBufferScaleFromOutputsWayland(window);
}

static void surfaceHandleLeave(void* userData,
                               struct wl_surface* surface,
                               struct wl_output* output)
{
    if (wl_proxy_get_tag((struct wl_proxy*) output) != &_plfw.wl.tag)
        return;

    _PLFWwindow* window = userData;

    for (size_t i = 0; i < window->wl.outputScaleCount; i++)
    {
        if (window->wl.outputScales[i].output == output)
        {
            window->wl.outputScales[i] =
                window->wl.outputScales[window->wl.outputScaleCount - 1];
            window->wl.outputScaleCount--;
            break;
        }
    }

    _plfwUpdateBufferScaleFromOutputsWayland(window);
}

static const struct wl_surface_listener surfaceListener =
{
    surfaceHandleEnter,
    surfaceHandleLeave
};

static void setIdleInhibitor(_PLFWwindow* window, PLFWbool enable)
{
    if (enable && !window->wl.idleInhibitor && _plfw.wl.idleInhibitManager)
    {
        window->wl.idleInhibitor =
            zwp_idle_inhibit_manager_v1_create_inhibitor(
                _plfw.wl.idleInhibitManager, window->wl.surface);
        if (!window->wl.idleInhibitor)
            _plfwInputError(PLFW_PLATFORM_ERROR,
                            "Wayland: Failed to create idle inhibitor");
    }
    else if (!enable && window->wl.idleInhibitor)
    {
        zwp_idle_inhibitor_v1_destroy(window->wl.idleInhibitor);
        window->wl.idleInhibitor = NULL;
    }
}

// Make the specified window and its video mode active on its monitor
//
static void acquireMonitor(_PLFWwindow* window)
{
    if (window->wl.libdecor.frame)
    {
        libdecor_frame_set_fullscreen(window->wl.libdecor.frame,
                                      window->monitor->wl.output);
    }
    else if (window->wl.xdg.toplevel)
    {
        xdg_toplevel_set_fullscreen(window->wl.xdg.toplevel,
                                    window->monitor->wl.output);
    }

    setIdleInhibitor(window, PLFW_TRUE);

    if (window->wl.fallback.decorations)
        destroyFallbackDecorations(window);
}

// Remove the window and restore the original video mode
//
static void releaseMonitor(_PLFWwindow* window)
{
    if (window->wl.libdecor.frame)
        libdecor_frame_unset_fullscreen(window->wl.libdecor.frame);
    else if (window->wl.xdg.toplevel)
        xdg_toplevel_unset_fullscreen(window->wl.xdg.toplevel);

    setIdleInhibitor(window, PLFW_FALSE);

    if (!window->wl.libdecor.frame &&
        window->wl.xdg.decorationMode != ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE)
    {
        if (window->decorated)
            createFallbackDecorations(window);
    }
}

void fractionalScaleHandlePreferredScale(void* userData,
                                         struct wp_fractional_scale_v1* fractionalScale,
                                         uint32_t numerator)
{
    _PLFWwindow* window = userData;

    window->wl.scalingNumerator = numerator;
    _plfwInputWindowContentScale(window, numerator / 120.f, numerator / 120.f);
    resizeFramebuffer(window);

    if (window->wl.visible)
        _plfwInputWindowDamage(window);
}

const struct wp_fractional_scale_v1_listener fractionalScaleListener =
{
    fractionalScaleHandlePreferredScale,
};

static void xdgToplevelHandleConfigure(void* userData,
                                       struct xdg_toplevel* toplevel,
                                       int32_t width,
                                       int32_t height,
                                       struct wl_array* states)
{
    _PLFWwindow* window = userData;
    uint32_t* state;

    window->wl.pending.activated  = PLFW_FALSE;
    window->wl.pending.maximized  = PLFW_FALSE;
    window->wl.pending.fullscreen = PLFW_FALSE;

    wl_array_for_each(state, states)
    {
        switch (*state)
        {
            case XDG_TOPLEVEL_STATE_MAXIMIZED:
                window->wl.pending.maximized = PLFW_TRUE;
                break;
            case XDG_TOPLEVEL_STATE_FULLSCREEN:
                window->wl.pending.fullscreen = PLFW_TRUE;
                break;
            case XDG_TOPLEVEL_STATE_RESIZING:
                break;
            case XDG_TOPLEVEL_STATE_ACTIVATED:
                window->wl.pending.activated = PLFW_TRUE;
                break;
        }
    }

    if (width && height)
    {
        if (window->wl.fallback.decorations)
        {
            window->wl.pending.width  = _plfw_max(0, width - PLFW_BORDER_SIZE * 2);
            window->wl.pending.height =
                _plfw_max(0, height - PLFW_BORDER_SIZE - PLFW_CAPTION_HEIGHT);
        }
        else
        {
            window->wl.pending.width  = width;
            window->wl.pending.height = height;
        }
    }
    else
    {
        window->wl.pending.width  = window->wl.width;
        window->wl.pending.height = window->wl.height;
    }
}

static void xdgToplevelHandleClose(void* userData,
                                   struct xdg_toplevel* toplevel)
{
    _PLFWwindow* window = userData;
    _plfwInputWindowCloseRequest(window);
}

static const struct xdg_toplevel_listener xdgToplevelListener =
{
    xdgToplevelHandleConfigure,
    xdgToplevelHandleClose
};

static void xdgSurfaceHandleConfigure(void* userData,
                                      struct xdg_surface* surface,
                                      uint32_t serial)
{
    _PLFWwindow* window = userData;

    xdg_surface_ack_configure(surface, serial);

    if (window->wl.activated != window->wl.pending.activated)
    {
        window->wl.activated = window->wl.pending.activated;
        if (!window->wl.activated)
        {
            if (window->monitor && window->autoIconify)
                xdg_toplevel_set_minimized(window->wl.xdg.toplevel);
        }
    }

    if (window->wl.maximized != window->wl.pending.maximized)
    {
        window->wl.maximized = window->wl.pending.maximized;
        _plfwInputWindowMaximize(window, window->wl.maximized);
    }

    window->wl.fullscreen = window->wl.pending.fullscreen;

    int width  = window->wl.pending.width;
    int height = window->wl.pending.height;

    if (!window->wl.maximized && !window->wl.fullscreen)
    {
        if (window->numer != PLFW_DONT_CARE && window->denom != PLFW_DONT_CARE)
        {
            const float aspectRatio = (float) width / (float) height;
            const float targetRatio = (float) window->numer / (float) window->denom;
            if (aspectRatio < targetRatio)
                height = width / targetRatio;
            else if (aspectRatio > targetRatio)
                width = height * targetRatio;
        }
    }

    if (resizeWindow(window, width, height))
    {
        _plfwInputWindowSize(window, window->wl.width, window->wl.height);

        if (window->wl.visible)
            _plfwInputWindowDamage(window);
    }

    if (!window->wl.visible)
    {
        // Allow the window to be mapped only if it either has no XDG
        // decorations or they have already received a configure event
        if (!window->wl.xdg.decoration || window->wl.xdg.decorationMode)
        {
            window->wl.visible = PLFW_TRUE;
            _plfwInputWindowDamage(window);
        }
    }

    if (window->wl.visible)
        attachBlankBuffer(window);
}

static const struct xdg_surface_listener xdgSurfaceListener =
{
    xdgSurfaceHandleConfigure
};

void libdecorFrameHandleConfigure(struct libdecor_frame* frame,
                                  struct libdecor_configuration* config,
                                  void* userData)
{
    _PLFWwindow* window = userData;
    int width, height;

    enum libdecor_window_state windowState;
    PLFWbool fullscreen, activated, maximized;

    if (libdecor_configuration_get_window_state(config, &windowState))
    {
        fullscreen = (windowState & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0;
        activated = (windowState & LIBDECOR_WINDOW_STATE_ACTIVE) != 0;
        maximized = (windowState & LIBDECOR_WINDOW_STATE_MAXIMIZED) != 0;
    }
    else
    {
        fullscreen = window->wl.fullscreen;
        activated = window->wl.activated;
        maximized = window->wl.maximized;
    }

    if (!libdecor_configuration_get_content_size(config, frame, &width, &height))
    {
        width = window->wl.width;
        height = window->wl.height;
    }

    if (!maximized && !fullscreen)
    {
        if (window->numer != PLFW_DONT_CARE && window->denom != PLFW_DONT_CARE)
        {
            const float aspectRatio = (float) width / (float) height;
            const float targetRatio = (float) window->numer / (float) window->denom;
            if (aspectRatio < targetRatio)
                height = width / targetRatio;
            else if (aspectRatio > targetRatio)
                width = height * targetRatio;
        }
    }

    struct libdecor_state* frameState = libdecor_state_new(width, height);
    libdecor_frame_commit(frame, frameState, config);
    libdecor_state_free(frameState);

    // NOTE: Frame visibility must only be set after a frame state has been committed
    if (window->decorated != libdecor_frame_is_visible(window->wl.libdecor.frame))
        libdecor_frame_set_visibility(window->wl.libdecor.frame, window->decorated);

    if (window->wl.activated != activated)
    {
        window->wl.activated = activated;
        if (!window->wl.activated)
        {
            if (window->monitor && window->autoIconify)
                libdecor_frame_set_minimized(window->wl.libdecor.frame);
        }
    }

    if (window->wl.maximized != maximized)
    {
        window->wl.maximized = maximized;
        _plfwInputWindowMaximize(window, window->wl.maximized);
    }

    window->wl.fullscreen = fullscreen;

    PLFWbool damaged = PLFW_FALSE;

    if (!window->wl.visible)
    {
        window->wl.visible = PLFW_TRUE;
        damaged = PLFW_TRUE;
    }

    if (resizeWindow(window, width, height))
    {
        _plfwInputWindowSize(window, window->wl.width, window->wl.height);
        damaged = PLFW_TRUE;
    }

    if (damaged)
        _plfwInputWindowDamage(window);
    else
        wl_surface_commit(window->wl.surface);

    if (window->wl.visible)
        attachBlankBuffer(window);
}

void libdecorFrameHandleClose(struct libdecor_frame* frame, void* userData)
{
    _PLFWwindow* window = userData;
    _plfwInputWindowCloseRequest(window);
}

void libdecorFrameHandleCommit(struct libdecor_frame* frame, void* userData)
{
    _PLFWwindow* window = userData;
    wl_surface_commit(window->wl.surface);
}

void libdecorFrameHandleDismissPopup(struct libdecor_frame* frame,
                                     const char* seatName,
                                     void* userData)
{
}

static const struct libdecor_frame_interface libdecorFrameInterface =
{
    libdecorFrameHandleConfigure,
    libdecorFrameHandleClose,
    libdecorFrameHandleCommit,
    libdecorFrameHandleDismissPopup
};

static PLFWbool createLibdecorFrame(_PLFWwindow* window)
{
    // Allow libdecor to finish initialization of itself and its plugin
    while (!_plfw.wl.libdecor.ready)
        _plfwWaitEventsWayland();

    window->wl.libdecor.frame = libdecor_decorate(_plfw.wl.libdecor.context,
                                                  window->wl.surface,
                                                  &libdecorFrameInterface,
                                                  window);
    if (!window->wl.libdecor.frame)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create libdecor frame");
        return PLFW_FALSE;
    }

    if (strlen(window->wl.appId))
        libdecor_frame_set_app_id(window->wl.libdecor.frame, window->wl.appId);

    libdecor_frame_set_title(window->wl.libdecor.frame, window->title);

    if (window->resizable)
    {
        if (window->minwidth != PLFW_DONT_CARE &&
            window->minheight != PLFW_DONT_CARE)
        {
            libdecor_frame_set_min_content_size(window->wl.libdecor.frame,
                                                window->minwidth,
                                                window->minheight);
        }

        if (window->maxwidth != PLFW_DONT_CARE &&
            window->maxheight != PLFW_DONT_CARE)
        {
            libdecor_frame_set_max_content_size(window->wl.libdecor.frame,
                                                window->maxwidth,
                                                window->maxheight);
        }
    }
    else
    {
        libdecor_frame_unset_capabilities(window->wl.libdecor.frame,
                                          LIBDECOR_ACTION_RESIZE);
        libdecor_frame_set_min_content_size(window->wl.libdecor.frame,
                                            window->wl.width,
                                            window->wl.height);
        libdecor_frame_set_max_content_size(window->wl.libdecor.frame,
                                            window->wl.width,
                                            window->wl.height);
    }

    if (window->monitor)
    {
        libdecor_frame_set_fullscreen(window->wl.libdecor.frame,
                                      window->monitor->wl.output);
        setIdleInhibitor(window, PLFW_TRUE);
    }
    else
    {
        // Frame visibility is applied in libdecorFrameHandleConfigure

        if (window->wl.maximized)
            libdecor_frame_set_maximized(window->wl.libdecor.frame);

        setIdleInhibitor(window, PLFW_FALSE);
    }

    libdecor_frame_map(window->wl.libdecor.frame);
    wl_display_roundtrip(_plfw.wl.display);
    return PLFW_TRUE;
}

static void updateXdgSizeLimits(_PLFWwindow* window)
{
    int minwidth, minheight, maxwidth, maxheight;

    if (window->resizable)
    {
        if (window->minwidth == PLFW_DONT_CARE || window->minheight == PLFW_DONT_CARE)
            minwidth = minheight = 0;
        else
        {
            minwidth  = window->minwidth;
            minheight = window->minheight;

            if (window->wl.fallback.decorations)
            {
                minwidth  += PLFW_BORDER_SIZE * 2;
                minheight += PLFW_CAPTION_HEIGHT + PLFW_BORDER_SIZE;
            }
        }

        if (window->maxwidth == PLFW_DONT_CARE || window->maxheight == PLFW_DONT_CARE)
            maxwidth = maxheight = 0;
        else
        {
            maxwidth  = window->maxwidth;
            maxheight = window->maxheight;

            if (window->wl.fallback.decorations)
            {
                maxwidth  += PLFW_BORDER_SIZE * 2;
                maxheight += PLFW_CAPTION_HEIGHT + PLFW_BORDER_SIZE;
            }
        }
    }
    else
    {
        minwidth = maxwidth = window->wl.width;
        minheight = maxheight = window->wl.height;
    }

    xdg_toplevel_set_min_size(window->wl.xdg.toplevel, minwidth, minheight);
    xdg_toplevel_set_max_size(window->wl.xdg.toplevel, maxwidth, maxheight);
}

static PLFWbool createXdgShellObjects(_PLFWwindow* window)
{
    window->wl.xdg.surface = xdg_wm_base_get_xdg_surface(_plfw.wl.wmBase,
                                                         window->wl.surface);
    if (!window->wl.xdg.surface)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create xdg-surface for window");
        return PLFW_FALSE;
    }

    xdg_surface_add_listener(window->wl.xdg.surface, &xdgSurfaceListener, window);

    window->wl.xdg.toplevel = xdg_surface_get_toplevel(window->wl.xdg.surface);
    if (!window->wl.xdg.toplevel)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create xdg-toplevel for window");
        return PLFW_FALSE;
    }

    xdg_toplevel_add_listener(window->wl.xdg.toplevel, &xdgToplevelListener, window);

    if (window->wl.appId)
        xdg_toplevel_set_app_id(window->wl.xdg.toplevel, window->wl.appId);

    xdg_toplevel_set_title(window->wl.xdg.toplevel, window->title);

    if (window->monitor)
    {
        xdg_toplevel_set_fullscreen(window->wl.xdg.toplevel, window->monitor->wl.output);
        setIdleInhibitor(window, PLFW_TRUE);
    }
    else
    {
        if (window->wl.maximized)
            xdg_toplevel_set_maximized(window->wl.xdg.toplevel);

        setIdleInhibitor(window, PLFW_FALSE);
    }

    if (_plfw.wl.decorationManager)
    {
        window->wl.xdg.decoration =
            zxdg_decoration_manager_v1_get_toplevel_decoration(
                _plfw.wl.decorationManager, window->wl.xdg.toplevel);
        zxdg_toplevel_decoration_v1_add_listener(window->wl.xdg.decoration,
                                                 &xdgDecorationListener,
                                                 window);

        uint32_t mode;

        if (window->decorated)
            mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        else
            mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;

        zxdg_toplevel_decoration_v1_set_mode(window->wl.xdg.decoration, mode);
    }
    else
    {
        if (window->decorated && !window->monitor)
            createFallbackDecorations(window);
    }

    updateXdgSizeLimits(window);

    wl_surface_commit(window->wl.surface);
    wl_display_roundtrip(_plfw.wl.display);
    return PLFW_TRUE;
}

static PLFWbool createShellObjects(_PLFWwindow* window)
{
    if (_plfw.wl.libdecor.context)
    {
        if (createLibdecorFrame(window))
            return PLFW_TRUE;
    }

    return createXdgShellObjects(window);
}

static void destroyShellObjects(_PLFWwindow* window)
{
    destroyFallbackDecorations(window);

    if (window->wl.libdecor.frame)
        libdecor_frame_unref(window->wl.libdecor.frame);

    if (window->wl.xdg.decoration)
        zxdg_toplevel_decoration_v1_destroy(window->wl.xdg.decoration);

    if (window->wl.xdg.toplevel)
        xdg_toplevel_destroy(window->wl.xdg.toplevel);

    if (window->wl.xdg.surface)
        xdg_surface_destroy(window->wl.xdg.surface);

    window->wl.libdecor.frame = NULL;
    window->wl.xdg.decoration = NULL;
    window->wl.xdg.decorationMode = 0;
    window->wl.xdg.toplevel = NULL;
    window->wl.xdg.surface = NULL;
}

static PLFWbool createNativeSurface(_PLFWwindow* window,
                                    const _PLFWwndconfig* wndconfig,
                                    const _PLFWfbconfig* fbconfig)
{
    window->wl.surface = wl_compositor_create_surface(_plfw.wl.compositor);
    if (!window->wl.surface)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR, "Wayland: Failed to create window surface");
        return PLFW_FALSE;
    }

    wl_proxy_set_tag((struct wl_proxy*) window->wl.surface, &_plfw.wl.tag);
    wl_surface_add_listener(window->wl.surface,
                            &surfaceListener,
                            window);

    window->wl.width = wndconfig->width;
    window->wl.height = wndconfig->height;
    window->wl.fbWidth = wndconfig->width;
    window->wl.fbHeight = wndconfig->height;
    window->wl.appId = _plfw_strdup(wndconfig->wl.appId);

    window->wl.bufferScale = 1;
    window->wl.scalingNumerator = 120;
    window->wl.scaleFramebuffer = wndconfig->scaleFramebuffer;

    window->wl.maximized = wndconfig->maximized;

    setContentAreaOpaque(window);

    if (_plfw.wl.fractionalScaleManager)
    {
        if (window->wl.scaleFramebuffer)
        {
            window->wl.scalingViewport =
                wp_viewporter_get_viewport(_plfw.wl.viewporter, window->wl.surface);

            wp_viewport_set_destination(window->wl.scalingViewport,
                                        window->wl.width,
                                        window->wl.height);

            window->wl.fractionalScale =
                wp_fractional_scale_manager_v1_get_fractional_scale(
                    _plfw.wl.fractionalScaleManager,
                    window->wl.surface);

            wp_fractional_scale_v1_add_listener(window->wl.fractionalScale,
                                                &fractionalScaleListener,
                                                window);
        }
    }

    return PLFW_TRUE;
}

static void setCursorImage(_PLFWwindow* window,
                           _PLFWcursorWayland* cursorWayland)
{
    struct itimerspec timer = {0};
    struct wl_cursor* wlCursor = cursorWayland->cursor;
    struct wl_cursor_image* image;
    struct wl_buffer* buffer;
    struct wl_surface* surface = _plfw.wl.cursorSurface;
    int scale = 1;

    if (!wlCursor)
        buffer = cursorWayland->buffer;
    else
    {
        if (window->wl.bufferScale > 1 && cursorWayland->cursorHiDPI)
        {
            wlCursor = cursorWayland->cursorHiDPI;
            scale = 2;
        }

        image = wlCursor->images[cursorWayland->currentImage];
        buffer = wl_cursor_image_get_buffer(image);
        if (!buffer)
            return;

        timer.it_value.tv_sec = image->delay / 1000;
        timer.it_value.tv_nsec = (image->delay % 1000) * 1000000;
        timerfd_settime(_plfw.wl.cursorTimerfd, 0, &timer, NULL);

        cursorWayland->width = image->width;
        cursorWayland->height = image->height;
        cursorWayland->xhot = image->hotspot_x;
        cursorWayland->yhot = image->hotspot_y;
    }

    wl_pointer_set_cursor(_plfw.wl.pointer, _plfw.wl.pointerEnterSerial,
                          surface,
                          cursorWayland->xhot / scale,
                          cursorWayland->yhot / scale);
    wl_surface_set_buffer_scale(surface, scale);
    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_damage(surface, 0, 0,
                      cursorWayland->width, cursorWayland->height);
    wl_surface_commit(surface);
}

static void incrementCursorImage(void)
{
    if (!_plfw.wl.pointerSurface)
        return;

    _PLFWwindow* window = wl_surface_get_user_data(_plfw.wl.pointerSurface);
    if (window->wl.surface != _plfw.wl.pointerSurface)
        return;

    _PLFWcursor* cursor = window->cursor;
    if (cursor && cursor->wl.cursor)
    {
        cursor->wl.currentImage += 1;
        cursor->wl.currentImage %= cursor->wl.cursor->image_count;
        setCursorImage(window, &cursor->wl);
    }
}

static PLFWbool flushDisplay(void)
{
    while (wl_display_flush(_plfw.wl.display) == -1)
    {
        if (errno != EAGAIN)
            return PLFW_FALSE;

        struct pollfd fd = { wl_display_get_fd(_plfw.wl.display), POLLOUT };

        while (poll(&fd, 1, -1) == -1)
        {
            if (errno != EINTR && errno != EAGAIN)
                return PLFW_FALSE;
        }
    }

    return PLFW_TRUE;
}

static int translateKey(uint32_t scancode)
{
    if (scancode < sizeof(_plfw.wl.keycodes) / sizeof(_plfw.wl.keycodes[0]))
        return _plfw.wl.keycodes[scancode];

    return PLFW_KEY_UNKNOWN;
}

static xkb_keysym_t composeSymbol(xkb_keysym_t sym)
{
    if (sym == XKB_KEY_NoSymbol || !_plfw.wl.xkb.composeState)
        return sym;
    if (xkb_compose_state_feed(_plfw.wl.xkb.composeState, sym)
            != XKB_COMPOSE_FEED_ACCEPTED)
        return sym;
    switch (xkb_compose_state_get_status(_plfw.wl.xkb.composeState))
    {
        case XKB_COMPOSE_COMPOSED:
            return xkb_compose_state_get_one_sym(_plfw.wl.xkb.composeState);
        case XKB_COMPOSE_COMPOSING:
        case XKB_COMPOSE_CANCELLED:
            return XKB_KEY_NoSymbol;
        case XKB_COMPOSE_NOTHING:
        default:
            return sym;
    }
}

static void inputText(_PLFWwindow* window, uint32_t scancode)
{
    const xkb_keysym_t* keysyms;
    const xkb_keycode_t keycode = scancode + 8;

    if (xkb_state_key_get_syms(_plfw.wl.xkb.state, keycode, &keysyms) == 1)
    {
        const xkb_keysym_t keysym = composeSymbol(keysyms[0]);
        const uint32_t codepoint = xkb_keysym_to_utf32(keysym);
        if (codepoint != 0)
        {
            const int mods = _plfw.wl.xkb.modifiers;
            const int plain = !(mods & (PLFW_MOD_CONTROL | PLFW_MOD_ALT));
            _plfwInputChar(window, codepoint, mods, plain);
        }
    }
}

static void handleEvents(double* timeout)
{
#if defined(PLFW_BUILD_LINUX_JOYSTICK)
    if (_plfw.joysticksInitialized)
        _plfwDetectJoystickConnectionLinux();
#endif

    PLFWbool event = PLFW_FALSE;
    enum { DISPLAY_FD, KEYREPEAT_FD, CURSOR_FD };
    struct pollfd fds[] =
    {
        [DISPLAY_FD] = { wl_display_get_fd(_plfw.wl.display), POLLIN },
        [KEYREPEAT_FD] = { _plfw.wl.keyRepeatTimerfd, POLLIN },
        [CURSOR_FD] = { _plfw.wl.cursorTimerfd, POLLIN }
    };

    while (!event)
    {
        if (_plfw.wl.libdecor.context)
        {
            // Dispatch unconditionally because it also processes non-Wayland events
            if (libdecor_dispatch(_plfw.wl.libdecor.context, 0) > 0)
                event = PLFW_TRUE;
        }

        while (wl_display_prepare_read(_plfw.wl.display) != 0)
        {
            if (wl_display_dispatch_pending(_plfw.wl.display) > 0)
                event = PLFW_TRUE;
        }

        // If an error other than EAGAIN happens, we have likely been disconnected
        // from the Wayland session; try to handle that the best we can.
        if (!flushDisplay())
        {
            wl_display_cancel_read(_plfw.wl.display);

            _PLFWwindow* window = _plfw.windowListHead;
            while (window)
            {
                _plfwInputWindowCloseRequest(window);
                window = window->next;
            }

            return;
        }

        double immediate = 0.0;

        if (event)
            timeout = &immediate;

        if (!_plfwPollPOSIX(fds, sizeof(fds) / sizeof(fds[0]), timeout))
        {
            wl_display_cancel_read(_plfw.wl.display);
            return;
        }

        if (fds[DISPLAY_FD].revents & POLLIN)
        {
            wl_display_read_events(_plfw.wl.display);
            if (wl_display_dispatch_pending(_plfw.wl.display) > 0)
                event = PLFW_TRUE;
        }
        else
            wl_display_cancel_read(_plfw.wl.display);

        if (fds[KEYREPEAT_FD].revents & POLLIN)
        {
            uint64_t repeats;

            if (read(_plfw.wl.keyRepeatTimerfd, &repeats, sizeof(repeats)) == 8)
            {
                if (_plfw.wl.keyboardFocus)
                {
                    for (uint64_t i = 0; i < repeats; i++)
                    {
                        _plfwInputKey(_plfw.wl.keyboardFocus,
                                      translateKey(_plfw.wl.keyRepeatScancode),
                                      _plfw.wl.keyRepeatScancode,
                                      PLFW_PRESS,
                                      _plfw.wl.xkb.modifiers);
                        inputText(_plfw.wl.keyboardFocus, _plfw.wl.keyRepeatScancode);
                    }

                    event = PLFW_TRUE;
                }

            }
        }

        if (fds[CURSOR_FD].revents & POLLIN)
        {
            uint64_t repeats;

            if (read(_plfw.wl.cursorTimerfd, &repeats, sizeof(repeats)) == 8)
                incrementCursorImage();
        }
    }
}

// Reads the specified data offer as the specified MIME type
//
static char* readDataOfferAsString(struct wl_data_offer* offer, const char* mimeType)
{
    int fds[2];

    if (pipe2(fds, O_CLOEXEC) == -1)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create pipe for data offer: %s",
                        strerror(errno));
        return NULL;
    }

    wl_data_offer_receive(offer, mimeType, fds[1]);
    flushDisplay();
    close(fds[1]);

    char* string = NULL;
    size_t size = 0;
    size_t length = 0;

    for (;;)
    {
        const size_t readSize = 4096;
        const size_t requiredSize = length + readSize + 1;
        if (requiredSize > size)
        {
            char* longer = _plfw_realloc(string, requiredSize);
            if (!longer)
            {
                _plfwInputError(PLFW_OUT_OF_MEMORY, NULL);
                _plfw_free(string);
                close(fds[0]);
                return NULL;
            }

            string = longer;
            size = requiredSize;
        }

        const ssize_t result = read(fds[0], string + length, readSize);
        if (result == 0)
            break;
        else if (result == -1)
        {
            if (errno == EINTR)
                continue;

            _plfwInputError(PLFW_PLATFORM_ERROR,
                            "Wayland: Failed to read from data offer pipe: %s",
                            strerror(errno));
            _plfw_free(string);
            close(fds[0]);
            return NULL;
        }

        length += result;
    }

    close(fds[0]);

    string[length] = '\0';
    return string;
}

static void processPointerEnterSurface(struct wl_surface* surface)
{
    _plfw.wl.pointerSurface = surface;

    _PLFWwindow* window = wl_surface_get_user_data(_plfw.wl.pointerSurface);
    if (window->wl.surface == _plfw.wl.pointerSurface)
    {
        _plfwSetCursorWayland(window, window->cursor);
        _plfwInputCursorEnter(window, PLFW_TRUE);
    }
}

static void processPointerLeaveSurface(struct wl_surface* surface)
{
    _plfw.wl.pointerSurface = NULL;

    _PLFWwindow* window = wl_surface_get_user_data(surface);
    if (window->wl.surface == surface)
        _plfwInputCursorEnter(window, PLFW_FALSE);
    else
    {
        if (window->wl.fallback.decorations)
            window->wl.fallback.cursorName = NULL;
    }
}

static void processPointerMotion(double xpos, double ypos)
{
    _PLFWwindow* window = wl_surface_get_user_data(_plfw.wl.pointerSurface);
    if (window->wl.surface == _plfw.wl.pointerSurface)
    {
        if (window->cursorMode != PLFW_CURSOR_DISABLED)
        {
            window->wl.cursorPosX = xpos;
            window->wl.cursorPosY = ypos;
            _plfwInputCursorPos(window, window->wl.cursorPosX, window->wl.cursorPosY);
        }
    }
    else
    {
        if (window->wl.fallback.decorations)
            updateFallbackDecorationCursor(window, xpos, ypos);
    }
}

static void processPointerButton(int button, int action)
{
    _PLFWwindow* window = wl_surface_get_user_data(_plfw.wl.pointerSurface);
    if (window->wl.surface == _plfw.wl.pointerSurface)
        _plfwInputMouseClick(window, button, action, _plfw.wl.xkb.modifiers);
    else
    {
        if (window->wl.fallback.decorations)
            handleFallbackDecorationButton(window, button, action);
    }
}

static void processPointerScroll(double xoffset, double yoffset)
{
    _PLFWwindow* window = wl_surface_get_user_data(_plfw.wl.pointerSurface);
    if (window->wl.surface == _plfw.wl.pointerSurface)
        _plfwInputScroll(window, xoffset, yoffset);
}

static void pointerHandleEnter(void* userData,
                               struct wl_pointer* pointer,
                               uint32_t serial,
                               struct wl_surface* surface,
                               wl_fixed_t sx,
                               wl_fixed_t sy)
{
    // Happens in the case we just destroyed the surface.
    if (!surface)
        return;

    if (wl_proxy_get_tag((struct wl_proxy*) surface) != &_plfw.wl.tag)
        return;

    _plfw.wl.serial = serial;
    _plfw.wl.pointerEnterSerial = serial;

    const double xpos = wl_fixed_to_double(sx);
    const double ypos = wl_fixed_to_double(sy);

    if (wl_pointer_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION)
    {
        _plfw.wl.pending.events |= (PLFW_PENDING_SURFACE | PLFW_PENDING_MOTION);
        _plfw.wl.pending.pointerSurface = surface;
        _plfw.wl.pending.pointerX = xpos;
        _plfw.wl.pending.pointerY = ypos;
    }
    else
    {
        processPointerEnterSurface(surface);
        processPointerMotion(xpos, ypos);
    }
}

static void pointerHandleLeave(void* userData,
                               struct wl_pointer* pointer,
                               uint32_t serial,
                               struct wl_surface* surface)
{
    if (!surface)
        return;

    if (wl_proxy_get_tag((struct wl_proxy*) surface) != &_plfw.wl.tag)
        return;

    _plfw.wl.serial = serial;

    if (wl_pointer_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION)
    {
        _plfw.wl.pending.events |= PLFW_PENDING_SURFACE;
        _plfw.wl.pending.pointerSurface = NULL;
    }
    else
        processPointerLeaveSurface(surface);
}

static void pointerHandleMotion(void* userData,
                                struct wl_pointer* pointer,
                                uint32_t time,
                                wl_fixed_t sx,
                                wl_fixed_t sy)
{
    if (!_plfw.wl.pointerSurface)
        return;

    const double xpos = wl_fixed_to_double(sx);
    const double ypos = wl_fixed_to_double(sy);

    if (wl_pointer_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION)
    {
        _plfw.wl.pending.events |= PLFW_PENDING_MOTION;
        _plfw.wl.pending.pointerX = xpos;
        _plfw.wl.pending.pointerY = ypos;
    }
    else
        processPointerMotion(xpos, ypos);
}

static void pointerHandleButton(void* userData,
                                struct wl_pointer* pointer,
                                uint32_t serial,
                                uint32_t time,
                                uint32_t buttonID,
                                uint32_t state)
{
    if (!_plfw.wl.pointerSurface)
        return;

    _plfw.wl.serial = serial;

    const int button = buttonID - BTN_LEFT;
    const int action = (state == WL_POINTER_BUTTON_STATE_PRESSED);

    _PLFWwindow* window = wl_surface_get_user_data(_plfw.wl.pointerSurface);
    if (window->wl.fallback.decorations)
    {
        if (action == PLFW_PRESS)
            window->wl.fallback.buttonPressSerial = serial;
    }

    if (wl_pointer_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION)
    {
        _plfw.wl.pending.events |= PLFW_PENDING_BUTTON;
        _plfw.wl.pending.button = button;
        _plfw.wl.pending.action = action;
    }
    else
        processPointerButton(button, action);
}

static void pointerHandleAxis(void* userData,
                              struct wl_pointer* pointer,
                              uint32_t time,
                              uint32_t axis,
                              wl_fixed_t value)
{
    if (!_plfw.wl.pointerSurface)
        return;

    if (wl_pointer_get_version(pointer) >= WL_POINTER_FRAME_SINCE_VERSION)
    {
        _plfw.wl.pending.events |= PLFW_PENDING_SCROLL;
        if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
            _plfw.wl.pending.scrollX = -wl_fixed_to_double(value) / 10.0;
        else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
            _plfw.wl.pending.scrollY = -wl_fixed_to_double(value) / 10.0;
    }
    else
    {
        // NOTE: 10 units of motion per mouse wheel step seems to be a common ratio
        if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
            processPointerScroll(-wl_fixed_to_double(value) / 10.0, 0.0);
        else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
            processPointerScroll(0.0, -wl_fixed_to_double(value) / 10.0);
    }
}

static void pointerHandleFrame(void* userData, struct wl_pointer* pointer)
{
    if (_plfw.wl.pending.events & PLFW_PENDING_SURFACE)
    {
        if (_plfw.wl.pointerSurface)
            processPointerLeaveSurface(_plfw.wl.pointerSurface);

        if (_plfw.wl.pending.pointerSurface)
            processPointerEnterSurface(_plfw.wl.pending.pointerSurface);
    }

    if (!_plfw.wl.pointerSurface)
        return;

    if (_plfw.wl.pending.events & PLFW_PENDING_MOTION)
        processPointerMotion(_plfw.wl.pending.pointerX, _plfw.wl.pending.pointerY);

    if (_plfw.wl.pending.events & PLFW_PENDING_BUTTON)
        processPointerButton(_plfw.wl.pending.button, _plfw.wl.pending.action);

    if (_plfw.wl.pending.events & PLFW_PENDING_DISCRETE)
        processPointerScroll(_plfw.wl.pending.discreteX, _plfw.wl.pending.discreteY);
    else if (_plfw.wl.pending.events & PLFW_PENDING_SCROLL)
        processPointerScroll(_plfw.wl.pending.scrollX, _plfw.wl.pending.scrollY);

    memset(&_plfw.wl.pending, 0, sizeof(_plfw.wl.pending));
}

static void pointerHandleAxisSource(void* userData,
                                    struct wl_pointer* pointer,
                                    uint32_t axisSource)
{
}

static void pointerHandleAxisStop(void* userData,
                                  struct wl_pointer* pointer,
                                  uint32_t time,
                                  uint32_t axis)
{
}

static void pointerHandleAxisDiscrete(void* userData,
                                      struct wl_pointer* pointer,
                                      uint32_t axis,
                                      int32_t discrete)
{
}

static void pointerHandleAxisValue120(void* data,
                                      struct wl_pointer* pointer,
                                      uint32_t axis,
                                      int32_t value120)
{
    if (!_plfw.wl.pointerSurface)
        return;

    _plfw.wl.pending.events |= PLFW_PENDING_DISCRETE;
    if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
        _plfw.wl.pending.discreteX = -(value120 / 120.0);
    else if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
        _plfw.wl.pending.discreteY = -(value120 / 120.0);
}

static const struct wl_pointer_listener pointerListener =
{
    pointerHandleEnter,
    pointerHandleLeave,
    pointerHandleMotion,
    pointerHandleButton,
    pointerHandleAxis,
    pointerHandleFrame,
    pointerHandleAxisSource,
    pointerHandleAxisStop,
    pointerHandleAxisDiscrete,
    pointerHandleAxisValue120
};

static void keyboardHandleKeymap(void* userData,
                                 struct wl_keyboard* keyboard,
                                 uint32_t format,
                                 int fd,
                                 uint32_t size)
{
    struct xkb_keymap* keymap;
    struct xkb_state* state;
    struct xkb_compose_table* composeTable;
    struct xkb_compose_state* composeState;

    char* mapStr;
    const char* locale;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
    {
        close(fd);
        return;
    }

    mapStr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapStr == MAP_FAILED)
    {
        close(fd);
        return;
    }

    keymap = xkb_keymap_new_from_string(_plfw.wl.xkb.context,
                                        mapStr,
                                        XKB_KEYMAP_FORMAT_TEXT_V1,
                                        XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(mapStr, size);
    close(fd);

    if (!keymap)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to compile keymap");
        return;
    }

    state = xkb_state_new(keymap);
    if (!state)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create XKB state");
        xkb_keymap_unref(keymap);
        return;
    }

    // Look up the preferred locale, falling back to "C" as default.
    locale = getenv("LC_ALL");
    if (!locale)
        locale = getenv("LC_CTYPE");
    if (!locale)
        locale = getenv("LANG");
    if (!locale)
        locale = "C";

    composeTable =
        xkb_compose_table_new_from_locale(_plfw.wl.xkb.context, locale,
                                          XKB_COMPOSE_COMPILE_NO_FLAGS);
    if (composeTable)
    {
        composeState =
            xkb_compose_state_new(composeTable, XKB_COMPOSE_STATE_NO_FLAGS);
        xkb_compose_table_unref(composeTable);
        if (composeState)
            _plfw.wl.xkb.composeState = composeState;
        else
            _plfwInputError(PLFW_PLATFORM_ERROR,
                            "Wayland: Failed to create XKB compose state");
    }
    else
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create XKB compose table");
    }

    xkb_keymap_unref(_plfw.wl.xkb.keymap);
    xkb_state_unref(_plfw.wl.xkb.state);
    _plfw.wl.xkb.keymap = keymap;
    _plfw.wl.xkb.state = state;

    _plfw.wl.xkb.controlIndex  = xkb_keymap_mod_get_index(_plfw.wl.xkb.keymap, "Control");
    _plfw.wl.xkb.altIndex      = xkb_keymap_mod_get_index(_plfw.wl.xkb.keymap, "Mod1");
    _plfw.wl.xkb.shiftIndex    = xkb_keymap_mod_get_index(_plfw.wl.xkb.keymap, "Shift");
    _plfw.wl.xkb.superIndex    = xkb_keymap_mod_get_index(_plfw.wl.xkb.keymap, "Mod4");
    _plfw.wl.xkb.capsLockIndex = xkb_keymap_mod_get_index(_plfw.wl.xkb.keymap, "Lock");
    _plfw.wl.xkb.numLockIndex  = xkb_keymap_mod_get_index(_plfw.wl.xkb.keymap, "Mod2");
}

static void keyboardHandleEnter(void* userData,
                                struct wl_keyboard* keyboard,
                                uint32_t serial,
                                struct wl_surface* surface,
                                struct wl_array* keys)
{
    // Happens in the case we just destroyed the surface.
    if (!surface)
        return;

    if (wl_proxy_get_tag((struct wl_proxy*) surface) != &_plfw.wl.tag)
        return;

    _PLFWwindow* window = wl_surface_get_user_data(surface);
    if (surface != window->wl.surface)
        return;

    _plfw.wl.serial = serial;
    _plfw.wl.keyboardFocus = window;
    _plfwInputWindowFocus(window, PLFW_TRUE);
}

static void keyboardHandleLeave(void* userData,
                                struct wl_keyboard* keyboard,
                                uint32_t serial,
                                struct wl_surface* surface)
{
    _PLFWwindow* window = _plfw.wl.keyboardFocus;

    if (!window)
        return;

    struct itimerspec timer = {0};
    timerfd_settime(_plfw.wl.keyRepeatTimerfd, 0, &timer, NULL);

    _plfw.wl.serial = serial;
    _plfw.wl.keyboardFocus = NULL;
    _plfwInputWindowFocus(window, PLFW_FALSE);
}

static void keyboardHandleKey(void* userData,
                              struct wl_keyboard* keyboard,
                              uint32_t serial,
                              uint32_t time,
                              uint32_t scancode,
                              uint32_t state)
{
    _PLFWwindow* window = _plfw.wl.keyboardFocus;
    if (!window)
        return;

    const int key = translateKey(scancode);
    const int action =
        state == WL_KEYBOARD_KEY_STATE_PRESSED ? PLFW_PRESS : PLFW_RELEASE;

    _plfw.wl.serial = serial;

    struct itimerspec timer = {0};

    if (action == PLFW_PRESS)
    {
        const xkb_keycode_t keycode = scancode + 8;

        if (xkb_keymap_key_repeats(_plfw.wl.xkb.keymap, keycode) &&
            _plfw.wl.keyRepeatRate > 0)
        {
            _plfw.wl.keyRepeatScancode = scancode;
            if (_plfw.wl.keyRepeatRate > 1)
                timer.it_interval.tv_nsec = 1000000000 / _plfw.wl.keyRepeatRate;
            else
                timer.it_interval.tv_sec = 1;

            timer.it_value.tv_sec = _plfw.wl.keyRepeatDelay / 1000;
            timer.it_value.tv_nsec = (_plfw.wl.keyRepeatDelay % 1000) * 1000000;
            timerfd_settime(_plfw.wl.keyRepeatTimerfd, 0, &timer, NULL);
        }
    }
    else if (scancode == _plfw.wl.keyRepeatScancode)
    {
        timerfd_settime(_plfw.wl.keyRepeatTimerfd, 0, &timer, NULL);
    }

    _plfwInputKey(window, key, scancode, action, _plfw.wl.xkb.modifiers);

    if (action == PLFW_PRESS)
        inputText(window, scancode);
}

static void keyboardHandleModifiers(void* userData,
                                    struct wl_keyboard* keyboard,
                                    uint32_t serial,
                                    uint32_t modsDepressed,
                                    uint32_t modsLatched,
                                    uint32_t modsLocked,
                                    uint32_t group)
{
    _plfw.wl.serial = serial;

    if (!_plfw.wl.xkb.keymap)
        return;

    xkb_state_update_mask(_plfw.wl.xkb.state,
                          modsDepressed,
                          modsLatched,
                          modsLocked,
                          0,
                          0,
                          group);

    _plfw.wl.xkb.modifiers = 0;

    struct
    {
        xkb_mod_index_t index;
        unsigned int bit;
    } modifiers[] =
    {
        { _plfw.wl.xkb.controlIndex,  PLFW_MOD_CONTROL },
        { _plfw.wl.xkb.altIndex,      PLFW_MOD_ALT },
        { _plfw.wl.xkb.shiftIndex,    PLFW_MOD_SHIFT },
        { _plfw.wl.xkb.superIndex,    PLFW_MOD_SUPER },
        { _plfw.wl.xkb.capsLockIndex, PLFW_MOD_CAPS_LOCK },
        { _plfw.wl.xkb.numLockIndex,  PLFW_MOD_NUM_LOCK }
    };

    for (size_t i = 0; i < sizeof(modifiers) / sizeof(modifiers[0]); i++)
    {
        if (xkb_state_mod_index_is_active(_plfw.wl.xkb.state,
                                          modifiers[i].index,
                                          XKB_STATE_MODS_EFFECTIVE) == 1)
        {
            _plfw.wl.xkb.modifiers |= modifiers[i].bit;
        }
    }
}

static void keyboardHandleRepeatInfo(void* userData,
                                     struct wl_keyboard* keyboard,
                                     int32_t rate,
                                     int32_t delay)
{
    if (keyboard != _plfw.wl.keyboard)
        return;

    _plfw.wl.keyRepeatRate = rate;
    _plfw.wl.keyRepeatDelay = delay;
}

static const struct wl_keyboard_listener keyboardListener =
{
    keyboardHandleKeymap,
    keyboardHandleEnter,
    keyboardHandleLeave,
    keyboardHandleKey,
    keyboardHandleModifiers,
    keyboardHandleRepeatInfo,
};

static void seatHandleCapabilities(void* userData,
                                   struct wl_seat* seat,
                                   enum wl_seat_capability caps)
{
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !_plfw.wl.pointer)
    {
        _plfw.wl.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(_plfw.wl.pointer, &pointerListener, NULL);
    }
    else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && _plfw.wl.pointer)
    {
        if (wl_pointer_get_version(_plfw.wl.pointer) >= WL_POINTER_RELEASE_SINCE_VERSION)
            wl_pointer_release(_plfw.wl.pointer);
        else
            wl_pointer_destroy(_plfw.wl.pointer);

        _plfw.wl.pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !_plfw.wl.keyboard)
    {
        _plfw.wl.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(_plfw.wl.keyboard, &keyboardListener, NULL);

        if (wl_keyboard_get_version(_plfw.wl.keyboard) <
            WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
        {
            _plfw.wl.keyRepeatRate = 4;
            _plfw.wl.keyRepeatDelay = 500;
        }
    }
    else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && _plfw.wl.keyboard)
    {
        if (wl_keyboard_get_version(_plfw.wl.keyboard) >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
            wl_keyboard_release(_plfw.wl.keyboard);
        else
            wl_keyboard_destroy(_plfw.wl.keyboard);

        _plfw.wl.keyboard = NULL;
    }
}

static void seatHandleName(void* userData,
                           struct wl_seat* seat,
                           const char* name)
{
}

static const struct wl_seat_listener seatListener =
{
    seatHandleCapabilities,
    seatHandleName,
};

static void dataOfferHandleOffer(void* userData,
                                 struct wl_data_offer* offer,
                                 const char* mimeType)
{
    for (unsigned int i = 0; i < _plfw.wl.offerCount; i++)
    {
        if (_plfw.wl.offers[i].offer == offer)
        {
            if (strcmp(mimeType, "text/plain;charset=utf-8") == 0)
                _plfw.wl.offers[i].text_plain_utf8 = PLFW_TRUE;
            else if (strcmp(mimeType, "text/uri-list") == 0)
                _plfw.wl.offers[i].text_uri_list = PLFW_TRUE;

            break;
        }
    }
}

static const struct wl_data_offer_listener dataOfferListener =
{
    dataOfferHandleOffer
};

static void dataDeviceHandleDataOffer(void* userData,
                                      struct wl_data_device* device,
                                      struct wl_data_offer* offer)
{
    _PLFWofferWayland* offers =
        _plfw_realloc(_plfw.wl.offers,
                      sizeof(_PLFWofferWayland) * (_plfw.wl.offerCount + 1));
    if (!offers)
    {
        _plfwInputError(PLFW_OUT_OF_MEMORY, NULL);
        return;
    }

    _plfw.wl.offers = offers;
    _plfw.wl.offerCount++;

    _plfw.wl.offers[_plfw.wl.offerCount - 1] = (_PLFWofferWayland) { offer };
    wl_data_offer_add_listener(offer, &dataOfferListener, NULL);
}

static void dataDeviceHandleEnter(void* userData,
                                  struct wl_data_device* device,
                                  uint32_t serial,
                                  struct wl_surface* surface,
                                  wl_fixed_t x,
                                  wl_fixed_t y,
                                  struct wl_data_offer* offer)
{
    if (_plfw.wl.dragOffer)
    {
        wl_data_offer_destroy(_plfw.wl.dragOffer);
        _plfw.wl.dragOffer = NULL;
        _plfw.wl.dragFocus = NULL;
    }

    unsigned int i;

    for (i = 0; i < _plfw.wl.offerCount; i++)
    {
        if (_plfw.wl.offers[i].offer == offer)
            break;
    }

    if (i == _plfw.wl.offerCount)
        return;

    if (surface && wl_proxy_get_tag((struct wl_proxy*) surface) == &_plfw.wl.tag)
    {
        _PLFWwindow* window = wl_surface_get_user_data(surface);
        if (window->wl.surface == surface)
        {
            if (_plfw.wl.offers[i].text_uri_list)
            {
                _plfw.wl.dragOffer = offer;
                _plfw.wl.dragFocus = window;
                _plfw.wl.dragSerial = serial;

                wl_data_offer_accept(offer, serial, "text/uri-list");
            }
        }
    }

    if (!_plfw.wl.dragOffer)
    {
        wl_data_offer_accept(offer, serial, NULL);
        wl_data_offer_destroy(offer);
    }

    _plfw.wl.offers[i] = _plfw.wl.offers[_plfw.wl.offerCount - 1];
    _plfw.wl.offerCount--;
}

static void dataDeviceHandleLeave(void* userData,
                                  struct wl_data_device* device)
{
    if (_plfw.wl.dragOffer)
    {
        wl_data_offer_destroy(_plfw.wl.dragOffer);
        _plfw.wl.dragOffer = NULL;
        _plfw.wl.dragFocus = NULL;
    }
}

static void dataDeviceHandleMotion(void* userData,
                                   struct wl_data_device* device,
                                   uint32_t time,
                                   wl_fixed_t x,
                                   wl_fixed_t y)
{
}

static void dataDeviceHandleDrop(void* userData,
                                 struct wl_data_device* device)
{
    if (!_plfw.wl.dragOffer)
        return;

    char* string = readDataOfferAsString(_plfw.wl.dragOffer, "text/uri-list");
    if (string)
    {
        int count;
        char** paths = _plfwParseUriList(string, &count);
        if (paths)
        {
            _plfwInputDrop(_plfw.wl.dragFocus, count, (const char**) paths);

            for (int i = 0; i < count; i++)
                _plfw_free(paths[i]);

            _plfw_free(paths);
        }

        _plfw_free(string);
    }
}

static void dataDeviceHandleSelection(void* userData,
                                      struct wl_data_device* device,
                                      struct wl_data_offer* offer)
{
    if (_plfw.wl.selectionOffer)
    {
        wl_data_offer_destroy(_plfw.wl.selectionOffer);
        _plfw.wl.selectionOffer = NULL;
    }

    for (unsigned int i = 0; i < _plfw.wl.offerCount; i++)
    {
        if (_plfw.wl.offers[i].offer == offer)
        {
            if (_plfw.wl.offers[i].text_plain_utf8)
                _plfw.wl.selectionOffer = offer;
            else
                wl_data_offer_destroy(offer);

            _plfw.wl.offers[i] = _plfw.wl.offers[_plfw.wl.offerCount - 1];
            _plfw.wl.offerCount--;
            break;
        }
    }
}

const struct wl_data_device_listener dataDeviceListener =
{
    dataDeviceHandleDataOffer,
    dataDeviceHandleEnter,
    dataDeviceHandleLeave,
    dataDeviceHandleMotion,
    dataDeviceHandleDrop,
    dataDeviceHandleSelection,
};

static void xdgActivationHandleDone(void* userData,
                                    struct xdg_activation_token_v1* activationToken,
                                    const char* token)
{
    _PLFWwindow* window = userData;

    if (activationToken != window->wl.activationToken)
        return;

    xdg_activation_v1_activate(_plfw.wl.activationManager, token, window->wl.surface);
    xdg_activation_token_v1_destroy(window->wl.activationToken);
    window->wl.activationToken = NULL;
}

static const struct xdg_activation_token_v1_listener xdgActivationListener =
{
    xdgActivationHandleDone
};

void _plfwAddSeatListenerWayland(struct wl_seat* seat)
{
    wl_seat_add_listener(seat, &seatListener, NULL);
}

void _plfwAddDataDeviceListenerWayland(struct wl_data_device* device)
{
    wl_data_device_add_listener(device, &dataDeviceListener, NULL);
}

//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwCreateWindowWayland(_PLFWwindow* window,
                                  const _PLFWwndconfig* wndconfig,
                                  const _PLFWfbconfig* fbconfig)
{
    if (!createNativeSurface(window, wndconfig, fbconfig))
        return PLFW_FALSE;

    // No graphics context is created: the window owns a bare wl_surface and
    // leaves buffer allocation and presentation to the application.

    if (wndconfig->mousePassthrough)
        _plfwSetWindowMousePassthroughWayland(window, PLFW_TRUE);

    if (window->monitor || wndconfig->visible)
    {
        if (!createShellObjects(window))
            return PLFW_FALSE;
    }

    return PLFW_TRUE;
}

void _plfwDestroyWindowWayland(_PLFWwindow* window)
{
    if (window->wl.surface == _plfw.wl.pointerSurface)
        _plfw.wl.pointerSurface = NULL;

    if (window == _plfw.wl.keyboardFocus)
    {
        struct itimerspec timer = {0};
        timerfd_settime(_plfw.wl.keyRepeatTimerfd, 0, &timer, NULL);

        _plfw.wl.keyboardFocus = NULL;
    }

    if (window->wl.fractionalScale)
        wp_fractional_scale_v1_destroy(window->wl.fractionalScale);

    if (window->wl.scalingViewport)
        wp_viewport_destroy(window->wl.scalingViewport);

    if (window->wl.activationToken)
        xdg_activation_token_v1_destroy(window->wl.activationToken);

    if (window->wl.idleInhibitor)
        zwp_idle_inhibitor_v1_destroy(window->wl.idleInhibitor);

    if (window->wl.relativePointer)
        zwp_relative_pointer_v1_destroy(window->wl.relativePointer);

    if (window->wl.lockedPointer)
        zwp_locked_pointer_v1_destroy(window->wl.lockedPointer);

    if (window->wl.confinedPointer)
        zwp_confined_pointer_v1_destroy(window->wl.confinedPointer);

    if (window->wl.blankBuffer)
        wl_buffer_destroy(window->wl.blankBuffer);

    destroyShellObjects(window);

    if (window->wl.fallback.buffer)
        wl_buffer_destroy(window->wl.fallback.buffer);

    if (window->wl.surface)
        wl_surface_destroy(window->wl.surface);

    _plfw_free(window->wl.appId);
    _plfw_free(window->wl.outputScales);
}

void _plfwSetWindowTitleWayland(_PLFWwindow* window, const char* title)
{
    if (window->wl.libdecor.frame)
        libdecor_frame_set_title(window->wl.libdecor.frame, title);
    else if (window->wl.xdg.toplevel)
        xdg_toplevel_set_title(window->wl.xdg.toplevel, title);
}

void _plfwSetWindowIconWayland(_PLFWwindow* window,
                               int count, const PLFWimage* images)
{
    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: The platform does not support setting the window icon");
}

void _plfwGetWindowPosWayland(_PLFWwindow* window, int* xpos, int* ypos)
{
    // A Wayland client is not aware of its position, so just warn and leave it
    // as (0, 0)

    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: The platform does not provide the window position");
}

void _plfwSetWindowPosWayland(_PLFWwindow* window, int xpos, int ypos)
{
    // A Wayland client can not set its position, so just warn

    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: The platform does not support setting the window position");
}

void _plfwGetWindowSizeWayland(_PLFWwindow* window, int* width, int* height)
{
    if (width)
        *width = window->wl.width;
    if (height)
        *height = window->wl.height;
}

void _plfwSetWindowSizeWayland(_PLFWwindow* window, int width, int height)
{
    if (window->monitor)
    {
        // Video mode setting is not available on Wayland
    }
    else
    {
        if (!resizeWindow(window, width, height))
            return;

        if (window->wl.libdecor.frame)
        {
            struct libdecor_state* frameState =
                libdecor_state_new(window->wl.width, window->wl.height);
            libdecor_frame_commit(window->wl.libdecor.frame, frameState, NULL);
            libdecor_state_free(frameState);
        }

        if (window->wl.visible)
            _plfwInputWindowDamage(window);
    }
}

void _plfwSetWindowSizeLimitsWayland(_PLFWwindow* window,
                                     int minwidth, int minheight,
                                     int maxwidth, int maxheight)
{
    if (window->wl.libdecor.frame)
    {
        if (minwidth == PLFW_DONT_CARE || minheight == PLFW_DONT_CARE)
            minwidth = minheight = 0;

        if (maxwidth == PLFW_DONT_CARE || maxheight == PLFW_DONT_CARE)
            maxwidth = maxheight = 0;

        libdecor_frame_set_min_content_size(window->wl.libdecor.frame,
                                            minwidth, minheight);
        libdecor_frame_set_max_content_size(window->wl.libdecor.frame,
                                            maxwidth, maxheight);
    }
    else if (window->wl.xdg.toplevel)
        updateXdgSizeLimits(window);
}

void _plfwSetWindowAspectRatioWayland(_PLFWwindow* window, int numer, int denom)
{
    if (window->wl.maximized || window->wl.fullscreen)
        return;

    int width = window->wl.width, height = window->wl.height;

    if (numer != PLFW_DONT_CARE && denom != PLFW_DONT_CARE)
    {
        const float aspectRatio = (float) width / (float) height;
        const float targetRatio = (float) numer / (float) denom;
        if (aspectRatio < targetRatio)
            height /= targetRatio;
        else if (aspectRatio > targetRatio)
            width *= targetRatio;
    }

    if (resizeWindow(window, width, height))
    {
        if (window->wl.libdecor.frame)
        {
            struct libdecor_state* frameState =
                libdecor_state_new(window->wl.width, window->wl.height);
            libdecor_frame_commit(window->wl.libdecor.frame, frameState, NULL);
            libdecor_state_free(frameState);
        }

        _plfwInputWindowSize(window, window->wl.width, window->wl.height);

        if (window->wl.visible)
            _plfwInputWindowDamage(window);
    }
}

void _plfwGetFramebufferSizeWayland(_PLFWwindow* window, int* width, int* height)
{
    if (width)
        *width = window->wl.fbWidth;
    if (height)
        *height = window->wl.fbHeight;
}

void _plfwGetWindowFrameSizeWayland(_PLFWwindow* window,
                                    int* left, int* top,
                                    int* right, int* bottom)
{
    if (window->wl.fallback.decorations)
    {
        if (top)
            *top = PLFW_CAPTION_HEIGHT;
        if (left)
            *left = PLFW_BORDER_SIZE;
        if (right)
            *right = PLFW_BORDER_SIZE;
        if (bottom)
            *bottom = PLFW_BORDER_SIZE;
    }
}

void _plfwGetWindowContentScaleWayland(_PLFWwindow* window,
                                       float* xscale, float* yscale)
{
    if (window->wl.fractionalScale)
    {
        if (xscale)
            *xscale = (float) window->wl.scalingNumerator / 120.f;
        if (yscale)
            *yscale = (float) window->wl.scalingNumerator / 120.f;
    }
    else
    {
        if (xscale)
            *xscale = (float) window->wl.bufferScale;
        if (yscale)
            *yscale = (float) window->wl.bufferScale;
    }
}

void _plfwIconifyWindowWayland(_PLFWwindow* window)
{
    if (window->wl.libdecor.frame)
        libdecor_frame_set_minimized(window->wl.libdecor.frame);
    else if (window->wl.xdg.toplevel)
        xdg_toplevel_set_minimized(window->wl.xdg.toplevel);
}

void _plfwRestoreWindowWayland(_PLFWwindow* window)
{
    if (window->monitor)
    {
        // There is no way to unset minimized, or even to know if we are
        // minimized, so there is nothing to do in this case.
    }
    else
    {
        // We assume we are not minimized and act only on maximization

        if (window->wl.maximized)
        {
            if (window->wl.libdecor.frame)
                libdecor_frame_unset_maximized(window->wl.libdecor.frame);
            else if (window->wl.xdg.toplevel)
                xdg_toplevel_unset_maximized(window->wl.xdg.toplevel);
            else
                window->wl.maximized = PLFW_FALSE;
        }
    }
}

void _plfwMaximizeWindowWayland(_PLFWwindow* window)
{
    if (window->wl.libdecor.frame)
        libdecor_frame_set_maximized(window->wl.libdecor.frame);
    else if (window->wl.xdg.toplevel)
        xdg_toplevel_set_maximized(window->wl.xdg.toplevel);
    else
        window->wl.maximized = PLFW_TRUE;
}

void _plfwShowWindowWayland(_PLFWwindow* window)
{
    if (!window->wl.libdecor.frame && !window->wl.xdg.toplevel)
    {
        // NOTE: The XDG surface and role are created here so command-line applications
        //       with off-screen windows do not appear in for example the Unity dock
        createShellObjects(window);
    }
}

void _plfwHideWindowWayland(_PLFWwindow* window)
{
    if (window->wl.visible)
    {
        window->wl.visible = PLFW_FALSE;
        destroyShellObjects(window);

        wl_surface_attach(window->wl.surface, NULL, 0, 0);
        wl_surface_commit(window->wl.surface);

        flushDisplay();
    }
}

void _plfwRequestWindowAttentionWayland(_PLFWwindow* window)
{
    if (!_plfw.wl.activationManager)
        return;

    // We're about to overwrite this with a new request
    if (window->wl.activationToken)
        xdg_activation_token_v1_destroy(window->wl.activationToken);

    window->wl.activationToken =
        xdg_activation_v1_get_activation_token(_plfw.wl.activationManager);
    xdg_activation_token_v1_add_listener(window->wl.activationToken,
                                         &xdgActivationListener,
                                         window);

    xdg_activation_token_v1_commit(window->wl.activationToken);
}

void _plfwFocusWindowWayland(_PLFWwindow* window)
{
    if (!_plfw.wl.activationManager)
        return;

    if (window->wl.activationToken)
        xdg_activation_token_v1_destroy(window->wl.activationToken);

    window->wl.activationToken =
        xdg_activation_v1_get_activation_token(_plfw.wl.activationManager);
    xdg_activation_token_v1_add_listener(window->wl.activationToken,
                                         &xdgActivationListener,
                                         window);

    xdg_activation_token_v1_set_serial(window->wl.activationToken,
                                       _plfw.wl.serial,
                                       _plfw.wl.seat);

    _PLFWwindow* requester = _plfw.wl.keyboardFocus;
    if (requester)
    {
        xdg_activation_token_v1_set_surface(window->wl.activationToken,
                                            requester->wl.surface);

        if (requester->wl.appId)
        {
            xdg_activation_token_v1_set_app_id(window->wl.activationToken,
                                               requester->wl.appId);
        }
    }

    xdg_activation_token_v1_commit(window->wl.activationToken);
}

void _plfwSetWindowMonitorWayland(_PLFWwindow* window,
                                  _PLFWmonitor* monitor,
                                  int xpos, int ypos,
                                  int width, int height,
                                  int refreshRate)
{
    if (window->monitor == monitor)
    {
        if (!monitor)
            _plfwSetWindowSizeWayland(window, width, height);

        return;
    }

    if (window->monitor)
        releaseMonitor(window);

    _plfwInputWindowMonitor(window, monitor);

    if (window->monitor)
        acquireMonitor(window);
    else
        _plfwSetWindowSizeWayland(window, width, height);
}

PLFWbool _plfwWindowFocusedWayland(_PLFWwindow* window)
{
    return _plfw.wl.keyboardFocus == window;
}

PLFWbool _plfwWindowIconifiedWayland(_PLFWwindow* window)
{
    // xdg-shell doesn’t give any way to request whether a surface is
    // iconified.
    return PLFW_FALSE;
}

PLFWbool _plfwWindowVisibleWayland(_PLFWwindow* window)
{
    return window->wl.visible;
}

PLFWbool _plfwWindowMaximizedWayland(_PLFWwindow* window)
{
    return window->wl.maximized;
}

PLFWbool _plfwWindowHoveredWayland(_PLFWwindow* window)
{
    return window->wl.surface == _plfw.wl.pointerSurface;
}

void _plfwSetWindowResizableWayland(_PLFWwindow* window, PLFWbool enabled)
{
    if (window->wl.libdecor.frame)
    {
        if (enabled)
        {
            libdecor_frame_set_capabilities(window->wl.libdecor.frame,
                                            LIBDECOR_ACTION_RESIZE);
        }
        else
        {
            libdecor_frame_unset_capabilities(window->wl.libdecor.frame,
                                              LIBDECOR_ACTION_RESIZE);
        }
    }
    else if (window->wl.xdg.toplevel)
        updateXdgSizeLimits(window);
}

void _plfwSetWindowDecoratedWayland(_PLFWwindow* window, PLFWbool enabled)
{
    if (window->wl.libdecor.frame)
    {
        libdecor_frame_set_visibility(window->wl.libdecor.frame, enabled);
    }
    else if (window->wl.xdg.decoration)
    {
        uint32_t mode;

        if (enabled)
            mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
        else
            mode = ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;

        zxdg_toplevel_decoration_v1_set_mode(window->wl.xdg.decoration, mode);
    }
    else if (window->wl.xdg.toplevel)
    {
        if (enabled)
            createFallbackDecorations(window);
        else
            destroyFallbackDecorations(window);
    }
}

void _plfwSetWindowFloatingWayland(_PLFWwindow* window, PLFWbool enabled)
{
    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: Platform does not support making a window floating");
}

void _plfwSetWindowMousePassthroughWayland(_PLFWwindow* window, PLFWbool enabled)
{
    if (enabled)
    {
        struct wl_region* region = wl_compositor_create_region(_plfw.wl.compositor);
        wl_surface_set_input_region(window->wl.surface, region);
        wl_region_destroy(region);
    }
    else
        wl_surface_set_input_region(window->wl.surface, NULL);
}

float _plfwGetWindowOpacityWayland(_PLFWwindow* window)
{
    return 1.f;
}

void _plfwSetWindowOpacityWayland(_PLFWwindow* window, float opacity)
{
    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: The platform does not support setting the window opacity");
}

void _plfwSetRawMouseMotionWayland(_PLFWwindow* window, PLFWbool enabled)
{
    // This is handled in relativePointerHandleRelativeMotion
}

PLFWbool _plfwRawMouseMotionSupportedWayland(void)
{
    return PLFW_TRUE;
}

void _plfwPollEventsWayland(void)
{
    double timeout = 0.0;
    handleEvents(&timeout);
}

void _plfwWaitEventsWayland(void)
{
    handleEvents(NULL);
}

void _plfwWaitEventsTimeoutWayland(double timeout)
{
    handleEvents(&timeout);
}

void _plfwPostEmptyEventWayland(void)
{
    struct wl_callback* callback = wl_display_sync(_plfw.wl.display);
    wl_callback_add_listener(callback, &noopCallbackListener, NULL);

    flushDisplay();
}

void _plfwGetCursorPosWayland(_PLFWwindow* window, double* xpos, double* ypos)
{
    if (xpos)
        *xpos = window->wl.cursorPosX;
    if (ypos)
        *ypos = window->wl.cursorPosY;
}

void _plfwSetCursorPosWayland(_PLFWwindow* window, double x, double y)
{
    _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                    "Wayland: The platform does not support setting the cursor position");
}

void _plfwSetCursorModeWayland(_PLFWwindow* window, int mode)
{
    _plfwSetCursorWayland(window, window->cursor);
}

const char* _plfwGetScancodeNameWayland(int scancode)
{
    if (scancode < 0 || scancode > 255)
    {
        _plfwInputError(PLFW_INVALID_VALUE,
                        "Wayland: Invalid scancode %i",
                        scancode);
        return NULL;
    }

    const int key = _plfw.wl.keycodes[scancode];
    if (key == PLFW_KEY_UNKNOWN)
        return NULL;

    const xkb_keycode_t keycode = scancode + 8;
    const xkb_layout_index_t layout =
        xkb_state_key_get_layout(_plfw.wl.xkb.state, keycode);
    if (layout == XKB_LAYOUT_INVALID)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to retrieve layout for key name");
        return NULL;
    }

    const xkb_keysym_t* keysyms = NULL;
    xkb_keymap_key_get_syms_by_level(_plfw.wl.xkb.keymap,
                                     keycode,
                                     layout,
                                     0,
                                     &keysyms);
    if (keysyms == NULL)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to retrieve keysym for key name");
        return NULL;
    }

    // WORKAROUND: xkb_keysym_to_utf8() requires the third parameter (size of the output buffer)
    // to be at least 7 (6 bytes + a null terminator), because it was written when UTF-8
    // sequences could be up to 6 bytes long. The _plfw.wl.keynames buffers are only 5 bytes
    // long, because UTF-8 sequences are now limited to 4 bytes and no codepoints were ever assigned
    // that needed more than that. To work around this, we first copy to a temporary buffer.
    //
    // See: https://github.com/xkbcommon/libxkbcommon/issues/418
    char temp_buffer[7];
    const int bytes_written = xkb_keysym_to_utf8(keysyms[0], temp_buffer, sizeof(temp_buffer));
    if (bytes_written <= 0 || bytes_written > 5)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to encode keysym as UTF-8");
        return NULL;
    }
    memcpy(_plfw.wl.keynames[key], temp_buffer, bytes_written);

    return _plfw.wl.keynames[key];
}

int _plfwGetKeyScancodeWayland(int key)
{
    return _plfw.wl.scancodes[key];
}

PLFWbool _plfwCreateCursorWayland(_PLFWcursor* cursor,
                                  const PLFWimage* image,
                                  int xhot, int yhot)
{
    cursor->wl.buffer = createShmBuffer(image);
    if (!cursor->wl.buffer)
        return PLFW_FALSE;

    cursor->wl.width = image->width;
    cursor->wl.height = image->height;
    cursor->wl.xhot = xhot;
    cursor->wl.yhot = yhot;
    return PLFW_TRUE;
}

PLFWbool _plfwCreateStandardCursorWayland(_PLFWcursor* cursor, int shape)
{
    const char* name = NULL;

    // Try the XDG names first
    switch (shape)
    {
        case PLFW_ARROW_CURSOR:
            name = "default";
            break;
        case PLFW_IBEAM_CURSOR:
            name = "text";
            break;
        case PLFW_CROSSHAIR_CURSOR:
            name = "crosshair";
            break;
        case PLFW_POINTING_HAND_CURSOR:
            name = "pointer";
            break;
        case PLFW_RESIZE_EW_CURSOR:
            name = "ew-resize";
            break;
        case PLFW_RESIZE_NS_CURSOR:
            name = "ns-resize";
            break;
        case PLFW_RESIZE_NWSE_CURSOR:
            name = "nwse-resize";
            break;
        case PLFW_RESIZE_NESW_CURSOR:
            name = "nesw-resize";
            break;
        case PLFW_RESIZE_ALL_CURSOR:
            name = "all-scroll";
            break;
        case PLFW_NOT_ALLOWED_CURSOR:
            name = "not-allowed";
            break;
    }

    cursor->wl.cursor = wl_cursor_theme_get_cursor(_plfw.wl.cursorTheme, name);

    if (_plfw.wl.cursorThemeHiDPI)
    {
        cursor->wl.cursorHiDPI =
            wl_cursor_theme_get_cursor(_plfw.wl.cursorThemeHiDPI, name);
    }

    if (!cursor->wl.cursor)
    {
        // Fall back to the core X11 names
        switch (shape)
        {
            case PLFW_ARROW_CURSOR:
                name = "left_ptr";
                break;
            case PLFW_IBEAM_CURSOR:
                name = "xterm";
                break;
            case PLFW_CROSSHAIR_CURSOR:
                name = "crosshair";
                break;
            case PLFW_POINTING_HAND_CURSOR:
                name = "hand2";
                break;
            case PLFW_RESIZE_EW_CURSOR:
                name = "sb_h_double_arrow";
                break;
            case PLFW_RESIZE_NS_CURSOR:
                name = "sb_v_double_arrow";
                break;
            case PLFW_RESIZE_ALL_CURSOR:
                name = "fleur";
                break;
            default:
                _plfwInputError(PLFW_CURSOR_UNAVAILABLE,
                                "Wayland: Standard cursor shape unavailable");
                return PLFW_FALSE;
        }

        cursor->wl.cursor = wl_cursor_theme_get_cursor(_plfw.wl.cursorTheme, name);
        if (!cursor->wl.cursor)
        {
            _plfwInputError(PLFW_CURSOR_UNAVAILABLE,
                            "Wayland: Failed to create standard cursor \"%s\"",
                            name);
            return PLFW_FALSE;
        }

        if (_plfw.wl.cursorThemeHiDPI)
        {
            if (!cursor->wl.cursorHiDPI)
            {
                cursor->wl.cursorHiDPI =
                    wl_cursor_theme_get_cursor(_plfw.wl.cursorThemeHiDPI, name);
            }
        }
    }

    return PLFW_TRUE;
}

void _plfwDestroyCursorWayland(_PLFWcursor* cursor)
{
    // If it's a standard cursor we don't need to do anything here
    if (cursor->wl.cursor)
        return;

    if (cursor->wl.buffer)
        wl_buffer_destroy(cursor->wl.buffer);
}

static void relativePointerHandleRelativeMotion(void* userData,
                                                struct zwp_relative_pointer_v1* pointer,
                                                uint32_t timeHi,
                                                uint32_t timeLo,
                                                wl_fixed_t dx,
                                                wl_fixed_t dy,
                                                wl_fixed_t dxUnaccel,
                                                wl_fixed_t dyUnaccel)
{
    _PLFWwindow* window = userData;
    double xpos = window->virtualCursorPosX;
    double ypos = window->virtualCursorPosY;

    if (window->cursorMode != PLFW_CURSOR_DISABLED)
        return;

    if (window->rawMouseMotion)
    {
        xpos += wl_fixed_to_double(dxUnaccel);
        ypos += wl_fixed_to_double(dyUnaccel);
    }
    else
    {
        xpos += wl_fixed_to_double(dx);
        ypos += wl_fixed_to_double(dy);
    }

    _plfwInputCursorPos(window, xpos, ypos);
}

static const struct zwp_relative_pointer_v1_listener relativePointerListener =
{
    relativePointerHandleRelativeMotion
};

static void lockedPointerHandleLocked(void* userData,
                                      struct zwp_locked_pointer_v1* lockedPointer)
{
}

static void lockedPointerHandleUnlocked(void* userData,
                                        struct zwp_locked_pointer_v1* lockedPointer)
{
}

static const struct zwp_locked_pointer_v1_listener lockedPointerListener =
{
    lockedPointerHandleLocked,
    lockedPointerHandleUnlocked
};

static void lockPointer(_PLFWwindow* window)
{
    if (!_plfw.wl.relativePointerManager)
    {
        _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                        "Wayland: The compositor does not support relative pointer motion");
        return;
    }

    if (!_plfw.wl.pointerConstraints)
    {
        _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                        "Wayland: The compositor does not support locking the pointer");
    }

    window->wl.relativePointer =
        zwp_relative_pointer_manager_v1_get_relative_pointer(
            _plfw.wl.relativePointerManager,
            _plfw.wl.pointer);
    zwp_relative_pointer_v1_add_listener(window->wl.relativePointer,
                                         &relativePointerListener,
                                         window);

    window->wl.lockedPointer =
        zwp_pointer_constraints_v1_lock_pointer(
            _plfw.wl.pointerConstraints,
            window->wl.surface,
            _plfw.wl.pointer,
            NULL,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    zwp_locked_pointer_v1_add_listener(window->wl.lockedPointer,
                                       &lockedPointerListener,
                                       window);
}

static void unlockPointer(_PLFWwindow* window)
{
    zwp_relative_pointer_v1_destroy(window->wl.relativePointer);
    window->wl.relativePointer = NULL;

    zwp_locked_pointer_v1_destroy(window->wl.lockedPointer);
    window->wl.lockedPointer = NULL;
}

static void confinedPointerHandleConfined(void* userData,
                                          struct zwp_confined_pointer_v1* confinedPointer)
{
}

static void confinedPointerHandleUnconfined(void* userData,
                                            struct zwp_confined_pointer_v1* confinedPointer)
{
}

static const struct zwp_confined_pointer_v1_listener confinedPointerListener =
{
    confinedPointerHandleConfined,
    confinedPointerHandleUnconfined
};

static void confinePointer(_PLFWwindow* window)
{
    if (!_plfw.wl.pointerConstraints)
    {
        _plfwInputError(PLFW_FEATURE_UNAVAILABLE,
                        "Wayland: The compositor does not support confining the pointer");
    }

    window->wl.confinedPointer =
        zwp_pointer_constraints_v1_confine_pointer(
            _plfw.wl.pointerConstraints,
            window->wl.surface,
            _plfw.wl.pointer,
            NULL,
            ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);

    zwp_confined_pointer_v1_add_listener(window->wl.confinedPointer,
                                         &confinedPointerListener,
                                         window);
}

static void unconfinePointer(_PLFWwindow* window)
{
    zwp_confined_pointer_v1_destroy(window->wl.confinedPointer);
    window->wl.confinedPointer = NULL;
}

void _plfwSetCursorWayland(_PLFWwindow* window, _PLFWcursor* cursor)
{
    if (!_plfw.wl.pointer)
        return;

    if (window->wl.surface != _plfw.wl.pointerSurface)
        return;

    // Update pointer lock to match cursor mode
    if (window->cursorMode == PLFW_CURSOR_DISABLED)
    {
        if (window->wl.confinedPointer)
            unconfinePointer(window);
        if (!window->wl.lockedPointer)
            lockPointer(window);
    }
    else if (window->cursorMode == PLFW_CURSOR_CAPTURED)
    {
        if (window->wl.lockedPointer)
            unlockPointer(window);
        if (!window->wl.confinedPointer)
            confinePointer(window);
    }
    else if (window->cursorMode == PLFW_CURSOR_NORMAL ||
             window->cursorMode == PLFW_CURSOR_HIDDEN)
    {
        if (window->wl.lockedPointer)
            unlockPointer(window);
        else if (window->wl.confinedPointer)
            unconfinePointer(window);
    }

    if (window->cursorMode == PLFW_CURSOR_NORMAL ||
        window->cursorMode == PLFW_CURSOR_CAPTURED)
    {
        if (cursor)
            setCursorImage(window, &cursor->wl);
        else
        {
            struct wl_cursor* defaultCursor =
                wl_cursor_theme_get_cursor(_plfw.wl.cursorTheme, "left_ptr");
            if (!defaultCursor)
            {
                _plfwInputError(PLFW_PLATFORM_ERROR,
                                "Wayland: Standard cursor not found");
                return;
            }

            struct wl_cursor* defaultCursorHiDPI = NULL;
            if (_plfw.wl.cursorThemeHiDPI)
            {
                defaultCursorHiDPI =
                    wl_cursor_theme_get_cursor(_plfw.wl.cursorThemeHiDPI, "left_ptr");
            }

            _PLFWcursorWayland cursorWayland =
            {
                defaultCursor,
                defaultCursorHiDPI,
                NULL,
                0, 0,
                0, 0,
                0
            };

            setCursorImage(window, &cursorWayland);
        }
    }
    else if (window->cursorMode == PLFW_CURSOR_HIDDEN ||
             window->cursorMode == PLFW_CURSOR_DISABLED)
    {
        wl_pointer_set_cursor(_plfw.wl.pointer, _plfw.wl.pointerEnterSerial, NULL, 0, 0);
    }
}

static void dataSourceHandleTarget(void* userData,
                                   struct wl_data_source* source,
                                   const char* mimeType)
{
    if (_plfw.wl.selectionSource != source)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Unknown clipboard data source");
        return;
    }
}

static void dataSourceHandleSend(void* userData,
                                 struct wl_data_source* source,
                                 const char* mimeType,
                                 int fd)
{
    // Ignore it if this is an outdated or invalid request
    if (_plfw.wl.selectionSource != source ||
        strcmp(mimeType, "text/plain;charset=utf-8") != 0)
    {
        close(fd);
        return;
    }

    char* string = _plfw.wl.clipboardString;
    size_t length = strlen(string);

    while (length > 0)
    {
        const ssize_t result = write(fd, string, length);
        if (result == -1)
        {
            if (errno == EINTR)
                continue;

            _plfwInputError(PLFW_PLATFORM_ERROR,
                            "Wayland: Error while writing the clipboard: %s",
                            strerror(errno));
            break;
        }

        length -= result;
        string += result;
    }

    close(fd);
}

static void dataSourceHandleCancelled(void* userData,
                                      struct wl_data_source* source)
{
    wl_data_source_destroy(source);

    if (_plfw.wl.selectionSource != source)
        return;

    _plfw.wl.selectionSource = NULL;
}

static const struct wl_data_source_listener dataSourceListener =
{
    dataSourceHandleTarget,
    dataSourceHandleSend,
    dataSourceHandleCancelled,
};

void _plfwSetClipboardStringWayland(const char* string)
{
    if (!_plfw.wl.dataDeviceManager || !_plfw.wl.dataDevice)
    {
        // A compositor may advertise no seat at all (headless or remote setups),
        // and a data source can only be attached through a data device.
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: The clipboard requires a seat with a data device");
        return;
    }

    if (_plfw.wl.selectionSource)
    {
        wl_data_source_destroy(_plfw.wl.selectionSource);
        _plfw.wl.selectionSource = NULL;
    }

    char* copy = _plfw_strdup(string);
    if (!copy)
    {
        _plfwInputError(PLFW_OUT_OF_MEMORY, NULL);
        return;
    }

    _plfw_free(_plfw.wl.clipboardString);
    _plfw.wl.clipboardString = copy;

    _plfw.wl.selectionSource =
        wl_data_device_manager_create_data_source(_plfw.wl.dataDeviceManager);
    if (!_plfw.wl.selectionSource)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: Failed to create clipboard data source");
        return;
    }
    wl_data_source_add_listener(_plfw.wl.selectionSource,
                                &dataSourceListener,
                                NULL);
    wl_data_source_offer(_plfw.wl.selectionSource, "text/plain;charset=utf-8");
    wl_data_device_set_selection(_plfw.wl.dataDevice,
                                 _plfw.wl.selectionSource,
                                 _plfw.wl.serial);
}

const char* _plfwGetClipboardStringWayland(void)
{
    if (!_plfw.wl.dataDeviceManager || !_plfw.wl.dataDevice)
    {
        _plfwInputError(PLFW_PLATFORM_ERROR,
                        "Wayland: The clipboard requires a seat with a data device");
        return NULL;
    }

    if (!_plfw.wl.selectionOffer)
    {
        _plfwInputError(PLFW_FORMAT_UNAVAILABLE,
                        "Wayland: No clipboard data available");
        return NULL;
    }

    if (_plfw.wl.selectionSource)
        return _plfw.wl.clipboardString;

    _plfw_free(_plfw.wl.clipboardString);
    _plfw.wl.clipboardString =
        readDataOfferAsString(_plfw.wl.selectionOffer, "text/plain;charset=utf-8");
    return _plfw.wl.clipboardString;
}

//////////////////////////////////////////////////////////////////////////
//////                        PLFW native API                       //////
//////////////////////////////////////////////////////////////////////////

PLFWAPI struct wl_display* plfwGetWaylandDisplay(void)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_plfw.platform.platformID != PLFW_PLATFORM_WAYLAND)
    {
        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE,
                        "Wayland: Platform not initialized");
        return NULL;
    }

    return _plfw.wl.display;
}

PLFWAPI struct wl_surface* plfwGetWaylandWindow(PLFWwindow* handle)
{
    _PLFW_REQUIRE_INIT_OR_RETURN(NULL);

    if (_plfw.platform.platformID != PLFW_PLATFORM_WAYLAND)
    {
        _plfwInputError(PLFW_PLATFORM_UNAVAILABLE,
                        "Wayland: Platform not initialized");
        return NULL;
    }

    _PLFWwindow* window = (_PLFWwindow*) handle;
    assert(window != NULL);

    return window->wl.surface;
}

#endif // _PLFW_WAYLAND

