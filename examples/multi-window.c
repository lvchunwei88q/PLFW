// =========================================================================
//  multi-window.c -- several windows, native handles and window state for the
//  windowing-only build of PLFW.
//
//  This windowing-only build of PLFW has no graphics context support:
//  plfwCreateWindow does not create an OpenGL, OpenGL ES, Vulkan or OSMesa
//  context, and there is no glfwMakeContextCurrent / glfwSwapBuffers /
//  glfwGetProcAddress.  What it still owns is the native window, the input
//  state, the monitors and the event queue, which is exactly what this
//  program exercises.
//
//  What this program shows:
//    - creating several windows from one event loop
//    - a per-window user pointer, so one callback set serves every window
//    - window state: hide / show, focus, iconify / restore, decorations
//    - native window handles: plfwGetWin32Window, plfwGetWaylandWindow
//    - monitor enumeration and which monitor a window sits on
//
//  Keys: 1 / 2 / 3 hide or show that window, I iconifies the first window,
//        D toggles decorations on the second, Escape closes everything.
// =========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
 #define PLFW_EXPOSE_NATIVE_WIN32
#elif defined(PLFW_EXAMPLE_HAS_WAYLAND)
 // Whether plfwGetWaylandWindow exists is a property of the library that was
 // built, not of the headers that happen to be installed: a Wayland-less build
 // (PLFW_BUILD_WAYLAND=OFF) has no such symbol, so examples/CMakeLists.txt
 // passes this definition only when that backend was compiled in.
 #define PLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <PLFW/plfw3.h>
#include <PLFW/plfw3native.h>

#define WINDOW_COUNT 3

typedef struct WindowState
{
    const char* label;
    int         decorated;
} WindowState;

static PLFWwindow* windows[WINDOW_COUNT];
static WindowState states[WINDOW_COUNT] =
{
    { "first",  1 },
    { "second", 1 },
    { "third",  1 },
};

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "PLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static int windowIndex(PLFWwindow* window)
{
    int index = (int) (intptr_t) plfwGetWindowUserPointer(window);
    if (index < 0 || index >= WINDOW_COUNT)
        return -1;
    return index;
}

static const char* labelOf(PLFWwindow* window)
{
    int index = windowIndex(window);
    return (index >= 0) ? states[index].label : "unknown";
}

static void printNativeHandle(PLFWwindow* window)
{
    printf("[%s] native window handle: ", labelOf(window));

    switch (plfwGetPlatform())
    {
        case PLFW_PLATFORM_WIN32:
        {
#if defined(PLFW_EXPOSE_NATIVE_WIN32)
            HWND handle = plfwGetWin32Window(window);
            printf("HWND %p\n", (void*) handle);
            return;
#else
            break;
#endif
        }

        case PLFW_PLATFORM_WAYLAND:
        {
#if defined(PLFW_EXPOSE_NATIVE_WAYLAND)
            struct wl_surface* surface = plfwGetWaylandWindow(window);
            printf("wl_surface %p\n", (void*) surface);
            return;
#else
            break;
#endif
        }

        default:
            break;
    }

    printf("not exposed by this build\n");
}

static void reportMonitor(PLFWwindow* window, int index)
{
    int monitorCount = 0;
    PLFWmonitor** monitors = plfwGetMonitors(&monitorCount);

    int x = 0, y = 0;
    plfwGetWindowPos(window, &x, &y);

    if (plfwGetWindowMonitor(window))
    {
        printf("[%s] placed at %d,%d in full screen\n", states[index].label, x, y);
        return;
    }

    // no fullscreen was ever requested, so find the monitor whose work area
    // contains the window origin instead
    for (int i = 0;  i < monitorCount;  i++)
    {
        int mx = 0, my = 0, mw = 0, mh = 0;
        plfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
        if (x >= mx && y >= my && x < mx + mw && y < my + mh)
        {
            const char* name = plfwGetMonitorName(monitors[i]);
            printf("[%s] placed at %d,%d on monitor \"%s\" (%d of %d)\n",
                   states[index].label, x, y, name ? name : "unnamed",
                   i + 1, monitorCount);
            return;
        }
    }

    printf("[%s] placed at %d,%d on no known monitor\n",
           states[index].label, x, y);
}

