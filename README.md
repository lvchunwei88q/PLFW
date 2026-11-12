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
  分支。保留 `glfwGetWin32Window`、`glfwGetWin32Adapter`、`glfwGetWin32Monitor` 与
  `glfwGetWaylandDisplay`、`glfwGetWaylandMonitor`、`glfwGetWaylandWindow` 这些原生句柄
  访问器（Cocoa / X11 访问器随后端一起删除，见下文）。
- 内部结构（`src/internal.h`）：`_GLFWctxconfig`、`_GLFWcontext`、`window->context`、
  `window->doublebuffer`、平台表中的 EGL / OSMesa / Vulkan 函数指针、
  `library.egl` / `library.osmesa` / `library.vk`、`hints.context`、
  `library.contextSlot`（上下文销毁用的 TLS）。
- 平台层（上下文清理，已覆盖当时存在的四个后端）：
  - Win32：WGL 支持（`wgl_context.c`、WGL 常量 / 函数指针 / 结构体、`opengl32.dll`
    加载）、EGL / Vulkan 表面辅助函数。
  - X11（后端已删除，见下一节）：`x11_platform.h` 中的 GLX 类型、常量、函数指针与
    `libGL.so` 宏，Vulkan Xlib / Xcb 表面类型与 `xcbVulkanSurface`，
    `_GLFWcontextGLX` / `_GLFWlibraryGLX` 结构体。
  - Wayland：`wl_platform.h` 中的 Vulkan Wayland 表面类型、`wl_egl_window` 函数指针
    与宏、`_GLFWwindowWayland.egl` / `_GLFWlibraryWayland.egl` 子结构；`wl_init.c`
    不再加载 `libwayland-egl.so`、删除 `_glfwTerminateEGL/OSMesa`；`wl_window.c`
    删除 `wl_egl_window_resize` 分支、EGL 帧回调与 `_glfwWaitForEGLFrameWayland`、
    上下文创建、销毁时的 `egl.*` 资源释放。
  - Cocoa（后端已删除，见下一节）：Vulkan macOS / Metal 表面类型、`_GLFWcontextNSGL`
    / `_GLFWlibraryNSGL`、`NSOpenGL` 枚举 shim、`GL_SILENCE_DEPRECATION` 块。

### 平台后端范围：只保留 Win32 与 Wayland

X11 与 Cocoa 两个后端**整体删除**（不是用 CMake 选项关掉），源码树里不再存在
任何 X11 / Xlib / Cocoa / NSGL 代码。Windows 用 Win32 后端，Linux 用 Wayland
后端，两者都另有 Null / headless 后端用于无窗口系统的自动化测试。

- 删除的源文件：`src/x11_init.c`、`src/x11_window.c`、`src/x11_monitor.c`、
  `src/x11_platform.h`、`src/xkb_unicode.c`（X11 专用 keysym→Unicode 表），
  `src/cocoa_init.m`、`src/cocoa_window.m`、`src/cocoa_monitor.m`、
  `src/cocoa_joystick.m`、`src/cocoa_platform.h`、`src/cocoa_joystick.h`。
  共 11 个文件。
- `src/platform.h`：去掉 `_GLFW_X11` / `_GLFW_COCOA` 的 include 块与四个
  `GLFW_*_STATE` 状态宏；`GLFW_PLATFORM_*_STATE` 只剩 Win32 / Wayland / Null；
  Linux 手柄后端（`linux_joystick.c`）与 `posix_poll.c` 的启用条件从
  `_GLFW_X11 || _GLFW_WAYLAND` 收敛为 `_GLFW_WAYLAND`（二者 Wayland 仍需要）。
- `src/platform.c`：`supportedPlatforms` 表只剩 Win32 / Wayland；删掉
  “Wayland 与 X11 同时编译时才需要”的 `XDG_SESSION_TYPE` 探测块（只剩一个后端后
  没有选择问题）；版本串去掉 " X11" / " Cocoa"，Linux 构建现在是
  `3.5.1 Wayland Null window/input only`（Wayland 运行时再追加 `monotonic`）。
