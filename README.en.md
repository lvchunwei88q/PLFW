# PLFW (window / input / event build)

[**English**](README.en.md) | [简体中文](README.md)

> **This repository is a modified fork of GLFW 3.5.1: all graphics initialization code
> has been removed, and the library only keeps the window system, input (keyboard /
> mouse / joystick) and event handling.**
>
> That is, `plfwCreateWindow` (upstream `glfwCreateWindow`) no longer creates any
> OpenGL, OpenGL ES, EGL, WGL, GLX, NSGL, OSMesa or Vulkan object; the upstream
> `glfwMakeContextCurrent` / `glfwSwapBuffers` / `glfwSwapInterval` /
> `glfwGetProcAddress` / `glfwExtensionSupported` / `glfwGetWindowSurface` APIs are
> deleted as a whole and this library provides no `plfw*` counterpart. How to draw
> inside a window is entirely up to the application (D3D11/12, Vulkan, GDI, child
> controls, ...).
>
> Behaviourally this is upstream GLFW permanently running in `GLFW_NO_API` mode.
> What follows describes the port and the measured results.

## What changed

### Capabilities kept

| Area | Contents |
| --- | --- |
| Window | create / destroy, title, icon, position and size, size limits, aspect ratio, iconify, maximize, restore, show / hide, focus, always-on-top, decorations, whole-window opacity, mouse passthrough, content scale, `PLFW_SCALE_TO_MONITOR` / `PLFW_SCALE_FRAMEBUFFER`, full screen (`plfwSetWindowMonitor`), `PLFW_REFRESH_RATE` hint |
| Input | keyboard (keys, scan codes, key names, character callback, sticky / lock keys), mouse (buttons, cursor position, cursor mode, raw motion, scroll), joystick / gamepad (hot-plug, axes, buttons, hats, mappings, user mapping file) |
| Events | `plfwPollEvents`, `plfwWaitEvents`, `plfwWaitEventsTimeout`, `plfwPostEmptyEvent`, all window / input / monitor / joystick callbacks |
| Monitor | enumeration, name, position, work area, content scale, video modes, gamma ramp |
| Other | clipboard, cursor shapes, timers, error callback, thread TLS / mutex, platform selection |

### What was removed

- Source files deleted: `src/context.c`, `src/wgl_context.c`, `src/glx_context.c`,
  `src/egl_context.c`, `src/osmesa_context.c`, `src/nsgl_context.m`, `src/vulkan.c`.
- Public header (`include/PLFW/plfw3.h`): the `GL` / `GLES` / `EGL` / `Vulkan` include
  chain, `GLFWglproc` / `GLFWvkproc`, and all context and Vulkan APIs —
  `glfwGetProcAddress`, `glfwExtensionSupported`, `glfwMakeContextCurrent`,
  `glfwGetCurrentContext`, `glfwSwapBuffers`, `glfwSwapInterval`, `glfwInitVulkanLoader`,
  `glfwGetWindowSurface`. Context-related hints and attributes are gone too
  (`GLFW_CLIENT_API`, `GLFW_CONTEXT_VERSION_*`, `GLFW_OPENGL_*`, `GLFW_CONTEXT_*`,
  `GLFW_SAMPLES`, `GLFW_SRGB_CAPABLE`, `GLFW_DOUBLEBUFFER`, the colour-bit hints,
  `GLFW_COCOA_GRAPHICS_SWITCHING`).
- `include/PLFW/plfw3native.h`: `glfwGetWGLContext`, `glfwGetNSGLContext`,
  `glfwGetGLXContext`, `glfwGetGLXWindow`, `glfwGetEGLContext`, `glfwGetEGLSurface`,
  `glfwGetEGLConfig`, `glfwGetOSMesaColorBuffer`, `glfwGetOSMesaDepthBuffer`,
  `glfwGetOSMesaContext`, plus the EGL / WGL / NSGL / GLX / OSMesa branches of
  `PLFW_EXPOSE_NATIVE_*`. The native handle accessors `plfwGetWin32Window`,
  `plfwGetWin32Adapter`, `plfwGetWin32Monitor`, `plfwGetWaylandDisplay`,
  `plfwGetWaylandMonitor` and `plfwGetWaylandWindow` are kept (the Cocoa / X11 accessors
  went with their backends, see below).
