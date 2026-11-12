# GLFW（窗口 / 输入 / 事件 裁剪版）

> **本仓库是 GLFW 3.5.1 的一个改造分支：所有图形初始化代码已被移除，库只保留
> 窗口系统、输入（键盘 / 鼠标 / 手柄）与事件处理能力。**
>
> 也就是说：`glfwCreateWindow` 不再创建 OpenGL、OpenGL ES、EGL、WGL、GLX、NSGL、
> OSMesa 或 Vulkan 相关对象，`glfwMakeContextCurrent` / `glfwSwapBuffers` /
> `glfwSwapInterval` / `glfwGetProcAddress` / `glfwExtensionSupported` /
> `glfwGetWindowSurface` 等 API 已从公共头文件中删除。怎么在窗口里画东西完全由
> 应用自己决定（D3D11/12、Vulkan、GDI、子窗口控件……）。
>
> 行为上等价于让库永久运行在 GLFW 的 `GLFW_NO_API` 模式。下文是改造说明；再往下
> 是上游 GLFW 的原始 README 文本（除标注处外未改动）。

## 改造说明

### 保留的能力

| 类别 | 内容 |
| --- | --- |
| 窗口 | 创建 / 销毁、标题、图标、位置与尺寸、尺寸限制、宽高比、图标化、最大化、还原、显示 / 隐藏、焦点、置顶、装饰、透明度、鼠标穿透、内容缩放、`GLFW_SCALE_TO_MONITOR` / `GLFW_SCALE_FRAMEBUFFER`、`GLFW_TRANSPARENT_FRAMEBUFFER`、全屏切换（`glfwSetWindowMonitor`）、`GLFW_REFRESH_RATE` 提示 |
| 输入 | 键盘（键、扫描码、键名、字符回调、粘滞键 / 锁定键）、鼠标（按键、光标位置、光标模式、原始鼠标运动、滚轮）、手柄 / 游戏杆（连接热插拔、轴、按键、帽、映射、用户映射文件） |
| 事件 | `glfwPollEvents`、`glfwWaitEvents`、`glfwWaitEventsTimeout`、`glfwPostEmptyEvent`、全部窗口 / 输入 / 显示器 / 手柄回调 |
| 显示器 | 枚举、名称、位置、工作区、内容缩放、视频模式、gamma ramp |
| 其他 | 剪贴板、光标形状、定时器、错误回调、线程相关的 TLS / 互斥量、`glfwGetVersionString`、平台选择 |

### 被移除的东西

- 源文件删除：`src/context.c`、`src/wgl_context.c`、`src/glx_context.c`、
  `src/egl_context.c`、`src/osmesa_context.c`、`src/nsgl_context.m`、`src/vulkan.c`。
- 公共头文件（`include/GLFW/glfw3.h`）删除：`GL`/`GLES`/`EGL`/`Vulkan` 头文件引入链，
  `GLFWglproc` / `GLFWvkproc`，`glfwGetProcAddress`、`glfwExtensionSupported`、
  `glfwMakeContextCurrent`、`glfwGetCurrentContext`、`glfwSwapBuffers`、
  `glfwSwapInterval`、`glfwInitVulkanLoader`、`glfwGetWindowSurface` 等全部上下文与
  Vulkan API；上下文相关的提示与属性（`GLFW_CLIENT_API`、`GLFW_CONTEXT_VERSION_*`、
  `GLFW_OPENGL_*`、`GLFW_CONTEXT_*`、`GLFW_SAMPLES`、`GLFW_SRGB_CAPABLE`、
  `GLFW_DOUBLEBUFFER`、各色深位提示、`GLFW_COCOA_GRAPHICS_SWITCHING`）。
