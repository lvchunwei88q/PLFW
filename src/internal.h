//========================================================================
// PLFW 3.5 - windowing-only fork of GLFW (www.glfw.org)
//------------------------------------------------------------------------
// Copyright (c) 2002-2006 Marcus Geelnard
// Copyright (c) 2006-2019 Camilla Löwy <elmindreda@glfw.org>
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

#pragma once

#if defined(_PLFW_USE_CONFIG_H)
 #include "plfw_config.h"
#endif

#define PLFW_INCLUDE_NONE
#include "../include/PLFW/plfw3.h"

#include <stdbool.h>

#define _PLFW_INSERT_FIRST      0
#define _PLFW_INSERT_LAST       1

#define _PLFW_POLL_PRESENCE     0
#define _PLFW_POLL_AXES         1
#define _PLFW_POLL_BUTTONS      2
#define _PLFW_POLL_ALL          (_PLFW_POLL_AXES | _PLFW_POLL_BUTTONS)

#define _PLFW_MESSAGE_SIZE      1024

typedef int PLFWbool;
typedef void (*PLFWproc)(void);

typedef struct _PLFWerror       _PLFWerror;
typedef struct _PLFWinitconfig  _PLFWinitconfig;
typedef struct _PLFWwndconfig   _PLFWwndconfig;
typedef struct _PLFWfbconfig    _PLFWfbconfig;
typedef struct _PLFWwindow      _PLFWwindow;
typedef struct _PLFWplatform    _PLFWplatform;
typedef struct _PLFWlibrary     _PLFWlibrary;
typedef struct _PLFWmonitor     _PLFWmonitor;
typedef struct _PLFWcursor      _PLFWcursor;
typedef struct _PLFWmapelement  _PLFWmapelement;
typedef struct _PLFWmapping     _PLFWmapping;
typedef struct _PLFWjoystick    _PLFWjoystick;
typedef struct _PLFWtls         _PLFWtls;
typedef struct _PLFWmutex       _PLFWmutex;

#include "platform.h"

#define PLFW_NATIVE_INCLUDE_NONE
#include "../include/PLFW/plfw3native.h"

// Checks for whether the library has been initialized
#define _PLFW_REQUIRE_INIT()                         \
    if (!_plfw.initialized)                          \
    {                                                \
        _plfwInputError(PLFW_NOT_INITIALIZED, NULL); \
        return;                                      \
    }
#define _PLFW_REQUIRE_INIT_OR_RETURN(x)              \
    if (!_plfw.initialized)                          \
    {                                                \
        _plfwInputError(PLFW_NOT_INITIALIZED, NULL); \
        return x;                                    \
    }

// Swaps the provided pointers
#define _PLFW_SWAP(type, x, y) \
    {                          \
        type t;                \
        t = x;                 \
        x = y;                 \
        y = t;                 \
    }

// Per-thread error structure
//
struct _PLFWerror
{
    _PLFWerror*     next;
    int             code;
    char            description[_PLFW_MESSAGE_SIZE];
};

// Initialization configuration
//
// Parameters relating to the initialization of the library
//
struct _PLFWinitconfig
{
    bool          hatButtons;
    int           platformID;
    struct {
        int       libdecorMode;
    } wl;
};

// Window configuration
//
// Parameters relating to the creation of the window but not directly related
// to the framebuffer.  This is used to pass window creation parameters from
// shared code to the platform API.
//
struct _PLFWwndconfig
{
    int           xpos;
    int           ypos;
    int           width;
    int           height;
    bool          resizable;
    bool          visible;
    bool          decorated;
    bool          focused;
    bool          autoIconify;
    bool          floating;
    bool          maximized;
    bool          centerCursor;
    bool          focusOnShow;
    bool          mousePassthrough;
    bool          scaleToMonitor;
    bool          scaleFramebuffer;
    struct {
        bool      keymenu;
        bool      showDefault;
    } win32;
    struct {
        char      appId[256];
    } wl;
};