- Version query API: `glfwGetVersion`, `glfwGetVersionString` and the
  `GLFW_VERSION_MAJOR` / `GLFW_VERSION_MINOR` / `GLFW_VERSION_REVISION` macros are all
  deleted (see item 11 under "Semantic changes").
- Internal structures (`src/internal.h`): `_GLFWctxconfig`, `_GLFWcontext`,
  `window->context`, `window->doublebuffer`, the EGL / OSMesa / Vulkan function pointers
  in the platform table, `library.egl` / `library.osmesa` / `library.vk`, `hints.context`,
  and `library.contextSlot` (the TLS used to destroy the current context).
- Platform layer (context cleanup, covering the four backends that existed then):
  - Win32: WGL support (`wgl_context.c`, WGL constants / function pointers / structs,
    loading `opengl32.dll`) and the EGL / Vulkan surface helpers.
  - X11 (backend since deleted, next section): the GLX types, constants, function pointers
    and `libGL.so` macros in `x11_platform.h`, the Vulkan Xlib / Xcb surface types and
    `xcbVulkanSurface`, the `_GLFWcontextGLX` / `_GLFWlibraryGLX` structs.
  - Wayland: the Vulkan Wayland surface types, the `wl_egl_window` function pointers and
    macros in `wl_platform.h`, the `_GLFWwindowWayland.egl` / `_GLFWlibraryWayland.egl`
    sub-structs; `wl_init.c` no longer loads `libwayland-egl.so` and drops
    `_glfwTerminateEGL/OSMesa`; `wl_window.c` drops the `wl_egl_window_resize` branch,
    the EGL frame callbacks, `_glfwWaitForEGLFrameWayland` and the `egl.*` resource
    release on context creation and destruction.
  - Cocoa (backend since deleted, next section): the Vulkan macOS / Metal surface types,
    `_GLFWcontextNSGL` / `_GLFWlibraryNSGL`, the `NSOpenGL` enum shim and the
    `GL_SILENCE_DEPRECATION` block.

### Backend scope: Win32 and Wayland only

The X11 and Cocoa backends are **removed from the tree** (not disabled by a CMake option);
there is no X11 / Xlib / Cocoa / NSGL code left. Windows uses the Win32 backend, Linux uses
the Wayland backend, and both come with a Null / headless backend for automated testing
on systems without a window system.

- Source files deleted: `src/x11_init.c`, `src/x11_window.c`, `src/x11_monitor.c`,
  `src/x11_platform.h`, `src/xkb_unicode.c` (the X11-only keysym→Unicode table),
  `src/cocoa_init.m`, `src/cocoa_window.m`, `src/cocoa_monitor.m`,
  `src/cocoa_joystick.m`, `src/cocoa_platform.h`, `src/cocoa_joystick.h` — 11 files.
- `src/platform.h`: the `_PLFW_X11` / `_PLFW_COCOA` include blocks and four
  `PLFW_*_STATE` macros are gone; `PLFW_PLATFORM_*_STATE` now covers only Win32 /
  Wayland / Null; the Linux joystick backend (`linux_joystick.c`) and `posix_poll.c`
  are now enabled by `_PLFW_WAYLAND` alone (both are still needed there).
- `src/platform.c`: the `supportedPlatforms` table only lists Win32 / Wayland; the
  `XDG_SESSION_TYPE` probe block (needed when Wayland and X11 were both compiled) is
  deleted, so the table has the two entries (the old version string that lived next to
  it was removed later as well, see item 11 under "Semantic changes").
- `src/CMakeLists.txt` and the top-level `CMakeLists.txt`: the `GLFW_BUILD_X11` /
  `GLFW_BUILD_COCOA` `cmake_dependent_option` pairs, their "Including … support"
  reports, Cocoa's `enable_language(OBJC)` and `-framework Cocoa/IOKit/QuartzCore`
  link, the X11 source list, and the whole `find_package(X11 REQUIRED)` block with the
  XRandR / Xinerama / Xkb / Xcursor / XInput / XShape header checks are gone.
  **Configuration no longer probes any X11 dependency**: configuring with
  `-DCMAKE_DISABLE_FIND_PACKAGE_X11=TRUE` still succeeds, and CMake simply reports that
  the variable is not used by the project.