- `include/GLFW/glfw3native.h` 删除：`glfwGetWGLContext`、`glfwGetNSGLContext`、
  `glfwGetGLXContext`、`glfwGetGLXWindow`、`glfwGetEGLContext`、`glfwGetEGLSurface`、
  `glfwGetEGLConfig`、`glfwGetOSMesaColorBuffer`、`glfwGetOSMesaDepthBuffer`、
  `glfwGetOSMesaContext` 以及 `GLFW_EXPOSE_NATIVE_*` 中的 EGL / WGL / NSGL / GLX / OSMesa
  分支。保留 `glfwGetWin32Window`、`glfwGetWin32Adapter`、`glfwGetWin32Monitor`、
  `glfwGetCocoaWindow`、`glfwGetX11Window`、`glfwGetX11Display`、`glfwGetX11Adapter`、
  `glfwGetX11Monitor` 等原生窗口句柄访问器。
- 内部结构（`src/internal.h`）：`_GLFWctxconfig`、`_GLFWcontext`、`window->context`、
  `window->doublebuffer`、平台表中的 EGL / OSMesa / Vulkan 函数指针、
  `library.egl` / `library.osmesa` / `library.vk`、`hints.context`、
  `library.contextSlot`（上下文销毁用的 TLS）。
- 平台层：Win32 的 WGL 支持（`wgl_context.c`、WGL 常量 / 函数指针 / 结构体、
  `opengl32.dll` 加载）与 EGL / Vulkan 表面辅助函数、null 后端的 OSMesa / EGL /
  Vulkan 分支。
- 示例与测试：原有 11 个示例、20 个测试全部依赖 OpenGL 或 Vulkan，已随改造删除
  （仍可从首次提交取回：`git show 3373d3b:examples/gears.c`）。替换为两个无图形
  依赖的程序：`examples/window-input.exe`（窗口 + 输入 + 事件演示）与
  `tests/smoke.exe`（自检式冒烟测试）。
- 文档：`docs/context.md`、`docs/vulkan.md` 已删除；`docs/` 下其余页面保留上游
  原文，其中涉及“创建上下文”的段落已经失效。

### 语义变化（迁移时注意）

1. **`glfwCreateWindow` 的 `share` 参数被忽略**。为保持源码兼容性保留了该形参，
   传入非 `NULL` 不会报错，也不会共享任何东西；建议直接传 `NULL`。
2. **帧缓冲位深提示被移除**，`GLFW_RED_BITS` / `GLFW_GREEN_BITS` / `GLFW_BLUE_BITS` /
   `GLFW_ALPHA_BITS` / `GLFW_DEPTH_BITS` / `GLFW_STENCIL_BITS` 不再可设置；
   `_GLFWfbconfig` 内部仍以 8/8/8 作为 `glfwGetVideoMode` 与 `glfwGetWindowAttrib`
   名义值使用。`GLFW_TRANSPARENT_FRAMEBUFFER`、`GLFW_SCALE_TO_MONITOR`、
   `GLFW_SCALE_FRAMEBUFFER`、`GLFW_COCOA_RETINA_FRAMEBUFFER` 等窗口/帧缓冲缩放类提示
   仍然有效。
3. **`glfwGetWindowAttrib` 不再返回 `GLFW_DOUBLEBUFFER` 和任何上下文属性**
   （`GLFW_CLIENT_API`、`GLFW_CONTEXT_CREATION_API`、`GLFW_CONTEXT_VERSION_*`、
   `GLFW_OPENGL_*` 等）。
4. **`GLFW_NO_CURRENT_CONTEXT` 错误码保留但不再产生**（源码/ABI 兼容）。
5. **平台范围只有 Win32**。本改造只适配并验证了 Win32（MinGW-w64 + CMake + Ninja）。
   X11 / Wayland / Cocoa 的源码仍留在 `src/` 中，但它们调用的上下文层已被删除，
   因此顶层 `CMakeLists.txt` 在非 Windows 平台会直接 `FATAL_ERROR` 拒绝配置，
   而不是留到链接阶段报错。要恢复这些平台，需要重新接回上下文实现。
6. **null 后端可用**（`GLFW_PLATFORM_NULL`），用于无窗口系统的自动化测试；
   其版本串同样只列 “window/input only”。

