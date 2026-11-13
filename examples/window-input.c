// =========================================================================
//  window-input.c -- window, input and event demo for the windowing-only
//  build of PLFW.
//
//  This windowing-only build of PLFW has no graphics context support:
//  plfwCreateWindow does not create an OpenGL, OpenGL ES, Vulkan or OSMesa
//  context, and there is no glfwMakeContextCurrent / glfwSwapBuffers /
//  glfwGetProcAddress.  The library owns the native window, the input state
//  and the event queue; how anything gets drawn into the window is entirely
//  up to the application (Direct3D, Vulkan, GDI, a child control, ...).
//
//  What this program shows:
//    - creating a window without any graphics API
//    - callbacks: key, char, cursor position, mouse button, scroll,
//      window focus, window size, framebuffer size, joystick hotplug
//    - polling: plfwGetKey, plfwGetMouseButton, plfwGetCursorPos,
//      plfwJoystickPresent, plfwGetJoystickAxes/Buttons/Hats
//    - the event loop: plfwPollEvents and plfwWaitEventsTimeout
//
//  Press Escape or close the window to exit.
// =========================================================================

#include <stdio.h>
#include <stdlib.h>

#include <PLFW/plfw3.h>

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "PLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static void keyCallback(PLFWwindow* window, int key, int scancode, int action, int mods)
{
    const char* name = plfwGetKeyName(key, scancode);
    printf("[key]   key=%-3d scancode=%-3d %-5s mods=0x%02X (%s)\n",
           key, scancode,
           action == PLFW_PRESS ? "press" :
           action == PLFW_RELEASE ? "rel" : "repeat",
           (unsigned int) mods,
           name ? name : "unnamed");

    if (key == PLFW_KEY_ESCAPE && action == PLFW_PRESS)
        plfwSetWindowShouldClose(window, PLFW_TRUE);
}

static void charCallback(PLFWwindow* window, unsigned int codepoint)
{
    printf("[char]  U+%04X '%c'\n", codepoint,
           (codepoint >= 0x20 && codepoint < 0x7F) ? (char) codepoint : '?');
}

static void cursorPosCallback(PLFWwindow* window, double xpos, double ypos)
{
    printf("[cursor] %.0f, %.0f\n", xpos, ypos);
}

static void mouseButtonCallback(PLFWwindow* window, int button, int action, int mods)
{
    printf("[mouse] button=%d %s mods=0x%02X\n",
           button, action == PLFW_PRESS ? "press" : "release",
           (unsigned int) mods);
}

static void scrollCallback(PLFWwindow* window, double xoffset, double yoffset)
{
    printf("[scroll] %.2f, %.2f\n", xoffset, yoffset);
}

static void focusCallback(PLFWwindow* window, int focused)
{
    printf("[focus] %s\n", focused ? "gained" : "lost");
}

static void windowSizeCallback(PLFWwindow* window, int width, int height)
{
    printf("[size]  window %dx%d\n", width, height);
}

static void framebufferSizeCallback(PLFWwindow* window, int width, int height)
{
    printf("[fb]    framebuffer %dx%d\n", width, height);
}

static void joystickCallback(int jid, int event)
{
    if (event == PLFW_CONNECTED)
        printf("[pad]   joystick %d connected: %s\n", jid, plfwGetJoystickName(jid));
    else
        printf("[pad]   joystick %d disconnected\n", jid);
}

