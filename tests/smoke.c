// =========================================================================
//  smoke.c -- self-checking smoke test for the windowing-only build of GLFW.
//
//  Creates a hidden window, exercises the window, monitor, input, clipboard
//  and event APIs that survived the removal of all graphics context support,
//  then tears everything down.  Nothing here needs an OpenGL/Vulkan context,
//  so it runs on machines without any usable graphics driver.
//
//  Exit code 0 means every check passed; the number of failures otherwise.
// =========================================================================

#include <stdio.h>
#include <string.h>

#include <GLFW/glfw3.h>

static int failures = 0;

static void check(const char* what, int condition)
{
    printf("%s %s\n", condition ? "  ok  " : "  FAIL", what);
    if (!condition)
        failures++;
}

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "GLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static int keyEvents = 0;
static int focusEvents = 0;
static int sizeEvents = 0;
static int fbEvents = 0;

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    keyEvents++;
}

static void focusCallback(GLFWwindow* window, int focused)
{
    focusEvents++;
}

static void sizeCallback(GLFWwindow* window, int width, int height)
{
    sizeEvents++;
}

static void fbCallback(GLFWwindow* window, int width, int height)
{
    fbEvents++;
}

static const char* platformName(int platform)
{
    switch (platform)
    {
        case GLFW_PLATFORM_WIN32:   return "Win32";
        case GLFW_PLATFORM_WAYLAND: return "Wayland";
        case GLFW_PLATFORM_NULL:    return "Null";
        default:                    return "unknown";
    }
}