### 构建与验证

```sh
# 配置（需要 CMake 3.16+、Ninja 与 C99 编译器）
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=gcc.exe \
      -DGLFW_BUILD_DOCS=OFF -DGLFW_INSTALL=OFF

# 编译：静态库 + 示例 + 冒烟测试
cmake --build build

# 运行验证（自检式，失败时返回非 0）
./build/tests/smoke.exe

# 交互演示：800x480 窗口，打印按键 / 字符 / 鼠标 / 滚轮 / 焦点 / 尺寸 / 手柄事件
./build/examples/window-input.exe
```

本机实测（MinGW-w64 GCC 15.2.0 UCRT + CMake 4.3.2 + Ninja）：

- 库与程序全部编译通过，无警告（`-std=c99 -Wall`）；
- `tests/smoke.exe`：31 项检查全部 ok，含窗口创建（无图形上下文）、窗口/帧缓冲尺寸、
  位置与尺寸设置、窗口属性、光标位置、键鼠状态查询、剪贴板往返、显示器与视频模式、
  手柄枚举、事件泵 30 次唤醒、关闭请求与销毁；
- `examples/window-input.exe`：桌面出现 800x480 窗口（句柄与标题可被 `FindWindow`
  / `WM_CLOSE` 命中），事件循环稳定（1 秒 46 帧、2 秒 83 帧），光标坐标回报正确，
  显示器枚举 “Generic PnP Monitor 1920x1080 @ 60 Hz”，收到 `WM_CLOSE` 后正常退出。

### 已知不再成立的上游内容

- 上游 README 中“为 OpenGL / OpenGL ES / Vulkan 应用开发”“creating windows,
  contexts and surfaces”等描述属于上游原文，对本分支不再准确。
- `docs/quick.md`、`docs/intro.md`、`docs/window.md`、`docs/moving.md`、
  `docs/compat.md` 中关于上下文创建、`glfwGetProcAddress`、Vulkan 表面创建、
  `glfwSwapBuffers` 的段落均不适用；`docs/internal.md` 中提到的
  `_glfwCreateContext*` 系列函数已不存在。
- `deps/glad`（OpenGL / Vulkan 加载器头文件）与 `deps/` 下的 getopt、TinyCThread、
  linmath、Nuklear、stb 等示例依赖仍留在树中，但当前的示例与测试都不再使用它们。

---

# 以下为上游 GLFW README 原文

# GLFW