- `src/CMakeLists.txt` 与顶层 `CMakeLists.txt`：删掉 `GLFW_BUILD_X11` /
  `GLFW_BUILD_COCOA` 两个 `cmake_dependent_option` 与启动时的 “Including … support”
  汇报、Cocoa 的 `enable_language(OBJC)` 与 `-framework Cocoa/IOKit/QuartzCore` 链接、
  X11 的源文件列表，以及整段 `find_package(X11 REQUIRED)` 与 XRandR / Xinerama /
  Xkb / Xcursor / XInput / XShape 头文件检查。**配置阶段不再探测任何 X11 依赖**：
  带 `-DCMAKE_DISABLE_FIND_PACKAGE_X11=TRUE` 配置依旧成功，CMake 会提示该变量
  “未被工程使用”。
- Wayland 的 `pkg_check_modules` REQUIRED 列表去掉 `wayland-egl`：代码早已不再使用
  `libwayland-egl`，运行时依赖只剩 `wayland-client` / `wayland-cursor` / `xkbcommon`。
- 公共 API 同步收敛：
  - 删除 `GLFW_X11_CLASS_NAME`、`GLFW_X11_INSTANCE_NAME`、
    `GLFW_COCOA_RETINA_FRAMEBUFFER`、`GLFW_COCOA_FRAME_NAME`、
    `GLFW_COCOA_CHDIR_RESOURCES`、`GLFW_COCOA_MENUBAR` 六个后端专用 hint
    （没有对应后端时它们只能变成静默无效的提示），以及 `src/internal.h` 中的
    `hints.window.x11`、`hints.window.ns`、`hints.init.ns` 三个子结构。
  - `glfw3native.h` 删除 `GLFW_EXPOSE_NATIVE_COCOA` / `GLFW_EXPOSE_NATIVE_X11`
    两段，连同 `glfwGetCocoaMonitor` / `glfwGetCocoaWindow` / `glfwGetCocoaView`、
    `glfwGetX11Display` / `glfwGetX11Adapter` / `glfwGetX11Monitor` /
    `glfwGetX11Window`、`glfwGetX11SelectionString` / `glfwSetX11SelectionString`。
    保留 `GLFW_EXPOSE_NATIVE_WIN32` 与 `GLFW_EXPOSE_NATIVE_WAYLAND`。
  - `GLFW_PLATFORM_X11` / `GLFW_PLATFORM_COCOA` 这两个平台**标识符**保留：调用方的
    `switch (glfwGetPlatform())` 不会因删除后端而编译不过，但
    `glfwPlatformSupported()` 对它们一律返回 `GLFW_FALSE`，用 `GLFW_PLATFORM`
    hint 显式请求会得到 `GLFW_PLATFORM_UNAVAILABLE`（而不是 `GLFW_INVALID_ENUM`）。
  - `src/mappings.h` 中 `#if defined(_GLFW_COCOA)` 的 macOS 手柄映射表随宏一起删除
    （`mappings.h.in` 同步去掉 `@GLFW_COCOA_MAPPINGS@`）。
