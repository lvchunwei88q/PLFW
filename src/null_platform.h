//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2016 Google Inc.
// Copyright (c) 2016-2017 Camilla Löwy <elmindreda@glfw.org>
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

#define PLFW_NULL_WINDOW_STATE          _PLFWwindowNull null;
#define PLFW_NULL_LIBRARY_WINDOW_STATE  _PLFWlibraryNull null;
#define PLFW_NULL_MONITOR_STATE         _PLFWmonitorNull null;

#define PLFW_NULL_CONTEXT_STATE
#define PLFW_NULL_CURSOR_STATE
#define PLFW_NULL_LIBRARY_CONTEXT_STATE

#define PLFW_NULL_SC_FIRST          PLFW_NULL_SC_SPACE
#define PLFW_NULL_SC_SPACE          1
#define PLFW_NULL_SC_APOSTROPHE     2
#define PLFW_NULL_SC_COMMA          3
#define PLFW_NULL_SC_MINUS          4
#define PLFW_NULL_SC_PERIOD         5
#define PLFW_NULL_SC_SLASH          6
#define PLFW_NULL_SC_0              7
#define PLFW_NULL_SC_1              8
#define PLFW_NULL_SC_2              9
#define PLFW_NULL_SC_3              10
#define PLFW_NULL_SC_4              11
#define PLFW_NULL_SC_5              12
#define PLFW_NULL_SC_6              13
#define PLFW_NULL_SC_7              14
#define PLFW_NULL_SC_8              15
#define PLFW_NULL_SC_9              16
#define PLFW_NULL_SC_SEMICOLON      17
#define PLFW_NULL_SC_EQUAL          18
#define PLFW_NULL_SC_LEFT_BRACKET   19
#define PLFW_NULL_SC_BACKSLASH      20
#define PLFW_NULL_SC_RIGHT_BRACKET  21
#define PLFW_NULL_SC_GRAVE_ACCENT   22
#define PLFW_NULL_SC_WORLD_1        23
#define PLFW_NULL_SC_WORLD_2        24
#define PLFW_NULL_SC_ESCAPE         25
#define PLFW_NULL_SC_ENTER          26
#define PLFW_NULL_SC_TAB            27
#define PLFW_NULL_SC_BACKSPACE      28
#define PLFW_NULL_SC_INSERT         29
#define PLFW_NULL_SC_DELETE         30
#define PLFW_NULL_SC_RIGHT          31
#define PLFW_NULL_SC_LEFT           32
#define PLFW_NULL_SC_DOWN           33
#define PLFW_NULL_SC_UP             34
#define PLFW_NULL_SC_PAGE_UP        35
#define PLFW_NULL_SC_PAGE_DOWN      36
#define PLFW_NULL_SC_HOME           37
#define PLFW_NULL_SC_END            38
#define PLFW_NULL_SC_CAPS_LOCK      39
#define PLFW_NULL_SC_SCROLL_LOCK    40
#define PLFW_NULL_SC_NUM_LOCK       41
#define PLFW_NULL_SC_PRINT_SCREEN   42
#define PLFW_NULL_SC_PAUSE          43
#define PLFW_NULL_SC_A              44
#define PLFW_NULL_SC_B              45
#define PLFW_NULL_SC_C              46
#define PLFW_NULL_SC_D              47
#define PLFW_NULL_SC_E              48
#define PLFW_NULL_SC_F              49
#define PLFW_NULL_SC_G              50
#define PLFW_NULL_SC_H              51
#define PLFW_NULL_SC_I              52
#define PLFW_NULL_SC_J              53
#define PLFW_NULL_SC_K              54
#define PLFW_NULL_SC_L              55
#define PLFW_NULL_SC_M              56
#define PLFW_NULL_SC_N              57
#define PLFW_NULL_SC_O              58
#define PLFW_NULL_SC_P              59
#define PLFW_NULL_SC_Q              60
#define PLFW_NULL_SC_R              61
#define PLFW_NULL_SC_S              62
#define PLFW_NULL_SC_T              63
#define PLFW_NULL_SC_U              64
#define PLFW_NULL_SC_V              65
#define PLFW_NULL_SC_W              66
#define PLFW_NULL_SC_X              67
#define PLFW_NULL_SC_Y              68
#define PLFW_NULL_SC_Z              69
#define PLFW_NULL_SC_F1             70
#define PLFW_NULL_SC_F2             71
#define PLFW_NULL_SC_F3             72
#define PLFW_NULL_SC_F4             73
#define PLFW_NULL_SC_F5             74
#define PLFW_NULL_SC_F6             75
#define PLFW_NULL_SC_F7             76
#define PLFW_NULL_SC_F8             77
#define PLFW_NULL_SC_F9             78
#define PLFW_NULL_SC_F10            79
#define PLFW_NULL_SC_F11            80
#define PLFW_NULL_SC_F12            81
#define PLFW_NULL_SC_F13            82
#define PLFW_NULL_SC_F14            83
#define PLFW_NULL_SC_F15            84
#define PLFW_NULL_SC_F16            85
#define PLFW_NULL_SC_F17            86
#define PLFW_NULL_SC_F18            87
#define PLFW_NULL_SC_F19            88
#define PLFW_NULL_SC_F20            89
#define PLFW_NULL_SC_F21            90
#define PLFW_NULL_SC_F22            91
#define PLFW_NULL_SC_F23            92
#define PLFW_NULL_SC_F24            93
#define PLFW_NULL_SC_F25            94
#define PLFW_NULL_SC_KP_0           95
#define PLFW_NULL_SC_KP_1           96
#define PLFW_NULL_SC_KP_2           97
#define PLFW_NULL_SC_KP_3           98
#define PLFW_NULL_SC_KP_4           99
#define PLFW_NULL_SC_KP_5           100
#define PLFW_NULL_SC_KP_6           101
#define PLFW_NULL_SC_KP_7           102
#define PLFW_NULL_SC_KP_8           103
#define PLFW_NULL_SC_KP_9           104
#define PLFW_NULL_SC_KP_DECIMAL     105
#define PLFW_NULL_SC_KP_DIVIDE      106
#define PLFW_NULL_SC_KP_MULTIPLY    107
#define PLFW_NULL_SC_KP_SUBTRACT    108
#define PLFW_NULL_SC_KP_ADD         109
#define PLFW_NULL_SC_KP_ENTER       110
#define PLFW_NULL_SC_KP_EQUAL       111
#define PLFW_NULL_SC_LEFT_SHIFT     112
#define PLFW_NULL_SC_LEFT_CONTROL   113
#define PLFW_NULL_SC_LEFT_ALT       114
#define PLFW_NULL_SC_LEFT_SUPER     115
#define PLFW_NULL_SC_RIGHT_SHIFT    116
#define PLFW_NULL_SC_RIGHT_CONTROL  117
#define PLFW_NULL_SC_RIGHT_ALT      118
#define PLFW_NULL_SC_RIGHT_SUPER    119
#define PLFW_NULL_SC_MENU           120
#define PLFW_NULL_SC_LAST           PLFW_NULL_SC_MENU

