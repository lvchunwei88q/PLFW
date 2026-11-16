// =========================================================================
//  hello-window.c -- the smallest useful PLFW program.
//
//  This windowing-only build of PLFW has no graphics context: plfwCreateWindow
//  returns a native window, an input state and an event queue, and nothing
//  else.  There is no plfwMakeContextCurrent, no plfwSwapBuffers and no
//  plfwGetProcAddress, so this example draws nothing at all -- the window
//  stays whatever colour the system paints it.  How a real application puts
//  pixels into the window (Direct3D, Vulkan, GDI, a child control, ...) is
//  entirely its own business.
//
//  What this program shows:
//    - plfwInit / plfwTerminate and an error callback
//    - one window with a title and a fixed size, created without any graphics
//      API
//    - a key callback (Escape closes the window) and a size callback
//    - the event loop: plfwWaitEventsTimeout sleeps instead of spinning
//
//  Press Escape or close the window to exit.
// =========================================================================

#include <stdio.h>

#include <PLFW/plfw3.h>

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "PLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static const char* platformName(int platform)
{
    switch (platform)
    {
        case PLFW_PLATFORM_WIN32:   return "Win32";
        case PLFW_PLATFORM_WAYLAND: return "Wayland";
        case PLFW_PLATFORM_NULL:    return "Null";
        default:                    return "unknown";
    }
}

static void keyCallback(PLFWwindow* window, int key, int scancode, int action, int mods)
{
    // scancode and mods are unused here; the parameters are part of the
    // callback signature and must still be accepted.
    (void) scancode;
    (void) mods;

    if (key == PLFW_KEY_ESCAPE && action == PLFW_PRESS)
        plfwSetWindowShouldClose(window, PLFW_TRUE);
}

static void windowSizeCallback(PLFWwindow* window, int width, int height)
{
    printf("[size]  window is now %dx%d\n", width, height);
}

int main(void)
{
    PLFWwindow* window;
    int width, height;

    plfwSetErrorCallback(errorCallback);

    if (!plfwInit())
        return 1;

    // Ask for a 640x400 client area.  Everything else is left at the defaults:
    // resizable, decorated, focused and visible.
    plfwWindowHint(PLFW_RESIZABLE, PLFW_TRUE);

    window = plfwCreateWindow(640, 400, "PLFW hello window", NULL, NULL);
    if (!window)
    {
        plfwTerminate();
        return 1;
    }

    printf("backend: %s\n", platformName(plfwGetPlatform()));

    plfwGetWindowSize(window, &width, &height);
    printf("window:  \"%s\" %dx%d\n", "PLFW hello window", width, height);

    plfwSetKeyCallback(window, keyCallback);
    plfwSetWindowSizeCallback(window, windowSizeCallback);

    printf("press Escape or close the window to exit\n");

    // Wait for events instead of spinning: the timeout wakes the loop up
    // periodically so the program cannot hang if no events ever arrive.
    while (!plfwWindowShouldClose(window))
        plfwWaitEventsTimeout(0.1);

    plfwDestroyWindow(window);
    plfwTerminate();

    printf("bye\n");
    return 0;
}