- 示例与测试：原有 11 个示例、20 个测试全部依赖 OpenGL 或 Vulkan，已随改造删除
  （仍可从首次提交取回：`git show 3373d3b:examples/gears.c`）。替换为三个无图形
  依赖的程序，全部为本分支自写：
  - `examples/window-input.c`：单窗口的键盘 / 字符 / 鼠标 / 滚轮 / 焦点 / 尺寸 /
    帧缓冲 / 手柄事件全量打印，每秒汇报一次状态。
  - `examples/multi-window.c`：一个事件循环驱动三个窗口，演示用户指针、隐藏 / 显示、
    图标化 / 还原、装饰切换、原生句柄（HWND / wl_surface）、窗口落在哪块显示器、
    跨窗口剪贴板。
  - `tests/smoke.c`：自检式冒烟测试（注册到 CTest 的 `smoke`），覆盖初始化、版本、
    平台后端、定时器、显示器与视频模式、无图形 API 建窗、`share` 参数被忽略、
    窗口尺寸 / 帧缓冲尺寸 / 帧尺寸、可见性 / 图标化 / 可缩放属性、光标与键鼠状态、
    窗口尺寸与位置设置、剪贴板往返、手柄枚举、键扫描码、事件泵、关闭请求与销毁。
    该测试按后端能力自适应：无显示环境自动回落到 headless 后端；Null 后端跳过窗口
    几何断言；Wayland 跳过窗口定位断言（协议不允许客户端定位）；后端声明剪贴板不可用
    时跳过剪贴板断言。另有两条断言专门守住后端范围：
    `glfwPlatformSupported(GLFW_PLATFORM_X11)` 与 `(GLFW_PLATFORM_COCOA)` 必须返回
    `GLFW_FALSE`。
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
5. **平台范围只保留 Win32 与 Wayland**：X11 与 Cocoa 后端已从源码树中整体删除
   （详见上文“平台后端范围”一节），Linux 侧只剩 Wayland + Null，Windows 侧为
   Win32 + Null。顶层 `CMakeLists.txt` 只保留 `GLFW_BUILD_WIN32` /
   `GLFW_BUILD_WAYLAND` 两个后端选项。
6. **null 后端可用**（`GLFW_PLATFORM_NULL`），用于无窗口系统的自动化测试；
   其版本串同样只列 “window/input only”。
7. **Wayland 的剪贴板在无 seat 时明确报错而不是崩溃**。合成器不提供
   `wl_data_device_manager` 或 seat 时，`glfwSetClipboardString` /
   `glfwGetClipboardString` 返回 `GLFW_PLATFORM_ERROR`（上游会解引用空指针）。
8. **Linux 上不再有 `DISPLAY` 回退**：只编译 Wayland 时，`DISPLAY` 指向一个不可用
   的 X 服务器不会有任何效果——`glfwInit()` 会直接在 Wayland 连接阶段失败
   （`GLFW_PLATFORM_UNAVAILABLE` / 平台错误），需要 headless 时显式使用
   `GLFW_PLATFORM_NULL`。

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

# 多窗口演示（三个窗口 + 原生句柄 + 跨窗口剪贴板）
./build/examples/multi-window.exe
```

Linux 侧只剩 Wayland 一个后端，构建依赖随之变少：`libwayland-dev`（提供
`wayland-client` / `wayland-cursor`）、`libxkbcommon-dev`、`wayland-protocols`，
以及运行时的 `wayland-scanner`。**不再需要任何 X11 开发包**（`libx11-dev`、
`libxrandr-dev`、`libxinerama-dev`、`libxcursor-dev`、`libxi-dev` 都可以不装）。
无显示环境下可用 `weston --backend=headless-backend.so` 提供 Wayland 服务。

```sh
# Linux（Wayland + Null）
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DGLFW_BUILD_DOCS=OFF -DGLFW_INSTALL=OFF
cmake --build build-linux

# 有 Wayland 合成器时
env XDG_RUNTIME_DIR=/tmp/xdg WAYLAND_DISPLAY=wl-test ./build-linux/tests/smoke