- Wayland's `pkg_check_modules` REQUIRED list no longer contains `wayland-egl`: the code
  stopped using `libwayland-egl`, so the runtime dependencies are
  `wayland-client` / `wayland-cursor` / `xkbcommon`.
- Public API narrowed accordingly:
  - Deleted hints: `GLFW_X11_CLASS_NAME`, `GLFW_X11_INSTANCE_NAME`,
    `GLFW_COCOA_RETINA_FRAMEBUFFER`, `GLFW_COCOA_FRAME_NAME`,
    `GLFW_COCOA_CHDIR_RESOURCES`, `GLFW_COCOA_MENUBAR` (without their backends they
    could only become silently ignored hints), together with the `hints.window.x11`,
    `hints.window.ns` and `hints.init.ns` sub-structs in `src/internal.h`.
  - `plfw3native.h` drops `GLFW_EXPOSE_NATIVE_COCOA` / `GLFW_EXPOSE_NATIVE_X11` and,
    with them, `glfwGetCocoaMonitor` / `glfwGetCocoaWindow` / `glfwGetCocoaView`,
    `glfwGetX11Display` / `glfwGetX11Adapter` / `glfwGetX11Monitor` /
    `glfwGetX11Window`, `glfwGetX11SelectionString` / `glfwSetX11SelectionString`.
    `PLFW_EXPOSE_NATIVE_WIN32` and `PLFW_EXPOSE_NATIVE_WAYLAND` are kept.
  - `PLFW_PLATFORM_X11` / `PLFW_PLATFORM_COCOA` **identifiers are deleted**: the
    platform IDs are `PLFW_ANY_PLATFORM`, `PLFW_PLATFORM_WIN32`, `PLFW_PLATFORM_WAYLAND`
    and `PLFW_PLATFORM_NULL`. Requesting a backend with their old numeric values is
    treated as an unknown value — `plfwInitHint` reports `PLFW_INVALID_ENUM` and
    `plfwPlatformSupported` returns `PLFW_FALSE`. A caller's
    `switch (plfwGetPlatform())` has to drop those two `case` labels.
  - The macOS joystick mappings guarded by `#if defined(_PLFW_COCOA)` in
    `src/mappings.h` are deleted with the macro.
- Examples and tests: upstream's 11 examples and 20 tests all depended on OpenGL or
  Vulkan and were deleted with the port (they can still be fetched from the first commit,
  e.g. `git show 3373d3b:examples/gears.c`). They are replaced by three
  graphics-free programs, all written for this fork:
  - `examples/window-input.c`: a single window printing every keyboard / character /
    mouse / scroll / focus / size / framebuffer / joystick event, with a status line
    once a second.
  - `examples/multi-window.c`: one event loop driving three windows, demonstrating user
    pointers, show / hide, iconify / restore, decoration toggling, native handles
    (HWND / wl_surface), which monitor a window is on, and cross-window clipboard.
  - `examples/hello-window.c`: the smallest usable single-window program — init,
    create, Escape to close, size callback, `plfwWaitEventsTimeout` loop; a template
    to copy from.
  - `tests/smoke.c`: a self-checking smoke test registered as the CTest case `smoke`
    (that executable can also be launched directly). It covers init, the active backend,
    timers, monitors and video modes, window creation without any graphics API, the
    ignored `share` parameter, window / framebuffer / content sizes, visibility / iconify
    / resizable attributes, cursor and keyboard / mouse state, window size and position
    setting, clipboard round-trip, joystick enumeration, scan codes, the event pump,
    close requests and destruction. It adapts to backend capabilities: without a display
    it falls back to the headless backend; the Null backend skips window geometry
    assertions; Wayland skips the window-position assertion (the protocol does not let
    clients place windows); assertions on the clipboard are skipped when the backend
    reports it unavailable, or when a Wayland window has no focus and therefore no
    clipboard data offer.
- Docs: `docs/` holds 5 Chinese guides (overview, window and monitor, input, build and
  linking, source tree and internals), all rewritten for the current capabilities and
  readable as plain Markdown; there is no Doxygen setup and no HTML is generated at
  build time.

### Semantic changes (read before migrating)

1. **The `share` parameter of `plfwCreateWindow` is ignored.** It is kept for source
   compatibility: passing something other than `NULL` neither fails nor shares anything.
   Pass `NULL`.
