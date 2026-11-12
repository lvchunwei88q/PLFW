// =========================================================================
//  window-input.c -- window, input and event demo for the windowing-only
//  build of GLFW.
//
//  This fork of GLFW has no graphics context support: glfwCreateWindow does
//  not create an OpenGL, OpenGL ES, Vulkan or OSMesa context, and there is no
//  glfwMakeContextCurrent / glfwSwapBuffers / glfwGetProcAddress.  The
//  library owns the native window, the input state and the event queue; how
//  anything gets drawn into the window is entirely up to the application
//  (Direct3D, Vulkan, GDI, a child control, ...).
//
//  What this program shows:
//    - creating a window without any graphics API
//    - callbacks: key, char, cursor position, mouse button, scroll,
//      window focus, window size, framebuffer size, joystick hotplug
//    - polling: glfwGetKey, glfwGetMouseButton, glfwGetCursorPos,
//      glfwJoystickPresent, glfwGetJoystickAxes/Buttons/Hats
//    - the event loop: glfwPollEvents and glfwWaitEventsTimeout
//
//  Press Escape or close the window to exit.
// =========================================================================

#include <stdio.h>
#include <stdlib.h>

#include <GLFW/glfw3.h>

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "GLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    const char* name = glfwGetKeyName(key, scancode);
    printf("[key]   key=%-3d scancode=%-3d %-5s mods=0x%02X (%s)\n",
           key, scancode,
           action == GLFW_PRESS ? "press" :
           action == GLFW_RELEASE ? "rel" : "repeat",
           (unsigned int) mods,
           name ? name : "unnamed");

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void charCallback(GLFWwindow* window, unsigned int codepoint)
{
    printf("[char]  U+%04X '%c'\n", codepoint,
           (codepoint >= 0x20 && codepoint < 0x7F) ? (char) codepoint : '?');
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
    printf("[cursor] %.0f, %.0f\n", xpos, ypos);
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    printf("[mouse] button=%d %s mods=0x%02X\n",
           button, action == GLFW_PRESS ? "press" : "release",
           (unsigned int) mods);
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    printf("[scroll] %.2f, %.2f\n", xoffset, yoffset);
}

static void focusCallback(GLFWwindow* window, int focused)
{
    printf("[focus] %s\n", focused ? "gained" : "lost");
}

static void windowSizeCallback(GLFWwindow* window, int width, int height)
{
    printf("[size]  window %dx%d\n", width, height);
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    printf("[fb]    framebuffer %dx%d\n", width, height);
}

static void joystickCallback(int jid, int event)
{
    if (event == GLFW_CONNECTED)
        printf("[pad]   joystick %d connected: %s\n", jid, glfwGetJoystickName(jid));
    else
        printf("[pad]   joystick %d disconnected\n", jid);
}

// Print the state of every connected gamepad.  Called once per second so the
// console is not flooded by axis updates.
static void reportJoysticks(void)
{
    for (int jid = GLFW_JOYSTICK_1;  jid <= GLFW_JOYSTICK_LAST;  jid++)
    {
        int axisCount, buttonCount, hatCount;
        const float* axes;
        const unsigned char* buttons;
        const unsigned char* hats;

        if (!glfwJoystickPresent(jid))
            continue;

        axes = glfwGetJoystickAxes(jid, &axisCount);
        buttons = glfwGetJoystickButtons(jid, &buttonCount);
        hats = glfwGetJoystickHats(jid, &hatCount);

        printf("[pad]   %d \"%s\": %d axes, %d buttons, %d hats\n",
               jid, glfwGetJoystickName(jid), axisCount, buttonCount, hatCount);

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

    glfwSetErrorCallback(errorCallback);

    if (!glfwInit())
        return 1;

    printf("windowing-only build, platform %s\n",
           glfwGetPlatform() == GLFW_PLATFORM_WIN32 ? "Win32" :
           glfwGetPlatform() == GLFW_PLATFORM_WAYLAND ? "Wayland" : "Null");

    // The window is created without any graphics API: the share parameter of
    // glfwCreateWindow is accepted for source compatibility and ignored.
    GLFWwindow* window = glfwCreateWindow(800, 480,
                                          "GLFW window / input demo (no graphics API)",
                                          NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetWindowFocusCallback(window, focusCallback);
    glfwSetWindowSizeCallback(window, windowSizeCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetJoystickCallback(joystickCallback);

    // Cursor mode is a window-system feature and works without a context
    printf("cursor mode: normal, sticky keys off\n");
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    printf("monitors:\n");
    int monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    for (int i = 0;  i < monitorCount;  i++)
    {
        int xpos, ypos, width, height;
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        glfwGetMonitorWorkarea(monitors[i], &xpos, &ypos, &width, &height);
        printf("  %s: video mode %dx%d @ %d Hz, work area %d,%d %dx%d\n",
               glfwGetMonitorName(monitors[i]),
               mode ? mode->width : 0, mode ? mode->height : 0,
               mode ? mode->refreshRate : 0, xpos, ypos, width, height);
    }

    printf("connected gamepads:\n");
    reportJoysticks();
    printf("event loop started, press Escape to exit\n");

    double lastReport = glfwGetTime();
    unsigned int frame = 0;

    while (!glfwWindowShouldClose(window))
    {
        // Wait for input instead of spinning: a real renderer would draw a
        // frame here using its own graphics API.
        glfwWaitEventsTimeout(1.0 / 60.0);
        frame++;

        const double now = glfwGetTime();
        if (now - lastReport >= 1.0)
        {
            double xpos, ypos;
            int width, height;
            glfwGetCursorPos(window, &xpos, &ypos);
            glfwGetWindowSize(window, &width, &height);
            printf("[tick]  t=%.2fs frame=%u window=%dx%d cursor=%.0f,%.0f "
                   "LMB=%d focused=%d\n",
                   now, frame, width, height, xpos, ypos,
                   glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS,
                   glfwGetWindowAttrib(window, GLFW_FOCUSED));

            reportJoysticks();
            lastReport = now;
        }
    }

    printf("closing window\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