// Null-specific per-window data
//
typedef struct _PLFWwindowNull
{
    int             xpos;
    int             ypos;
    int             width;
    int             height;
    PLFWbool        visible;
    PLFWbool        iconified;
    PLFWbool        maximized;
    PLFWbool        resizable;
    PLFWbool        decorated;
    PLFWbool        floating;
    float           opacity;
} _PLFWwindowNull;

// Null-specific per-monitor data
//
typedef struct _PLFWmonitorNull
{
    PLFWgammaramp   ramp;
} _PLFWmonitorNull;

// Null-specific global data
//
typedef struct _PLFWlibraryNull
{
    int             xcursor;
    int             ycursor;
    char*           clipboardString;
    _PLFWwindow*    focusedWindow;
    uint16_t        keycodes[PLFW_NULL_SC_LAST + 1];
    uint8_t         scancodes[PLFW_KEY_LAST + 1];
} _PLFWlibraryNull;

void _plfwPollMonitorsNull(void);

PLFWbool _plfwConnectNull(int platformID, _PLFWplatform* platform);
int _plfwInitNull(void);
void _plfwTerminateNull(void);

void _plfwFreeMonitorNull(_PLFWmonitor* monitor);
void _plfwGetMonitorPosNull(_PLFWmonitor* monitor, int* xpos, int* ypos);
void _plfwGetMonitorContentScaleNull(_PLFWmonitor* monitor, float* xscale, float* yscale);
void _plfwGetMonitorWorkareaNull(_PLFWmonitor* monitor, int* xpos, int* ypos, int* width, int* height);
PLFWvidmode* _plfwGetVideoModesNull(_PLFWmonitor* monitor, int* found);
PLFWbool _plfwGetVideoModeNull(_PLFWmonitor* monitor, PLFWvidmode* mode);
PLFWbool _plfwGetGammaRampNull(_PLFWmonitor* monitor, PLFWgammaramp* ramp);
void _plfwSetGammaRampNull(_PLFWmonitor* monitor, const PLFWgammaramp* ramp);