2. **The framebuffer bit-depth hints are gone**: `GLFW_RED_BITS` / `GLFW_GREEN_BITS` /
   `GLFW_BLUE_BITS` / `GLFW_ALPHA_BITS` / `GLFW_DEPTH_BITS` / `GLFW_STENCIL_BITS` can no
   longer be set; `_PLFWfbconfig` still uses 8/8/8 as the nominal value reported by
   `plfwGetVideoMode` and `plfwGetWindowAttrib`. Window / framebuffer scaling hints
   such as `PLFW_SCALE_TO_MONITOR` and `PLFW_SCALE_FRAMEBUFFER` still work.
3. **Transparent framebuffer support is removed entirely**: the
   `PLFW_TRANSPARENT_FRAMEBUFFER` hint and attribute, the transparency field in
   `_PLFWfbconfig`, the per-backend transparency branches and the whole Win32 path that
   loaded `dwmapi.dll` to enable DWM blur are deleted. Windows are always opaque, and
   passing that enum to `plfwWindowHint` reports `PLFW_INVALID_ENUM`; whole-window
   fading still works through `plfwSetWindowOpacity`.
4. **`plfwGetWindowAttrib` no longer returns `GLFW_DOUBLEBUFFER` or any context
   attribute** (`GLFW_CLIENT_API`, `GLFW_CONTEXT_CREATION_API`,
   `GLFW_CONTEXT_VERSION_*`, `GLFW_OPENGL_*`, ...).
5. **Three context / API error codes are kept but never produced**:
   `PLFW_NO_CURRENT_CONTEXT`, `PLFW_API_UNAVAILABLE`, `PLFW_VERSION_UNAVAILABLE`
   (source / ABI compatibility).
6. **Only Win32 and Wayland are left**: the X11 and Cocoa backends are removed from the
   tree (see "Backend scope" above), so Linux has Wayland + Null and Windows has
   Win32 + Null. The top-level `CMakeLists.txt` keeps only the `PLFW_BUILD_WIN32` /
   `PLFW_BUILD_WAYLAND` backend options; **macOS is not a build target** — configuring
   on `APPLE` fails with `PLFW does not support macOS`, and `macos_time.*` is deleted
   (Unix time always goes through POSIX). The toolchains are a hard constraint as well:
   **Windows means MSVC, Linux means GCC**; MinGW / Clang / clang-cl / MSYS2 / mingw-w64
   cross builds are rejected at configure time with `message(FATAL_ERROR)` under
   `CMake/toolchains/` (that directory is deleted).
7. **A Null backend is available** (`PLFW_PLATFORM_NULL`) for automated testing on
   systems without a window system; like the other two it only does windows and input
   and creates no graphics context.
8. **On Wayland the window attaches its own solid-colour `wl_shm` buffer.** A Wayland
   surface is only mapped by the compositor once a buffer has been committed; upstream
   got that buffer from the application's `glfwSwapBuffers`, but this library has no
   graphics context and never draws. Without that buffer the window would never appear
   on Wayland and would get neither pointer enter nor keyboard focus (and therefore no
   clipboard). So the window attaches a window-sized, always-opaque dark grey buffer
   whenever it becomes visible or is resized, which lets it show up, be clicked for
   focus and be resized.
9. **On Wayland the clipboard reports an error instead of crashing when there is no
   seat.** When the compositor provides no `wl_data_device_manager` or seat,
   `plfwSetClipboardString` / `plfwGetClipboardString` return `PLFW_PLATFORM_ERROR`
   (upstream dereferenced a null pointer).
10. **No `DISPLAY` fallback on Linux**: with only Wayland compiled, a `DISPLAY` pointing
    at an unusable X server has no effect — `plfwInit()` fails right at the Wayland
    connection stage (`PLFW_PLATFORM_UNAVAILABLE` / platform error). Use
    `PLFW_PLATFORM_NULL` explicitly when you need headless operation.
11. **No version query API**: `glfwGetVersion` / `glfwGetVersionString` and the three
    `PLFW_VERSION_*` macros are deleted, so the library no longer reports its own
    version; the version number only lives in `project(PLFW VERSION 3.5.1)` in
    `CMakeLists.txt` so the build system can tag the library, and the binary itself
    carries no version information.

### Build and verification

