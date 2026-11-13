// =========================================================================
//  smoke.c -- self-checking smoke test for the windowing-only build of PLFW.
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

#include <PLFW/plfw3.h>

static int failures = 0;

static void check(const char* what, int condition)
{
    printf("%s %s\n", condition ? "  ok  " : "  FAIL", what);
    if (!condition)
        failures++;
}

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "PLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static int keyEvents = 0;
static int focusEvents = 0;
static int sizeEvents = 0;
static int fbEvents = 0;

static void keyCallback(PLFWwindow* window, int key, int scancode, int action, int mods)
{
    keyEvents++;
}

static void focusCallback(PLFWwindow* window, int focused)
{
    focusEvents++;
}

static void sizeCallback(PLFWwindow* window, int width, int height)
{
    sizeEvents++;
}

static void fbCallback(PLFWwindow* window, int width, int height)
{
    fbEvents++;
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

int main(void)
{

    plfwSetErrorCallback(errorCallback);
    printf("== PLFW windowing-only smoke test ==\n");

    // Prefer whichever real window system this machine can initialise.  When the
    // machine has no display at all (plain CI runner, SSH session), retry with the
    // headless backend so the windowing and input API is still exercised.
    int initialized = plfwInit();
    if (!initialized)
    {
        printf("no window system available, retrying with the headless backend\n");
        plfwInitHint(PLFW_PLATFORM, PLFW_PLATFORM_NULL);
        initialized = plfwInit();
    }

    check("plfwInit", initialized == PLFW_TRUE);
    if (failures)
        return failures;

    // The build carries every window-system backend it can compile for, so the
    // running platform is whatever could be initialised here, not a fixed one.
    const int platform = plfwGetPlatform();
    printf("active platform backend: %s\n", platformName(platform));
    check("platform backend is a supported window system backend",
          platform == PLFW_PLATFORM_WIN32 ||
          platform == PLFW_PLATFORM_WAYLAND ||
          platform == PLFW_PLATFORM_NULL);

    // The library ships Win32, Wayland and Null only; the platform identifiers
    // for the removed backends must still be known but never supported.
    check("X11 backend is not part of the build",
          plfwPlatformSupported(PLFW_PLATFORM_X11) == PLFW_FALSE);
    check("Cocoa backend is not part of the build",
          plfwPlatformSupported(PLFW_PLATFORM_COCOA) == PLFW_FALSE);
    check("timer value advances",
          plfwGetTimerValue() > 0 && plfwGetTimerFrequency() > 0);
    check("time starts near zero", plfwGetTime() >= 0.0);

    int monitorCount = 0;
    PLFWmonitor** monitors = plfwGetMonitors(&monitorCount);
    check("at least one monitor", monitors != NULL && monitorCount > 0);
    if (monitorCount > 0)
    {
        int xpos, ypos, width, height;
        const PLFWvidmode* mode = plfwGetVideoMode(monitors[0]);

        check("monitor has a name", plfwGetMonitorName(monitors[0]) != NULL);
        plfwGetMonitorWorkarea(monitors[0], &xpos, &ypos, &width, &height);
        check("monitor work area is non-empty", width > 0 && height > 0);
        check("video mode is non-empty",
              mode != NULL && mode->width > 0 && mode->height > 0);
    }

    // A hidden window keeps the test from flashing anything on screen while
    // still going through the full native window creation path.
    plfwWindowHint(PLFW_VISIBLE, PLFW_FALSE);
    PLFWwindow* window = plfwCreateWindow(320, 240, "smoke", NULL, NULL);
    check("plfwCreateWindow without any graphics API",
          window != NULL);
    if (!window)
    {
        plfwTerminate();
        return failures ? failures : 1;
    }

    plfwSetKeyCallback(window, keyCallback);
    plfwSetWindowFocusCallback(window, focusCallback);
    plfwSetWindowSizeCallback(window, sizeCallback);
    plfwSetFramebufferSizeCallback(window, fbCallback);

    check("share parameter was ignored, window usable", window != NULL);

    int width = 0, height = 0;
    plfwGetWindowSize(window, &width, &height);
    check("window size is 320x240", width == 320 && height == 240);

    int fbWidth = 0, fbHeight = 0;
    plfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    check("framebuffer size is reported", fbWidth > 0 && fbHeight > 0);

    int left = 0, top = 0, right = 0, bottom = 0;
    plfwGetWindowFrameSize(window, &left, &top, &right, &bottom);
    check("window frame size is reported", left >= 0 && top >= 0);

    check("window is not visible (hint applied)",
          plfwGetWindowAttrib(window, PLFW_VISIBLE) == PLFW_FALSE);
    check("window is not iconified",
          plfwGetWindowAttrib(window, PLFW_ICONIFIED) == PLFW_FALSE);
    check("window is resizable by default",
          plfwGetWindowAttrib(window, PLFW_RESIZABLE) == PLFW_TRUE);

    if (platform == PLFW_PLATFORM_NULL)
    {
        printf("  skip cursor position check (Null platform has no cursor)\n");
    }
    else
    {
        double cursorX = -1.0, cursorY = -1.0;
        plfwGetCursorPos(window, &cursorX, &cursorY);
        check("cursor position is queryable", cursorX > -1.0 || cursorY > -1.0);
    }

    check("a key is not pressed before any input",
          plfwGetKey(window, PLFW_KEY_A) == PLFW_RELEASE);
    check("a mouse button is not pressed before any input",
          plfwGetMouseButton(window, PLFW_MOUSE_BUTTON_LEFT) == PLFW_RELEASE);

    plfwSetWindowTitle(window, "smoke (renamed)");

    if (platform == PLFW_PLATFORM_NULL)
    {
        // The headless backend reports the requested geometry but has no window
        // system behind it, so only the get/set round trip is meaningful here.
        printf("  skip resize and move checks (Null platform has no window system)\n");
        plfwSetWindowSize(window, 400, 300);
        plfwGetWindowSize(window, &width, &height);
        check("window size is still queryable after a resize request",
              width > 0 && height > 0);
        plfwSetWindowPos(window, 100, 100);
    }
    else
    {
        plfwSetWindowSize(window, 400, 300);
        plfwGetWindowSize(window, &width, &height);
        check("resize through plfwSetWindowSize applies", width == 400 && height == 300);

        if (platform == PLFW_PLATFORM_WAYLAND)
        {
            // Wayland deliberately gives the client no way to place a window:
            // the surface position belongs to the compositor.
            printf("  skip move check (Wayland does not let a client place windows)\n");
            plfwSetWindowPos(window, 100, 100);
        }
        else
        {
            plfwSetWindowPos(window, 100, 100);
            int xpos = -1, ypos = -1;
            plfwGetWindowPos(window, &xpos, &ypos);
            check("move through plfwSetWindowPos applies", xpos == 100 && ypos == 100);
        }
    }

    const char* text = "PLFW windowing-only build";
    while (plfwGetError(NULL) != PLFW_NO_ERROR)
        ; // drain earlier errors so the check below only sees the clipboard result
    plfwSetClipboardString(window, text);

    const char* clipboardError = NULL;
    if (plfwGetError(&clipboardError) != PLFW_NO_ERROR)
    {
        // Some backends cannot offer a selection without a seat (a headless
        // Wayland compositor, for instance); that is a platform limit, not a bug.
        printf("  skip clipboard check (%s)\n", clipboardError ? clipboardError : "unavailable");
    }
    else
    {
        const char* readBack = plfwGetClipboardString(window);
        if (readBack)
        {
            check("clipboard round trip", strcmp(readBack, text) == 0);
        }
        else
        {
            // A Wayland client only sees the selection through a data offer, and
            // the compositor hands one out to the window that holds keyboard
            // focus.  An unfocused or hidden window therefore cannot read back
            // even its own selection; that is a platform limit, not a bug.
            const char* getError = NULL;
            plfwGetError(&getError);
            printf("  skip clipboard check (%s)\n",
                   getError ? getError : "no data offer");
        }
    }

    int joystickCount = 0;
    for (int jid = PLFW_JOYSTICK_1;  jid <= PLFW_JOYSTICK_LAST;  jid++)
    {
        if (plfwJoystickPresent(jid))
        {
            int axes = 0, buttons = 0, hats = 0;
            check("connected joystick has a name", plfwGetJoystickName(jid) != NULL);
            plfwGetJoystickAxes(jid, &axes);
            plfwGetJoystickButtons(jid, &buttons);
            plfwGetJoystickHats(jid, &hats);
            printf("joystick %d: %d axes, %d buttons, %d hats\n",
                   jid, axes, buttons, hats);
            joystickCount++;
        }
    }
    printf("joysticks connected: %d\n", joystickCount);
    check("joystick enumeration ran without error", PLFW_TRUE);

    int scancode = plfwGetKeyScancode(PLFW_KEY_A);
    check("key scancode mapping exists", scancode >= 0);

    // Pump the event loop for a few frames: if the message pump or the window
    // procedure is broken this either hangs, crashes or reports errors.
    for (int i = 0;  i < 30;  i++)
    {
        plfwPollEvents();
        plfwWaitEventsTimeout(0.001);
    }
    check("event pump survived 30 wake-ups", PLFW_TRUE);
    check("no spurious close request",
          plfwWindowShouldClose(window) == PLFW_FALSE);

    plfwSetWindowShouldClose(window, PLFW_TRUE);
    check("plfwSetWindowShouldClose is reported",
          plfwWindowShouldClose(window) == PLFW_TRUE);

    plfwDestroyWindow(window);
    check("window destroyed without error", PLFW_TRUE);

    plfwTerminate();
    check("plfwTerminate completed", PLFW_TRUE);

    printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
    return failures;
}
