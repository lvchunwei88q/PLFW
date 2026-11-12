// =========================================================================
//  multi-window.c -- several windows, native handles and window state for the
//  windowing-only build of GLFW.
//
//  This fork of GLFW has no graphics context support: glfwCreateWindow does
//  not create an OpenGL, OpenGL ES, Vulkan or OSMesa context, and there is no
//  glfwMakeContextCurrent / glfwSwapBuffers / glfwGetProcAddress.  What it
//  still owns is the native window, the input state, the monitors and the
//  event queue, which is exactly what this program exercises.
//
//  What this program shows:
//    - creating several windows from one event loop
//    - a per-window user pointer, so one callback set serves every window
//    - window state: hide / show, focus, iconify / restore, decorations
//    - native window handles: glfwGetWin32Window, glfwGetWaylandWindow
//    - monitor enumeration and which monitor a window sits on
//
//  Keys: 1 / 2 / 3 hide or show that window, I iconifies the first window,
//        D toggles decorations on the second, Escape closes everything.
// =========================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
 #define GLFW_EXPOSE_NATIVE_WIN32
#else
 // The library carries no X11 backend on Linux anymore, only Wayland.
 #if defined(__has_include)
  #if __has_include(<wayland-client.h>)
   #define GLFW_EXPOSE_NATIVE_WAYLAND
  #endif
 #endif
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define WINDOW_COUNT 3

typedef struct WindowState
{
    const char* label;
    int         decorated;
} WindowState;

static GLFWwindow* windows[WINDOW_COUNT];
static WindowState states[WINDOW_COUNT] =
{
    { "first",  1 },
    { "second", 1 },
    { "third",  1 },
};

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "GLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static int windowIndex(GLFWwindow* window)
{
    int index = (int) (intptr_t) glfwGetWindowUserPointer(window);
    if (index < 0 || index >= WINDOW_COUNT)
        return -1;
    return index;
}

static const char* labelOf(GLFWwindow* window)
{
    int index = windowIndex(window);
    return (index >= 0) ? states[index].label : "unknown";
}

static void printNativeHandle(GLFWwindow* window)
{
    printf("[%s] native window handle: ", labelOf(window));

    switch (glfwGetPlatform())
    {
        case GLFW_PLATFORM_WIN32:
        {
#if defined(GLFW_EXPOSE_NATIVE_WIN32)
            HWND handle = glfwGetWin32Window(window);
            printf("HWND %p\n", (void*) handle);
            return;
#else
            break;
#endif
        }

        case GLFW_PLATFORM_WAYLAND:
        {
#if defined(GLFW_EXPOSE_NATIVE_WAYLAND)
            struct wl_surface* surface = glfwGetWaylandWindow(window);
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

static void reportMonitor(GLFWwindow* window, int index)
{
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    int x = 0, y = 0;
    glfwGetWindowPos(window, &x, &y);

    if (glfwGetWindowMonitor(window))
    {
        printf("[%s] placed at %d,%d in full screen\n", states[index].label, x, y);
        return;
    }

    // no fullscreen was ever requested, so find the monitor whose work area
    // contains the window origin instead
    for (int i = 0;  i < monitorCount;  i++)
    {
        int mx = 0, my = 0, mw = 0, mh = 0;
        glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
        if (x >= mx && y >= my && x < mx + mw && y < my + mh)
        {
            const char* name = glfwGetMonitorName(monitors[i]);
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

    if (glfwGetWindowAttrib(windows[index], GLFW_VISIBLE))
    {
        printf("[%s] hiding\n", states[index].label);
        glfwHideWindow(windows[index]);
    }
    else
    {
        printf("[%s] showing\n", states[index].label);
        glfwShowWindow(windows[index]);
        glfwFocusWindow(windows[index]);
    }
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS)
        return;

    if (key == GLFW_KEY_ESCAPE)
    {
        printf("Escape pressed, closing every window\n");
        for (int i = 0;  i < WINDOW_COUNT;  i++)
        {
            if (windows[i])
                glfwSetWindowShouldClose(windows[i], GLFW_TRUE);
        }
        return;
    }

    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_3)
    {
        toggleVisibility(key - GLFW_KEY_1);
        return;
    }

    if (key == GLFW_KEY_I && windows[0])
    {
        if (glfwGetWindowAttrib(windows[0], GLFW_ICONIFIED))
        {
            printf("[%s] restoring\n", states[0].label);
            glfwRestoreWindow(windows[0]);
        }
        else
        {
            printf("[%s] iconifying\n", states[0].label);
            glfwIconifyWindow(windows[0]);
        }
        return;
    }

    if (key == GLFW_KEY_D && windows[1])
    {
        states[1].decorated = !states[1].decorated;
        printf("[%s] decorations %s\n", states[1].label,
               states[1].decorated ? "on" : "off");
        glfwSetWindowAttrib(windows[1], GLFW_DECORATED,
                            states[1].decorated ? GLFW_TRUE : GLFW_FALSE);
    }
}

static void focusCallback(GLFWwindow* window, int focused)
{
    printf("[%s] %s\n", labelOf(window), focused ? "focused" : "defocused");
}

static void sizeCallback(GLFWwindow* window, int width, int height)
{
    printf("[%s] resized to %dx%d\n", labelOf(window), width, height);
}

static void closeCallback(GLFWwindow* window)
{
    printf("[%s] close requested\n", labelOf(window));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    glfwSetErrorCallback(errorCallback);

    if (!glfwInit())
        return -1;

    printf("version string: %s\n", glfwGetVersionString());

    // No graphics API is involved anywhere in this build, so there is no
    // context hint to set: the window is a plain native window.
    for (int i = 0;  i < WINDOW_COUNT;  i++)
    {
        char title[64];
        snprintf(title, sizeof(title), "multi-window #%d", i + 1);

        windows[i] = glfwCreateWindow(320, 200, title, NULL, NULL);
        if (!windows[i])
        {
            fprintf(stderr, "failed to create window %d\n", i + 1);
            glfwTerminate();
            return -1;
        }

        glfwSetWindowUserPointer(windows[i], (void*) (intptr_t) i);
        glfwSetKeyCallback(windows[i], keyCallback);
        glfwSetWindowFocusCallback(windows[i], focusCallback);
        glfwSetWindowSizeCallback(windows[i], sizeCallback);
        glfwSetWindowCloseCallback(windows[i], closeCallback);

        glfwSetWindowPos(windows[i], 80 + i * 360, 120 + i * 40);

        printNativeHandle(windows[i]);
        reportMonitor(windows[i], i);
    }

    const char* text = "shared by all three windows";
    glfwSetClipboardString(windows[0], text);
    const char* readBack = glfwGetClipboardString(windows[1]);
    printf("clipboard through window 2: \"%s\"\n",
           readBack ? readBack : "(unavailable on this backend)");

    printf("keys: 1/2/3 hide or show a window, I iconify, D toggle decorations, "
           "Escape quits\n");

    while (windows[0] || windows[1] || windows[2])
    {
        glfwWaitEventsTimeout(0.25);

        for (int i = 0;  i < WINDOW_COUNT;  i++)
        {
            if (windows[i] && glfwWindowShouldClose(windows[i]))
            {
                glfwDestroyWindow(windows[i]);
                windows[i] = NULL;
            }
        }
    }

    glfwTerminate();
    return 0;
}