Windows builds with MSVC, Linux with GCC (other compilers are rejected at configure time).
On Windows run inside an "x64 Native Tools Command Prompt", or `call vcvars64.bat` first,
so that `cl.exe` and `rc.exe` are on `PATH`:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

:: self-check (single-config generator; add -C Release for the Visual Studio generator)
ctest --test-dir build
build\tests\smoke.exe

:: interactive demo: an 800x480 window printing key / char / mouse / scroll / focus / size / joystick events
build\examples\window-input.exe

:: minimal example: a 640x400 window, Escape or close to quit
build\examples\hello-window.exe

:: multi-window demo (three windows + native handles + cross-window clipboard)
build\examples\multi-window.exe
```

Linux has only the Wayland backend, so the build dependencies shrink to
`libwayland-dev` (providing `wayland-client` / `wayland-cursor`), `libxkbcommon-dev`,
`wayland-protocols` and `wayland-scanner` at build time. **No X11 development package is
needed** (`libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`,
`libxi-dev` can all stay uninstalled). On a system without a display,
`weston --backend=headless-backend.so` can provide the Wayland service.

```sh
# Linux (Wayland + Null)
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux

# self-check (build-linux/tests/smoke can also be run directly)
ctest --test-dir build-linux

# with a Wayland compositor
env XDG_RUNTIME_DIR=/tmp/xdg WAYLAND_DISPLAY=wl-test ./build-linux/tests/smoke