// Print the state of every connected gamepad.  Called once per second so the
// console is not flooded by axis updates.
static void reportJoysticks(void)
{
    for (int jid = PLFW_JOYSTICK_1;  jid <= PLFW_JOYSTICK_LAST;  jid++)
    {
        int axisCount, buttonCount, hatCount;
        const float* axes;
        const unsigned char* buttons;
        const unsigned char* hats;

        if (!plfwJoystickPresent(jid))
            continue;

        axes = plfwGetJoystickAxes(jid, &axisCount);
        buttons = plfwGetJoystickButtons(jid, &buttonCount);
        hats = plfwGetJoystickHats(jid, &hatCount);

        printf("[pad]   %d \"%s\": %d axes, %d buttons, %d hats\n",
               jid, plfwGetJoystickName(jid), axisCount, buttonCount, hatCount);

        if (axes && axisCount > 0)
        {
            printf("[pad]     axes:");
            for (int i = 0;  i < axisCount;  i++)
                printf(" %.2f", axes[i]);
            printf("\n");
        }

        if (buttons && buttonCount > 0)
        {
            printf("[pad]     buttons:");
            for (int i = 0;  i < buttonCount;  i++)
                printf(" %d", buttons[i]);
            printf("\n");
        }

        if (hats && hatCount > 0)
        {
            printf("[pad]     hats:");
            for (int i = 0;  i < hatCount;  i++)
                printf(" %d", hats[i]);
            printf("\n");
        }
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    plfwSetErrorCallback(errorCallback);

    if (!plfwInit())
        return 1;

    printf("windowing-only build, platform %s\n",
           plfwGetPlatform() == PLFW_PLATFORM_WIN32 ? "Win32" :
           plfwGetPlatform() == PLFW_PLATFORM_WAYLAND ? "Wayland" : "Null");

    // The window is created without any graphics API: the share parameter of
    // plfwCreateWindow is accepted for source compatibility and ignored.
    PLFWwindow* window = plfwCreateWindow(800, 480,
                                          "PLFW window / input demo (no graphics API)",
                                          NULL, NULL);
    if (!window)
    {
        plfwTerminate();
        return 1;
    }

    plfwSetKeyCallback(window, keyCallback);
    plfwSetCharCallback(window, charCallback);
    plfwSetCursorPosCallback(window, cursorPosCallback);
    plfwSetMouseButtonCallback(window, mouseButtonCallback);
    plfwSetScrollCallback(window, scrollCallback);
    plfwSetWindowFocusCallback(window, focusCallback);
    plfwSetWindowSizeCallback(window, windowSizeCallback);
    plfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    plfwSetJoystickCallback(joystickCallback);

    // Cursor mode is a window-system feature and works without a context
    printf("cursor mode: normal, sticky keys off\n");
    plfwSetInputMode(window, PLFW_CURSOR, PLFW_CURSOR_NORMAL);

    printf("monitors:\n");
    int monitorCount;
    PLFWmonitor** monitors = plfwGetMonitors(&monitorCount);
    for (int i = 0;  i < monitorCount;  i++)
    {
        int xpos, ypos, width, height;
        const PLFWvidmode* mode = plfwGetVideoMode(monitors[i]);
        plfwGetMonitorWorkarea(monitors[i], &xpos, &ypos, &width, &height);
        printf("  %s: video mode %dx%d @ %d Hz, work area %d,%d %dx%d\n",
               plfwGetMonitorName(monitors[i]),
               mode ? mode->width : 0, mode ? mode->height : 0,
               mode ? mode->refreshRate : 0, xpos, ypos, width, height);
    }

    printf("connected gamepads:\n");
    reportJoysticks();
    printf("event loop started, press Escape to exit\n");

    double lastReport = plfwGetTime();
    unsigned int frame = 0;

    while (!plfwWindowShouldClose(window))
    {
        // Wait for input instead of spinning: a real renderer would draw a
        // frame here using its own graphics API.
        plfwWaitEventsTimeout(1.0 / 60.0);
        frame++;

        const double now = plfwGetTime();
        if (now - lastReport >= 1.0)
        {
            double xpos, ypos;
            int width, height;
            plfwGetCursorPos(window, &xpos, &ypos);
            plfwGetWindowSize(window, &width, &height);
            printf("[tick]  t=%.2fs frame=%u window=%dx%d cursor=%.0f,%.0f "
                   "LMB=%d focused=%d\n",
                   now, frame, width, height, xpos, ypos,
                   plfwGetMouseButton(window, PLFW_MOUSE_BUTTON_LEFT) == PLFW_PRESS,
                   plfwGetWindowAttrib(window, PLFW_FOCUSED));

            reportJoysticks();
            lastReport = now;
        }
    }

    printf("closing window\n");
    plfwDestroyWindow(window);
    plfwTerminate();
    return 0;
}