static void toggleVisibility(int index)
{
    if (!windows[index])
        return;

    if (plfwGetWindowAttrib(windows[index], PLFW_VISIBLE))
    {
        printf("[%s] hiding\n", states[index].label);
        plfwHideWindow(windows[index]);
    }
    else
    {
        printf("[%s] showing\n", states[index].label);
        plfwShowWindow(windows[index]);
        plfwFocusWindow(windows[index]);
    }
}

static void keyCallback(PLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != PLFW_PRESS)
        return;

    if (key == PLFW_KEY_ESCAPE)
    {
        printf("Escape pressed, closing every window\n");
        for (int i = 0;  i < WINDOW_COUNT;  i++)
        {
            if (windows[i])
                plfwSetWindowShouldClose(windows[i], PLFW_TRUE);
        }
        return;
    }

    if (key >= PLFW_KEY_1 && key <= PLFW_KEY_3)
    {
        toggleVisibility(key - PLFW_KEY_1);
        return;
    }

    if (key == PLFW_KEY_I && windows[0])
    {
        if (plfwGetWindowAttrib(windows[0], PLFW_ICONIFIED))
        {
            printf("[%s] restoring\n", states[0].label);
            plfwRestoreWindow(windows[0]);
        }
        else
        {
            printf("[%s] iconifying\n", states[0].label);
            plfwIconifyWindow(windows[0]);
        }
        return;
    }

    if (key == PLFW_KEY_D && windows[1])
    {
        states[1].decorated = !states[1].decorated;
        printf("[%s] decorations %s\n", states[1].label,
               states[1].decorated ? "on" : "off");
        plfwSetWindowAttrib(windows[1], PLFW_DECORATED,
                            states[1].decorated ? PLFW_TRUE : PLFW_FALSE);
    }
}

static void focusCallback(PLFWwindow* window, int focused)
{
    printf("[%s] %s\n", labelOf(window), focused ? "focused" : "defocused");
}

static void sizeCallback(PLFWwindow* window, int width, int height)
{
    printf("[%s] resized to %dx%d\n", labelOf(window), width, height);
}

static void closeCallback(PLFWwindow* window)
{
    printf("[%s] close requested\n", labelOf(window));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    plfwSetErrorCallback(errorCallback);

    if (!plfwInit())
        return -1;

    printf("windowing-only build, platform %d\n", plfwGetPlatform());

    // No graphics API is involved anywhere in this build, so there is no
    // context hint to set: the window is a plain native window.
    for (int i = 0;  i < WINDOW_COUNT;  i++)
    {
        char title[64];
        snprintf(title, sizeof(title), "multi-window #%d", i + 1);

        windows[i] = plfwCreateWindow(320, 200, title, NULL, NULL);
        if (!windows[i])
        {
            fprintf(stderr, "failed to create window %d\n", i + 1);
            plfwTerminate();
            return -1;
        }

        plfwSetWindowUserPointer(windows[i], (void*) (intptr_t) i);
        plfwSetKeyCallback(windows[i], keyCallback);
        plfwSetWindowFocusCallback(windows[i], focusCallback);
        plfwSetWindowSizeCallback(windows[i], sizeCallback);
        plfwSetWindowCloseCallback(windows[i], closeCallback);

        plfwSetWindowPos(windows[i], 80 + i * 360, 120 + i * 40);

        printNativeHandle(windows[i]);
        reportMonitor(windows[i], i);
    }

    const char* text = "shared by all three windows";
    plfwSetClipboardString(windows[0], text);
    const char* readBack = plfwGetClipboardString(windows[1]);
    printf("clipboard through window 2: \"%s\"\n",
           readBack ? readBack : "(unavailable on this backend)");

    printf("keys: 1/2/3 hide or show a window, I iconify, D toggle decorations, "
           "Escape quits\n");

    while (windows[0] || windows[1] || windows[2])
    {
        plfwWaitEventsTimeout(0.25);

        for (int i = 0;  i < WINDOW_COUNT;  i++)
        {
            if (windows[i] && plfwWindowShouldClose(windows[i]))
            {
                plfwDestroyWindow(windows[i]);
                windows[i] = NULL;
            }
        }
    }

    plfwTerminate();
    return 0;
}