# with no display service at all (the test retries with PLFW_PLATFORM_NULL internally)
env -u DISPLAY -u WAYLAND_DISPLAY ./build-linux/tests/smoke
```

#### Measured results

MSVC 19.51 (VS 2026 Community, 14.51.36231) + CMake 4.3.2 + Ninja (Windows):

- library and all three programs compile with no warnings;
- `tests/smoke.exe`: every check ok, including window creation without any graphics API,
  window / framebuffer sizes, position and size setting, window attributes, cursor
  position, keyboard and mouse state queries, clipboard round-trip, monitors and video
  modes, joystick enumeration, 30 event-pump wake-ups, close request and destruction, plus
  the two "the X11 / Cocoa backends are not in this build" assertions;
  `active platform backend: Win32`;
- `examples/window-input.exe`: an 800x480 window appears (its handle and title are reachable
  through `FindWindow` / `WM_CLOSE`), the event loop is stable (46 frames in the first
  second, 83 after two), and it exits cleanly on `WM_CLOSE`;
- `examples/multi-window.exe`: three windows exist at once (`HWND 0x370650` / `0x610688` /
  `0x234080A`), each reports its monitor as "Generic PnP Monitor (1 of 1)", window 2 reads
  back the clipboard written by window 1, and it exits cleanly after the close request.

GCC 11.4.0 + CMake 3.22.1 + Ninja (Ubuntu 22.04, the Wayland-only version after dropping
X11):

- configure prints only `-- Including Wayland support`, with no X11 probing at all;
  `pkg_check_modules` looks for `wayland-client` / `wayland-cursor` / `xkbcommon` only;
  an extra configure with `-DCMAKE_DISABLE_FIND_PACKAGE_X11=TRUE` also succeeds (CMake
  reports the variable as unused by the project, i.e. there is no `find_package(X11)`);
- build: library, both examples and the test compile with `warning:` / `error:` counts of
  zero;
- `env -u DISPLAY -u WAYLAND_DISPLAY ./tests/smoke` (no display): falls back to
  `PLFW_PLATFORM_NULL` and succeeds (`active platform backend: Null`), every check ok;
- `tests/smoke` under `weston --backend=headless-backend.so --socket=wl-test`:
  `active platform backend: Wayland`, all ok apart from two assertions skipped by backend
  capability — window positioning (the Wayland protocol does not let clients place
  windows) and the clipboard (that compositor has no seat, so it reports
  `PLFW_PLATFORM_ERROR` as expected);
- `window-input` and `multi-window` both run their full 5 s under the same headless
  compositor: the former prints its 800x480 window and per-second status, the latter
  prints three `wl_surface` handles (`0x5624ac444e90` / `0x5624ac45cca0` /
  `0x5624ac45d840`);
- `ldd tests/smoke` shows dependencies on `libc` / `libm` only, with no `libX11` or
  other X11 library; the Wayland client library is `dlopen`ed at runtime, as upstream did.

Ubuntu 26.04 LTS + WSL2 / WSLg (GCC 15.2.0 + CMake 4.2.3 + Ninja 1.13.2):

- configure prints only `-- Including Wayland support` and finds `wayland-client` /
  `wayland-cursor` 1.24.0 and `xkbcommon` 1.13.1; library, both examples and the test
  compile with zero `warning` / `error` lines;
- `tests/smoke` straight on the WSLg compositor: `active platform backend: Wayland`,
  all ok apart from two capability-skipped assertions — window positioning (Wayland does
  not let clients place windows) and the clipboard (the smoke test deliberately uses a
  hidden window, `PLFW_VISIBLE=FALSE`, which is never mapped and therefore gets no
  selection data offer, so it reports
  `PLFW_FORMAT_UNAVAILABLE: Wayland: No clipboard data available` as expected);
- `examples/multi-window` on the same compositor: the three windows each get a
  `wl_surface` handle, report the `"weston rdp"` monitor, take focus in creation order
  (`[first] focused` → `[second] focused` → `[third] focused`), and
  `clipboard through window 2` successfully reads back the string written by window 1 —
  clipboard round-trips work for visible windows on Wayland;
- `ldd build/tests/smoke` lists only `libm` / `libc`: the Wayland client library is
  `dlopen`ed at runtime and the static library carries no X11 / Wayland link dependency;
- `BUILD_SHARED_LIBS=ON` produces `libplfw.so.3.5` (SONAME `libplfw.so.3`),
  `nm -D --defined-only` shows 113 exported `plfw*` symbols and 0 `glfw*` ones;
- the pure Null build with `-DPLFW_BUILD_WAYLAND=OFF` passes as well, and `tests/smoke`
  reports `active platform backend: Null`.

X11 and Cocoa: **removed from this repository**, so no X server environment is needed
and no backend exists that has never been compiled and verified. macOS is no longer
supported either: the Cocoa backend and `macos_time.*` are deleted, and `CMakeLists.txt`
raises `FATAL_ERROR` on `APPLE`.

### Upstream content that no longer holds

- Statements in the upstream README such as "for use with OpenGL / OpenGL ES / Vulkan
  applications", "creating windows, contexts and surfaces" and "On Linux both Wayland and
  X11 are supported" are upstream text and no longer accurate for this fork, which keeps
  only the Win32 and Wayland backends.
- `docs/` has been rewritten for the current state and merged twice: `quick.md` went into
  the overview, `main.md` into `intro.md`, `SUPPORT.md` into this README, and one round's
  code review report `review.md` was deleted; `moving.md`, `news.md` and
  `CONTRIBUTING.md` are gone as well. The latest round merged `monitor.md` into
  `window.md` and `internal.md` + `compat.md` into the new `build.md` (formerly
  `compile.md`), taking the docs from 7 to 4; afterwards the "source tree and internals"
  chapter at the end of `build.md` was split back into a separate `internal.md`, because
  it is written for people modifying the library rather than using it — 5 guides in total.
- `deps/` now holds **only the 9 protocol XML files under `deps/wayland/`**
  (`wayland.xml`, `viewporter.xml`, `xdg-shell.xml`, `idle-inhibit-unstable-v1.xml`,
  `pointer-constraints-unstable-v1.xml`, `relative-pointer-unstable-v1.xml`,
  `fractional-scale-v1.xml`, `xdg-activation-v1.xml`, `xdg-decoration-unstable-v1.xml`).
  `wayland-scanner` reads them at build time to generate `-client-protocol.h` /
  `-client-protocol-code.h`, so **they must stay**. The other third-party dependencies
  formerly used by the upstream GL examples (`deps/glad`, `deps/nuklear.h`,
  `deps/nuklear_plfw_gl2.h`, `deps/stb_image_write.h`, `deps/linmath.h`,
  `deps/tinycthread.*`, `deps/getopt.*`, 43,949 lines in total) are deleted — with the
  graphics layer and the GL examples gone, nothing in the sources, examples, tests or
  CMake referenced them.

## License

PLFW is distributed under the zlib-style licence in [LICENSE.md](LICENSE.md), which keeps
the upstream copyright notices of Marcus Geelnard and Camilla Löwy next to the fork's own.