// Framebuffer configuration
//
// This describes buffers and their sizes.  It also contains
// a platform-specific ID used to map back to the backend API object.
//
// It is used to pass framebuffer parameters from shared code to the platform
// API and also to enumerate and select available framebuffer configs.
//
struct _PLFWfbconfig
{
    int         redBits;
    int         greenBits;
    int         blueBits;
    int         alphaBits;
    int         depthBits;
    int         stencilBits;
    int         accumRedBits;
    int         accumGreenBits;
    int         accumBlueBits;
    int         accumAlphaBits;
    int         auxBuffers;
    bool        stereo;
    int         samples;
    bool        sRGB;
    bool        doublebuffer;
    bool        transparent;
    uintptr_t   handle;
};

// Window structure
//
struct _PLFWwindow
{
    struct _PLFWwindow* next;

    // Window settings and state
    PLFWbool            resizable;
    PLFWbool            decorated;
    PLFWbool            autoIconify;
    PLFWbool            floating;
    PLFWbool            focusOnShow;
    PLFWbool            mousePassthrough;
    PLFWbool            shouldClose;
    void*               userPointer;
    PLFWvidmode         videoMode;
    _PLFWmonitor*       monitor;
    _PLFWcursor*        cursor;
    char*               title;

    int                 minwidth, minheight;
    int                 maxwidth, maxheight;
    int                 numer, denom;

    PLFWbool            stickyKeys;
    PLFWbool            stickyMouseButtons;
    PLFWbool            lockKeyMods;
    PLFWbool            disableMouseButtonLimit;
    int                 cursorMode;
    char                mouseButtons[PLFW_MOUSE_BUTTON_LAST + 1];
    char                keys[PLFW_KEY_LAST + 1];
    // Virtual cursor position when cursor is disabled
    double              virtualCursorPosX, virtualCursorPosY;
    PLFWbool            rawMouseMotion;

    struct {
        PLFWwindowposfun          pos;
        PLFWwindowsizefun         size;
        PLFWwindowclosefun        close;
        PLFWwindowrefreshfun      refresh;
        PLFWwindowfocusfun        focus;
        PLFWwindowiconifyfun      iconify;
        PLFWwindowmaximizefun     maximize;
        PLFWframebuffersizefun    fbsize;
        PLFWwindowcontentscalefun scale;
        PLFWmousebuttonfun        mouseButton;
        PLFWcursorposfun          cursorPos;
        PLFWcursorenterfun        cursorEnter;
        PLFWscrollfun             scroll;
        PLFWkeyfun                key;
        PLFWcharfun               character;
        PLFWcharmodsfun           charmods;
        PLFWdropfun               drop;
    } callbacks;

    // This is defined in platform.h
    PLFW_PLATFORM_WINDOW_STATE
};

// Monitor structure
//
struct _PLFWmonitor
{
    char            name[128];
    void*           userPointer;

    // Physical dimensions in millimeters.
    int             widthMM, heightMM;

    // The window whose video mode is current on this monitor
    _PLFWwindow*    window;

    PLFWvidmode*    modes;
    int             modeCount;
    PLFWvidmode     currentMode;

    PLFWgammaramp   originalRamp;
    PLFWgammaramp   currentRamp;

    // This is defined in platform.h
    PLFW_PLATFORM_MONITOR_STATE
};

// Cursor structure
//
struct _PLFWcursor
{
    _PLFWcursor*    next;
    // This is defined in platform.h
    PLFW_PLATFORM_CURSOR_STATE
};

// Gamepad mapping element structure
//
struct _PLFWmapelement
{
    uint8_t         type;
    uint8_t         index;
    int8_t          axisScale;
    int8_t          axisOffset;
};

// Gamepad mapping structure
//
struct _PLFWmapping
{
    char            name[128];
    char            guid[33];
    _PLFWmapelement buttons[15];
    _PLFWmapelement axes[6];
};