[![Build status](https://github.com/glfw/glfw/actions/workflows/build.yml/badge.svg)](https://github.com/glfw/glfw/actions)
[![Build status](https://ci.appveyor.com/api/projects/status/0kf0ct9831i5l6sp/branch/master?svg=true)](https://ci.appveyor.com/project/elmindreda/glfw)

## Introduction

GLFW is an Open Source, multi-platform library for OpenGL, OpenGL ES and Vulkan
application development.  It provides a simple, platform-independent API for
creating windows, contexts and surfaces, reading input, handling events, etc.

GLFW is written primarily in C99, with parts of macOS support being written in
Objective-C.

GLFW supports Windows, macOS and Linux, and also works on many other Unix-like
systems.  On Linux both Wayland and X11 are supported.

GLFW is licensed under the [zlib/libpng
license](https://www.glfw.org/license.html).

You can [download](https://www.glfw.org/download.html) the latest stable release
as source or Windows and macOS binaries.  There are [release
tags](https://github.com/glfw/glfw/releases) with source and binary archives
attached for every version since 3.0.

The [documentation](https://www.glfw.org/docs/latest/) is available online and is
also included in source and binary archives, except those generated
automatically by Github.  The documentation contains guides, a tutorial and the
API reference.  The [release
notes](https://www.glfw.org/docs/latest/news.html) list the new features,
caveats and deprecations in the latest release.  The [version
history](https://www.glfw.org/changelog.html) lists every user-visible change
for every release.

GLFW exists because of the contributions of [many people](CONTRIBUTORS.md)
around the world, whether by reporting bugs, providing community support, adding
features, reviewing or testing code, debugging, proofreading docs, suggesting
features or fixing bugs.


## System requirements

GLFW supports Windows 7 and later and macOS 10.11 and later.  On GNOME Wayland,
window decorations will be very basic unless the
[libdecor](https://gitlab.freedesktop.org/libdecor/libdecor) package is
installed.  Linux and other Unix-like systems running X11 are supported even
without a desktop environment or modern extensions, although some features
require a clipboard manager or a modern window manager.

See the [compatibility guide](https://www.glfw.org/docs/latest/compat.html)
for more detailed information.


## Compiling GLFW

GLFW supports compilation with Visual C++ (2013 and later), GCC and Clang.  Both
Clang-CL and MinGW-w64 are supported.  Other C99 compilers will likely also
work, but this is not regularly tested.

There are [pre-compiled binaries](https://www.glfw.org/download.html)
available for Windows and macOS.

GLFW itself needs only CMake and the headers and libraries for your operating
system and window system.  No other SDKs are required.

See the [compilation guide](https://www.glfw.org/docs/latest/compile.html) for
more information about compiling GLFW and the exact dependencies required for
each window system.

The examples and test programs depend on a number of tiny libraries.  These are
bundled in the `deps/` directory.  The repository has no submodules.

 - [getopt\_port](https://github.com/kimgr/getopt_port/) for examples
   with command-line options
 - [TinyCThread](https://github.com/tinycthread/tinycthread) for threaded
   examples
 - [glad2](https://github.com/Dav1dde/glad) for loading OpenGL and Vulkan
   functions
 - [linmath.h](https://github.com/datenwolf/linmath.h) for linear algebra in
   examples
 - [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) for test and example UI
 - [stb\_image\_write](https://github.com/nothings/stb) for writing images to disk

The documentation is generated with [Doxygen](https://doxygen.org/) when the
library is built, provided CMake could find a sufficiently new version of it
during configuration.


## Using GLFW

See the [HTML documentation](https://www.glfw.org/docs/latest/) for a tutorial,
guides and the API reference.


## Contributing to GLFW

See the [contribution
guide](https://github.com/glfw/glfw/blob/master/docs/CONTRIBUTING.md) for
more information.

The `master` branch is the stable integration branch and _should_ always compile
and run on all supported platforms.  Details of a newly added feature,
including the public API, may change until it has been included in a release.

The `latest` branch is equivalent to the [highest numbered](https://semver.org/)
release, although it may not always point to the same commit as the tag for that
release.

The `ci` branch is used to trigger continuous integration jobs for code under
testing and should never be relied on for any purpose.


## Reporting bugs

Bugs are reported to our [issue tracker](https://github.com/glfw/glfw/issues).
Please check the [contribution
guide](https://github.com/glfw/glfw/blob/master/docs/CONTRIBUTING.md) for
information on what to include when reporting a bug.


## Changelog since 3.5

None.

## Changelog since 3.4

 - Added `GLFW_UNLIMITED_MOUSE_BUTTONS` input mode that allows mouse buttons beyond
   the limit of the mouse button tokens to be reported (#2423)
 - Added `glfwGetEGLConfig` function to query the `EGLConfig` of a window (#2045)
 - Added `glfwGetGLXFBConfig` function to query the `GLXFBConfig` of a window (#1925)
 - Updated minimum CMake version to 3.16 (#2541)
 - Removed support for building with original MinGW (#2540)
 - [Win32] Removed support for Windows XP and Vista (#2505)
 - [Win32] Bugfix: Media keys were reported with a scancode of 256 (#1768,#2417,#2625)
 - [Cocoa] Added `QuartzCore` framework as link-time dependency
 - [Cocoa] Removed support for OS X 10.10 Yosemite and earlier (#2506)
 - [Cocoa] Bugfix: Cmd+Period, Ctrl+Tab and Ctrl+Esc key events were not emitted
   (#1362,#2278)
 - [Wayland] Bugfix: The fractional scaling related objects were not destroyed
 - [Wayland] Bugfix: `glfwInit` would segfault on compositor with no seat (#2517)
 - [Wayland] Bugfix: A drag entering a non-GLFW surface could cause a segfault
 - [Wayland] Bugfix: Ignore key repeat events when no window has keyboard focus (#2727)
 - [Wayland] Bugfix: Reset key repeat timer when window destroyed (#2741,#2727)
 - [Wayland] Bugfix: Memory would leak if reading a data offer failed midway
 - [Wayland] Bugfix: Retrieved cursor position would be incorrect when hovering over
                     fallback decorations
 - [Wayland] Bugfix: Fallback decorations would report scroll events
 - [Wayland] Bugfix: Keyboard repeat events halted when any key is released (#2568)
 - [Wayland] Bugfix: Fallback decorations would show menu at wrong position
 - [Wayland] Bugfix: The cursor was not updated when clicking through from
   a modal to a fallback decoration
 - [Wayland] Bugfix: The cursor position was not updated when clicking through
   from a modal to the content area
 - [Wayland] Bugfix: free modules at end of terminate function to resolve
   potential segmentation fault (#2744)
 - [Wayland] Bugfix: Confining or disabling the cursor could segfault on
   compositors without `pointer-constraints-unstable-v1`
 - [Wayland] Bugfix: Key repeat did not function on very old compositors
 - [Wayland] Bugfix: The `libwayland-client` library was not unloaded at termination
 - [Wayland] Bugfix: Scroll events were sent twice on some versions of GNOME (#2494)
 - [Wayland] Bugfix: Two-dimensional scroll input was emitted as separate axes
 - [Wayland] Bugfix: Mouse wheel scroll distance was incorrect on some compositors
 - [Wayland] Bugfix: `glfwSwapBuffers` would halt with nonzero swap interval when window
   was suspended (#1350,#2582,#2640,#2719,#2723,#2800,#2827)
 - [Wayland] Bugfix: `glfwPostEmptyEvent` would leak a callback proxy (#2836)
 - [Wayland] Bugfix: `glfwHideWindow` did not always send its request immediately
 - [Wayland] Bugfix: Some event types were not always processed by `glfwPollEvents` or
   `glfwWait*Events` (#2793,#2795)
 - [Wayland] Bugfix: Scaled framebuffer size was sometimes incorect (#2713,#2810)
 - [X11] Bugfix: Running without a WM could trigger an assert (#2593,#2601,#2631)
 - [X11] Bugfix: Occasional crash when an idle display awakes (#2766) 
 - [X11] Bugfix: Prevent BadWindow when creating small windows with a content scale
   less than 1 (#2754)
 - [X11] Bugfix: Clamp width and height to >= 1 to prevent BadValue error and app exit
 - [X11] Bugfix: Floating windows silently became non-floating when hidden (#2276)
 - [X11] Bugfix: The `libXext` library was not unloaded at termination
 - [Linux] Bugfix: The header for `ioctl` was only implicitly included (#2778)
 - [Null] Added Vulkan 'window' surface creation via `VK_EXT_headless_surface`
 - [Null] Added EGL context creation on Mesa via `EGL_MESA_platform_surfaceless`
 - [EGL] Allowed native access on Wayland with `GLFW_CONTEXT_CREATION_API` set to
   `GLFW_NATIVE_CONTEXT_API` (#2518)


## Contact

On [glfw.org](https://www.glfw.org/) you can find the latest version of GLFW, as
well as news, documentation and other information about the project.

If you have questions related to the use of GLFW, we have a
[forum](https://discourse.glfw.org/).

If you have a bug to report, a patch to submit or a feature you'd like to
request, please file it in the
[issue tracker](https://github.com/glfw/glfw/issues) on GitHub.

Finally, if you're interested in helping out with the development of GLFW or
porting it to your favorite platform, join us on the forum or GitHub.