# 完全没有显示服务时（测试内部用 GLFW_PLATFORM_NULL 重试）
env -u DISPLAY -u WAYLAND_DISPLAY ./build-linux/tests/smoke
```

#### 实测结果

MinGW-w64 GCC 15.2.0 UCRT + CMake 4.3.2 + Ninja（Windows）：

- 库与三个程序全部编译通过，无警告；
- `tests/smoke.exe`：全部检查 ok，含窗口创建（无图形上下文）、窗口/帧缓冲尺寸、
  位置与尺寸设置、窗口属性、光标位置、键鼠状态查询、剪贴板往返、显示器与视频模式、
  手柄枚举、事件泵 30 次唤醒、关闭请求与销毁，以及“X11 / Cocoa 后端不在构建中”
  两条断言；版本串 `3.5.1 Win32 Null window/input only MinGW-w64`；
- `examples/window-input.exe`：桌面出现 800x480 窗口（句柄与标题可被 `FindWindow`
  / `WM_CLOSE` 命中），事件循环稳定（1 秒 46 帧、2 秒 83 帧），收到 `WM_CLOSE` 后正常退出；
- `examples/multi-window.exe`：三个窗口同时存在（`HWND 0x370650` / `0x610688` /
  `0x234080A`），各自报告所在显示器 “Generic PnP Monitor (1 of 1)”，窗口 2 读回窗口 1
  写入的剪贴板内容，关闭请求后干净退出。

GCC 11.4.0 + CMake 3.22.1 + Ninja（Ubuntu 22.04，删除 X11 后的 Wayland 单后端版本）：

- 配置：只打印 `-- Including Wayland support`，不再出现任何 X11 相关探测；
  `pkg_check_modules` 只找 `wayland-client` / `wayland-cursor` / `xkbcommon`；
  额外用 `-DCMAKE_DISABLE_FIND_PACKAGE_X11=TRUE` 配置一次仍成功（CMake 报告该变量
  “未被工程使用”，即工程里已不存在 `find_package(X11)`）；
- 编译：库、两个示例、测试全部通过，`warning:` / `error:` 计数为 0；
- `env -u DISPLAY -u WAYLAND_DISPLAY ./tests/smoke`（无显示）：以
  `GLFW_PLATFORM_NULL` 重试成功，版本串 `3.5.1 Wayland Null window/input only`，
  全部检查 ok；
- `weston --backend=headless-backend.so --socket=wl-test` 下跑 `tests/smoke`：
  `active platform backend: Wayland`，除两项按后端能力跳过的断言外全部 ok——
  窗口定位（Wayland 协议不允许客户端放置窗口）与剪贴板（该合成器没有 seat，
  按预期报 `GLFW_PLATFORM_ERROR`）；
- `window-input` 与 `multi-window` 在同一个 headless 合成器下都跑满 5 秒：
  前者打印 800x480 窗口与每秒状态，后者打印三个 `wl_surface` 句柄（
  `0x5624ac444e90` / `0x5624ac45cca0` / `0x5624ac45d840`）；
- `ldd tests/smoke` 显示只依赖 `libc` / `libm`，没有 `libX11` 等 X11 库；
  Wayland 客户端库是运行时 `dlopen` 的，与上游一致。

X11 与 Cocoa：**已从本仓库删除**，不再需要 macOS 或 X 服务器环境，也不存在
“未编译验证”的后端。

### 已知不再成立的上游内容

- 上游 README 中“为 OpenGL / OpenGL ES / Vulkan 应用开发”“creating windows,
  contexts and surfaces”“On Linux both Wayland and X11 are supported”等描述属于
  上游原文，对本分支不再准确：本分支只保留 Win32 与 Wayland 两个后端。
- `docs/quick.md`、`docs/intro.md`、`docs/window.md`、`docs/moving.md`、
  `docs/compat.md` 中关于上下文创建、`glfwGetProcAddress`、Vulkan 表面创建、
  `glfwSwapBuffers` 的段落均不适用；`docs/internal.md` 中提到的
  `_glfwCreateContext*` 系列函数已不存在。
- `deps/` 里只剩 **`deps/wayland/` 的 9 个协议 XML**（`wayland.xml`、`viewporter.xml`、
  `xdg-shell.xml`、`idle-inhibit-unstable-v1.xml`、`pointer-constraints-unstable-v1.xml`、
  `relative-pointer-unstable-v1.xml`、`fractional-scale-v1.xml`、`xdg-activation-v1.xml`、
  `xdg-decoration-unstable-v1.xml`）。构建时 `wayland-scanner` 要读它们生成
  `-client-protocol.h` / `-client-protocol-code.h`，**不能删**。
  其余曾用于上游 GL 示例的第三方依赖（`deps/glad`、`deps/nuklear.h`、
  `deps/nuklear_glfw_gl2.h`、`deps/stb_image_write.h`、`deps/linmath.h`、
  `deps/tinycthread.*`、`deps/getopt.*`，共 43,949 行）已删除——图形层与 GL 示例
  都不在了，它们在源码、示例、测试与 CMake 中均为零引用。

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