// Joystick structure
//
struct _PLFWjoystick
{
    PLFWbool        allocated;
    PLFWbool        connected;
    float*          axes;
    int             axisCount;
    unsigned char*  buttons;
    int             buttonCount;
    unsigned char*  hats;
    int             hatCount;
    char            name[128];
    void*           userPointer;
    char            guid[33];
    _PLFWmapping*   mapping;

    // This is defined in platform.h
    PLFW_PLATFORM_JOYSTICK_STATE
};

// Thread local storage structure
//
struct _PLFWtls
{
    // This is defined in platform.h
    PLFW_PLATFORM_TLS_STATE
};

// Mutex structure
//
struct _PLFWmutex
{
    // This is defined in platform.h
    PLFW_PLATFORM_MUTEX_STATE
};

// Platform API structure
//
struct _PLFWplatform
{
    int platformID;
    // init
    PLFWbool (*init)(void);
    void (*terminate)(void);
    // input
    void (*getCursorPos)(_PLFWwindow*,double*,double*);
    void (*setCursorPos)(_PLFWwindow*,double,double);
    void (*setCursorMode)(_PLFWwindow*,int);
    void (*setRawMouseMotion)(_PLFWwindow*,PLFWbool);
    PLFWbool (*rawMouseMotionSupported)(void);
    PLFWbool (*createCursor)(_PLFWcursor*,const PLFWimage*,int,int);
    PLFWbool (*createStandardCursor)(_PLFWcursor*,int);
    void (*destroyCursor)(_PLFWcursor*);
    void (*setCursor)(_PLFWwindow*,_PLFWcursor*);
    const char* (*getScancodeName)(int);
    int (*getKeyScancode)(int);
    void (*setClipboardString)(const char*);
    const char* (*getClipboardString)(void);
    PLFWbool (*initJoysticks)(void);
    void (*terminateJoysticks)(void);
    PLFWbool (*pollJoystick)(_PLFWjoystick*,int);
    const char* (*getMappingName)(void);
    void (*updateGamepadGUID)(char*);
    // monitor
    void (*freeMonitor)(_PLFWmonitor*);
    void (*getMonitorPos)(_PLFWmonitor*,int*,int*);
    void (*getMonitorContentScale)(_PLFWmonitor*,float*,float*);
    void (*getMonitorWorkarea)(_PLFWmonitor*,int*,int*,int*,int*);
    PLFWvidmode* (*getVideoModes)(_PLFWmonitor*,int*);
    PLFWbool (*getVideoMode)(_PLFWmonitor*,PLFWvidmode*);
    PLFWbool (*getGammaRamp)(_PLFWmonitor*,PLFWgammaramp*);
    void (*setGammaRamp)(_PLFWmonitor*,const PLFWgammaramp*);
    // window
    PLFWbool (*createWindow)(_PLFWwindow*,const _PLFWwndconfig*,const _PLFWfbconfig*);
    void (*destroyWindow)(_PLFWwindow*);
    void (*setWindowTitle)(_PLFWwindow*,const char*);
    void (*setWindowIcon)(_PLFWwindow*,int,const PLFWimage*);
    void (*getWindowPos)(_PLFWwindow*,int*,int*);
    void (*setWindowPos)(_PLFWwindow*,int,int);
    void (*getWindowSize)(_PLFWwindow*,int*,int*);
    void (*setWindowSize)(_PLFWwindow*,int,int);
    void (*setWindowSizeLimits)(_PLFWwindow*,int,int,int,int);
    void (*setWindowAspectRatio)(_PLFWwindow*,int,int);
    void (*getFramebufferSize)(_PLFWwindow*,int*,int*);
    void (*getWindowFrameSize)(_PLFWwindow*,int*,int*,int*,int*);
    void (*getWindowContentScale)(_PLFWwindow*,float*,float*);
    void (*iconifyWindow)(_PLFWwindow*);
    void (*restoreWindow)(_PLFWwindow*);
    void (*maximizeWindow)(_PLFWwindow*);
    void (*showWindow)(_PLFWwindow*);
    void (*hideWindow)(_PLFWwindow*);
    void (*requestWindowAttention)(_PLFWwindow*);
    void (*focusWindow)(_PLFWwindow*);
    void (*setWindowMonitor)(_PLFWwindow*,_PLFWmonitor*,int,int,int,int,int);
    PLFWbool (*windowFocused)(_PLFWwindow*);
    PLFWbool (*windowIconified)(_PLFWwindow*);
    PLFWbool (*windowVisible)(_PLFWwindow*);
    PLFWbool (*windowMaximized)(_PLFWwindow*);
    PLFWbool (*windowHovered)(_PLFWwindow*);
    PLFWbool (*framebufferTransparent)(_PLFWwindow*);
    float (*getWindowOpacity)(_PLFWwindow*);
    void (*setWindowResizable)(_PLFWwindow*,PLFWbool);
    void (*setWindowDecorated)(_PLFWwindow*,PLFWbool);
    void (*setWindowFloating)(_PLFWwindow*,PLFWbool);
    void (*setWindowOpacity)(_PLFWwindow*,float);
    void (*setWindowMousePassthrough)(_PLFWwindow*,PLFWbool);
    void (*pollEvents)(void);
    void (*waitEvents)(void);
    void (*waitEventsTimeout)(double);
    void (*postEmptyEvent)(void);
};