PLFWbool _plfwCreateWindowNull(_PLFWwindow* window, const _PLFWwndconfig* wndconfig, const _PLFWfbconfig* fbconfig);
void _plfwDestroyWindowNull(_PLFWwindow* window);
void _plfwSetWindowTitleNull(_PLFWwindow* window, const char* title);
void _plfwSetWindowIconNull(_PLFWwindow* window, int count, const PLFWimage* images);
void _plfwSetWindowMonitorNull(_PLFWwindow* window, _PLFWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate);
void _plfwGetWindowPosNull(_PLFWwindow* window, int* xpos, int* ypos);
void _plfwSetWindowPosNull(_PLFWwindow* window, int xpos, int ypos);
void _plfwGetWindowSizeNull(_PLFWwindow* window, int* width, int* height);
void _plfwSetWindowSizeNull(_PLFWwindow* window, int width, int height);
void _plfwSetWindowSizeLimitsNull(_PLFWwindow* window, int minwidth, int minheight, int maxwidth, int maxheight);
void _plfwSetWindowAspectRatioNull(_PLFWwindow* window, int n, int d);
void _plfwGetFramebufferSizeNull(_PLFWwindow* window, int* width, int* height);
void _plfwGetWindowFrameSizeNull(_PLFWwindow* window, int* left, int* top, int* right, int* bottom);
void _plfwGetWindowContentScaleNull(_PLFWwindow* window, float* xscale, float* yscale);
void _plfwIconifyWindowNull(_PLFWwindow* window);
void _plfwRestoreWindowNull(_PLFWwindow* window);
void _plfwMaximizeWindowNull(_PLFWwindow* window);
PLFWbool _plfwWindowMaximizedNull(_PLFWwindow* window);
PLFWbool _plfwWindowHoveredNull(_PLFWwindow* window);
void _plfwSetWindowResizableNull(_PLFWwindow* window, PLFWbool enabled);
void _plfwSetWindowDecoratedNull(_PLFWwindow* window, PLFWbool enabled);
void _plfwSetWindowFloatingNull(_PLFWwindow* window, PLFWbool enabled);
void _plfwSetWindowMousePassthroughNull(_PLFWwindow* window, PLFWbool enabled);
float _plfwGetWindowOpacityNull(_PLFWwindow* window);
void _plfwSetWindowOpacityNull(_PLFWwindow* window, float opacity);
void _plfwSetRawMouseMotionNull(_PLFWwindow *window, PLFWbool enabled);
PLFWbool _plfwRawMouseMotionSupportedNull(void);
void _plfwShowWindowNull(_PLFWwindow* window);
void _plfwRequestWindowAttentionNull(_PLFWwindow* window);
void _plfwHideWindowNull(_PLFWwindow* window);
void _plfwFocusWindowNull(_PLFWwindow* window);
PLFWbool _plfwWindowFocusedNull(_PLFWwindow* window);
PLFWbool _plfwWindowIconifiedNull(_PLFWwindow* window);
PLFWbool _plfwWindowVisibleNull(_PLFWwindow* window);
void _plfwPollEventsNull(void);
void _plfwWaitEventsNull(void);
void _plfwWaitEventsTimeoutNull(double timeout);
void _plfwPostEmptyEventNull(void);
void _plfwGetCursorPosNull(_PLFWwindow* window, double* xpos, double* ypos);
void _plfwSetCursorPosNull(_PLFWwindow* window, double x, double y);
void _plfwSetCursorModeNull(_PLFWwindow* window, int mode);
PLFWbool _plfwCreateCursorNull(_PLFWcursor* cursor, const PLFWimage* image, int xhot, int yhot);
PLFWbool _plfwCreateStandardCursorNull(_PLFWcursor* cursor, int shape);
void _plfwDestroyCursorNull(_PLFWcursor* cursor);
void _plfwSetCursorNull(_PLFWwindow* window, _PLFWcursor* cursor);
void _plfwSetClipboardStringNull(const char* string);
const char* _plfwGetClipboardStringNull(void);
const char* _plfwGetScancodeNameNull(int scancode);
int _plfwGetKeyScancodeNull(int key);

void _plfwPollMonitorsNull(void);