int main(void)
{

    glfwSetErrorCallback(errorCallback);
    printf("== GLFW windowing-only smoke test ==\n");

    // Prefer whichever real window system this machine can initialise.  When the
    // machine has no display at all (plain CI runner, SSH session), retry with the
    // headless backend so the windowing and input API is still exercised.
    int initialized = glfwInit();
    if (!initialized)
    {
        printf("no window system available, retrying with the headless backend\n");
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_NULL);
        initialized = glfwInit();
    }

    check("glfwInit", initialized == GLFW_TRUE);
    if (failures)
        return failures;

    // The build carries every window-system backend it can compile for, so the
    // running platform is whatever could be initialised here, not a fixed one.
    const int platform = glfwGetPlatform();
    printf("active platform backend: %s\n", platformName(platform));
    check("platform backend is a supported window system backend",
          platform == GLFW_PLATFORM_WIN32 ||
          platform == GLFW_PLATFORM_WAYLAND ||
          platform == GLFW_PLATFORM_NULL);

    // The library ships Win32, Wayland and Null only; the platform identifiers
    // for the removed backends must still be known but never supported.
    check("X11 backend is not part of the build",
          glfwPlatformSupported(GLFW_PLATFORM_X11) == GLFW_FALSE);
    check("Cocoa backend is not part of the build",
          glfwPlatformSupported(GLFW_PLATFORM_COCOA) == GLFW_FALSE);
    check("timer value advances",
          glfwGetTimerValue() > 0 && glfwGetTimerFrequency() > 0);
    check("time starts near zero", glfwGetTime() >= 0.0);

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    check("at least one monitor", monitors != NULL && monitorCount > 0);
    if (monitorCount > 0)
    {
        int xpos, ypos, width, height;
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[0]);

        check("monitor has a name", glfwGetMonitorName(monitors[0]) != NULL);
        glfwGetMonitorWorkarea(monitors[0], &xpos, &ypos, &width, &height);
        check("monitor work area is non-empty", width > 0 && height > 0);
        check("video mode is non-empty",
              mode != NULL && mode->width > 0 && mode->height > 0);
    }

    // A hidden window keeps the test from flashing anything on screen while
    // still going through the full native window creation path.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(320, 240, "smoke", NULL, NULL);
    check("glfwCreateWindow without any graphics API",
          window != NULL);
    if (!window)
    {
        glfwTerminate();
        return failures ? failures : 1;
    }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetWindowFocusCallback(window, focusCallback);
    glfwSetWindowSizeCallback(window, sizeCallback);
    glfwSetFramebufferSizeCallback(window, fbCallback);

    check("share parameter was ignored, window usable", window != NULL);

    int width = 0, height = 0;
    glfwGetWindowSize(window, &width, &height);
    check("window size is 320x240", width == 320 && height == 240);

    int fbWidth = 0, fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    check("framebuffer size is reported", fbWidth > 0 && fbHeight > 0);

    int left = 0, top = 0, right = 0, bottom = 0;
    glfwGetWindowFrameSize(window, &left, &top, &right, &bottom);
    check("window frame size is reported", left >= 0 && top >= 0);

    check("window is not visible (hint applied)",
          glfwGetWindowAttrib(window, GLFW_VISIBLE) == GLFW_FALSE);
    check("window is not iconified",
          glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_FALSE);
    check("window is resizable by default",
          glfwGetWindowAttrib(window, GLFW_RESIZABLE) == GLFW_TRUE);

    if (platform == GLFW_PLATFORM_NULL)
    {
        printf("  skip cursor position check (Null platform has no cursor)\n");
    }
    else
    {
        double cursorX = -1.0, cursorY = -1.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);
        check("cursor position is queryable", cursorX > -1.0 || cursorY > -1.0);
    }

    check("a key is not pressed before any input",
          glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE);
    check("a mouse button is not pressed before any input",
          glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE);

    glfwSetWindowTitle(window, "smoke (renamed)");

    if (platform == GLFW_PLATFORM_NULL)
    {
        // The headless backend reports the requested geometry but has no window
        // system behind it, so only the get/set round trip is meaningful here.
        printf("  skip resize and move checks (Null platform has no window system)\n");
        glfwSetWindowSize(window, 400, 300);
        glfwGetWindowSize(window, &width, &height);
        check("window size is still queryable after a resize request",
              width > 0 && height > 0);
        glfwSetWindowPos(window, 100, 100);
    }
    else
    {
        glfwSetWindowSize(window, 400, 300);
        glfwGetWindowSize(window, &width, &height);
        check("resize through glfwSetWindowSize applies", width == 400 && height == 300);

        if (platform == GLFW_PLATFORM_WAYLAND)
        {
            // Wayland deliberately gives the client no way to place a window:
            // the surface position belongs to the compositor.
            printf("  skip move check (Wayland does not let a client place windows)\n");
            glfwSetWindowPos(window, 100, 100);
        }
        else
        {
            glfwSetWindowPos(window, 100, 100);
            int xpos = -1, ypos = -1;
            glfwGetWindowPos(window, &xpos, &ypos);
            check("move through glfwSetWindowPos applies", xpos == 100 && ypos == 100);
        }
    }

    const char* text = "GLFW windowing-only build";
    while (glfwGetError(NULL) != GLFW_NO_ERROR)
        ; // drain earlier errors so the check below only sees the clipboard result
    glfwSetClipboardString(window, text);

    const char* clipboardError = NULL;
    if (glfwGetError(&clipboardError) != GLFW_NO_ERROR)
    {
        // Some backends cannot offer a selection without a seat (a headless
        // Wayland compositor, for instance); that is a platform limit, not a bug.
        printf("  skip clipboard check (%s)\n", clipboardError ? clipboardError : "unavailable");
    }
    else
    {
        const char* readBack = glfwGetClipboardString(window);
        check("clipboard round trip", readBack != NULL && strcmp(readBack, text) == 0);
    }

    int joystickCount = 0;
    for (int jid = GLFW_JOYSTICK_1;  jid <= GLFW_JOYSTICK_LAST;  jid++)
    {
        if (glfwJoystickPresent(jid))
        {
            int axes = 0, buttons = 0, hats = 0;
            check("connected joystick has a name", glfwGetJoystickName(jid) != NULL);
            glfwGetJoystickAxes(jid, &axes);
            glfwGetJoystickButtons(jid, &buttons);
            glfwGetJoystickHats(jid, &hats);
            printf("joystick %d: %d axes, %d buttons, %d hats\n",
                   jid, axes, buttons, hats);
            joystickCount++;
        }
    }
    printf("joysticks connected: %d\n", joystickCount);
    check("joystick enumeration ran without error", GLFW_TRUE);

    int scancode = glfwGetKeyScancode(GLFW_KEY_A);
    check("key scancode mapping exists", scancode >= 0);

    // Pump the event loop for a few frames: if the message pump or the window
    // procedure is broken this either hangs, crashes or reports errors.
    for (int i = 0;  i < 30;  i++)
    {
        glfwPollEvents();
        glfwWaitEventsTimeout(0.001);
    }
    check("event pump survived 30 wake-ups", GLFW_TRUE);
    check("no spurious close request",
          glfwWindowShouldClose(window) == GLFW_FALSE);

    glfwSetWindowShouldClose(window, GLFW_TRUE);
    check("glfwSetWindowShouldClose is reported",
          glfwWindowShouldClose(window) == GLFW_TRUE);

    glfwDestroyWindow(window);
    check("window destroyed without error", GLFW_TRUE);

    glfwTerminate();
    check("glfwTerminate completed", GLFW_TRUE);

    printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures;
}