// Library global data
//
struct _PLFWlibrary
{
    PLFWbool            initialized;
    PLFWallocator       allocator;

    _PLFWplatform       platform;

    struct {
        _PLFWinitconfig init;
        _PLFWfbconfig   framebuffer;
        _PLFWwndconfig  window;
        int             refreshRate;
    } hints;

    _PLFWerror*         errorListHead;
    _PLFWcursor*        cursorListHead;
    _PLFWwindow*        windowListHead;

    _PLFWmonitor**      monitors;
    int                 monitorCount;

    PLFWbool            joysticksInitialized;
    _PLFWjoystick       joysticks[PLFW_JOYSTICK_LAST + 1];
    _PLFWmapping*       mappings;
    int                 mappingCount;

    _PLFWtls            errorSlot;
    _PLFWmutex          errorLock;

    struct {
        uint64_t        offset;
        // This is defined in platform.h
        PLFW_PLATFORM_LIBRARY_TIMER_STATE
    } timer;

    struct {
        PLFWmonitorfun  monitor;
        PLFWjoystickfun joystick;
    } callbacks;

    // These are defined in platform.h
    PLFW_PLATFORM_LIBRARY_WINDOW_STATE
    PLFW_PLATFORM_LIBRARY_JOYSTICK_STATE
};

// Global state shared between compilation units of PLFW
//
extern _PLFWlibrary _plfw;


//////////////////////////////////////////////////////////////////////////
//////                       PLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

void _plfwPlatformInitTimer(void);
uint64_t _plfwPlatformGetTimerValue(void);
uint64_t _plfwPlatformGetTimerFrequency(void);

PLFWbool _plfwPlatformCreateTls(_PLFWtls* tls);
void _plfwPlatformDestroyTls(_PLFWtls* tls);
void* _plfwPlatformGetTls(_PLFWtls* tls);
void _plfwPlatformSetTls(_PLFWtls* tls, void* value);

PLFWbool _plfwPlatformCreateMutex(_PLFWmutex* mutex);
void _plfwPlatformDestroyMutex(_PLFWmutex* mutex);
void _plfwPlatformLockMutex(_PLFWmutex* mutex);
void _plfwPlatformUnlockMutex(_PLFWmutex* mutex);

void* _plfwPlatformLoadModule(const char* path);
void _plfwPlatformFreeModule(void* module);
PLFWproc _plfwPlatformGetModuleSymbol(void* module, const char* name);


//////////////////////////////////////////////////////////////////////////
//////                         PLFW event API                       //////
//////////////////////////////////////////////////////////////////////////

