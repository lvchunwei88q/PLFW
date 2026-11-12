# 内部结构 {#internals_guide}

[TOC]

本页给要改这个库的人看：源码树怎么分层、抽象接口在哪、加后端要动什么。

## 公共接口 {#internals_public}

`include/GLFW/glfw3.h` 是唯一的公共头，`include/GLFW/glfw3native.h` 暴露原生句柄。
所有导出符号都以 `GLFWAPI` 修饰（`__declspec(dllexport)` 或可见性属性），并在
`src/*.c` 里定义。公共头只依赖 `<stddef.h>`、`<stdint.h>`、`<stdarg.h>` 这类 C 标准
头与平台基础类型，不引入任何图形 API 头。

## 原生接口 {#internals_native}

`glfw3native.h` 的实现散在 `src/win32_window.c`（`glfwGetWin32Window`）与
`src/wl_window.c`（`glfwGetWaylandDisplay` / `glfwGetWaylandWindow`）里。每个访问器
都只在对应后端编译时才存在，所以缺后端时是链接错误而不是运行期 `NULL`——这是有意的。

## 内部接口 {#internals_internal}

`src/internal.h` 定义所有内部结构：

- `_GLFWwindow`：原生句柄、输入状态、回调表、尺寸与缩放缓存、用户指针；
- `_GLFWmonitor`、`_GLFWvidmode`、`_GLFWcursor`；
- `_GLFWjoystick`：轴 / 按键 / 帽的缓冲区与映射；
- `_GLFWlibrary`：全局单例，含平台表、错误状态、计时器、手柄数组；
- `_GLFWallocator` 与 `_GLFW_REQUIRE_INIT` 之类的辅助宏。

**没有** `_GLFWcontext`、`_GLFWctxconfig`，也没有 `window->context`：这是它和上游
最大的结构差异，任何“给窗口挂上下文”的代码都不该存在。

## 平台层 {#internals_platform}

`src/platform.h` 用 `_GLFW_WIN32` / `_GLFW_WAYLAND` / （Null 永远可用）定义
`_GLFW_PLATFORM_*` 状态宏，并把对应后端的头文件 include 进来。

`src/platform.c` 里有一张平台表（`_GLFWplatform` 结构数组），每项提供
`init` / `terminate` / `createWindow` / `destroyWindow` / `pollEvents` 等函数指针，
`glfwInit` 按 `GLFW_PLATFORM` 提示与 `glfwPlatformSupported` 的结果选一个。

- 平台表只剩 Win32、Wayland、Null 三项；
- `GLFW_PLATFORM_X11`、`GLFW_PLATFORM_COCOA` 的标识符保留，但
  `glfwPlatformSupported` 对它们返回 `GLFW_FALSE`，显式请求得到
  `GLFW_PLATFORM_UNAVAILABLE` 而不是 `GLFW_INVALID_ENUM`——这样调用方的 `switch`
  不会编译不过；

## 事件接口 {#internals_event}

后端在自己的消息循环里调用 `_glfwInput*` 系列函数（`_glfwInputKey`、`_glfwInputMouseClick`、
`_glfwInputCursorPos`、`_glfwInputWindowSize`、`_glfwInputWindowCloseRequest` 等），
由 `src/input.c` 与 `src/window.c` 统一维护状态并派发回调。加一个新后端只需实现
平台表里的函数，事件上报一律走这些 `_glfwInput*`，不要直接调用户回调。

`glfwPollEvents` → 平台表 `pollEvents` → 后端读原生事件 → `_glfwInput*`。阻塞版本
`glfwWaitEvents` 在 Linux 上靠 `posix_poll.c` 的 `poll()`，Windows 上靠
`MsgWaitForMultipleObjects`；`glfwPostEmptyEvent` 通过一对内部事件 / pipe 唤醒。

## 静态分析工具 {#internals_static}

- `src/mappings.h` 由 `src/mappings.h.in` + `CMake/GenerateMappings.cmake` 生成，
  内置 SDL_GameControllerDB 映射。更新用 `cmake --build build --target update_mappings`。
- `tests/smoke.c` 是唯一的自动化测试，随 `ctest` 跑；它断言 X11 / Cocoa 后端不在
  构建里，防止误把后端加回来。
- 编译时 `-Wall` / `/W3` 默认打开；本仓库要求零警告。

## 配置宏 {#internals_config}

宏 | 作用
--- | ----
`_GLFW_WIN32` | 编译 Win32 后端
`_GLFW_WAYLAND` | 编译 Wayland 后端
`_GLFW_BUILD_DLL` | 目标定义为 DLL（由 CMake 设置）
`HAVE_MEMFD_CREATE` | Linux 上用 `memfd_create` 分配匿名缓冲（自动探测）
`_GLFW_USE_HYBRID_HPG` | Win32 强制高性能 GPU（`GLFW_USE_HYBRID_HPG`）
`_CRT_SECURE_NO_WARNINGS` | MSVC 下屏蔽 CRT 弃用警告
`_DEFAULT_SOURCE` | Linux 上用 glibc 的 POSIX 扩展（`poll` 等）

Null 后端没有自己的宏：它总是被编译进去，只在显式请求 `GLFW_PLATFORM_NULL` 时启用。