void _plfwInputWindowFocus(_PLFWwindow* window, PLFWbool focused);
void _plfwInputWindowPos(_PLFWwindow* window, int xpos, int ypos);
void _plfwInputWindowSize(_PLFWwindow* window, int width, int height);
void _plfwInputFramebufferSize(_PLFWwindow* window, int width, int height);
void _plfwInputWindowContentScale(_PLFWwindow* window,
                                  float xscale, float yscale);
void _plfwInputWindowIconify(_PLFWwindow* window, PLFWbool iconified);
void _plfwInputWindowMaximize(_PLFWwindow* window, PLFWbool maximized);
void _plfwInputWindowDamage(_PLFWwindow* window);
void _plfwInputWindowCloseRequest(_PLFWwindow* window);
void _plfwInputWindowMonitor(_PLFWwindow* window, _PLFWmonitor* monitor);

void _plfwInputKey(_PLFWwindow* window,
                   int key, int scancode, int action, int mods);
void _plfwInputChar(_PLFWwindow* window,
                    uint32_t codepoint, int mods, PLFWbool plain);
void _plfwInputScroll(_PLFWwindow* window, double xoffset, double yoffset);
void _plfwInputMouseClick(_PLFWwindow* window, int button, int action, int mods);
void _plfwInputCursorPos(_PLFWwindow* window, double xpos, double ypos);
void _plfwInputCursorEnter(_PLFWwindow* window, PLFWbool entered);
void _plfwInputDrop(_PLFWwindow* window, int count, const char** names);
void _plfwInputJoystick(_PLFWjoystick* js, int event);
void _plfwInputJoystickAxis(_PLFWjoystick* js, int axis, float value);
void _plfwInputJoystickButton(_PLFWjoystick* js, int button, char value);
void _plfwInputJoystickHat(_PLFWjoystick* js, int hat, char value);

void _plfwInputMonitor(_PLFWmonitor* monitor, int action, int placement);
void _plfwInputMonitorWindow(_PLFWmonitor* monitor, _PLFWwindow* window);

#if defined(__GNUC__)
void _plfwInputError(int code, const char* format, ...)
    __attribute__((format(printf, 2, 3)));
#else
void _plfwInputError(int code, const char* format, ...);
#endif


//////////////////////////////////////////////////////////////////////////
//////                       PLFW internal API                      //////
//////////////////////////////////////////////////////////////////////////

PLFWbool _plfwSelectPlatform(int platformID, _PLFWplatform* platform);

PLFWbool _plfwStringInExtensionString(const char* string, const char* extensions);
const _PLFWfbconfig* _plfwChooseFBConfig(const _PLFWfbconfig* desired,
                                         const _PLFWfbconfig* alternatives,
                                         unsigned int count);
const PLFWvidmode* _plfwChooseVideoMode(_PLFWmonitor* monitor,
                                        const PLFWvidmode* desired);
int _plfwCompareVideoModes(const PLFWvidmode* first, const PLFWvidmode* second);
_PLFWmonitor* _plfwAllocMonitor(const char* name, int widthMM, int heightMM);
void _plfwFreeMonitor(_PLFWmonitor* monitor);
void _plfwAllocGammaArrays(PLFWgammaramp* ramp, unsigned int size);
void _plfwFreeGammaArrays(PLFWgammaramp* ramp);
void _plfwSplitBPP(int bpp, int* red, int* green, int* blue);

void _plfwInitGamepadMappings(void);
_PLFWjoystick* _plfwAllocJoystick(const char* name,
                                  const char* guid,
                                  int axisCount,
                                  int buttonCount,
                                  int hatCount);
void _plfwFreeJoystick(_PLFWjoystick* js);
void _plfwCenterCursorInContentArea(_PLFWwindow* window);

size_t _plfwEncodeUTF8(char* s, uint32_t codepoint);
char** _plfwParseUriList(char* text, int* count);

char* _plfw_strdup(const char* source);
int _plfw_min(int a, int b);
int _plfw_max(int a, int b);

void* _plfw_calloc(size_t count, size_t size);
void* _plfw_realloc(void* pointer, size_t size);
void _plfw_free(void* pointer);

