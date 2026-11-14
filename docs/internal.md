# 内部结构 <a name="internals_guide"></a>

本页给要改这个库的人看：源码树怎么分层、抽象接口在哪、加后端要动什么。

## 源码树布局 <a name="internals_layout"></a>

`src/` 按“与后端无关的核心 / OS 原语层 / 各后端”分层，根目录只放核心文件：

```text
src/
├── internal.h  platform.h            内部结构与平台层声明
├── mappings.h  mappings.h.in         手柄映射（由 CMake 生成）
├── plfw.rc.in                        Windows DLL 版本资源模板
├── init.c  input.c  monitor.c  window.c   公共 API 实现（后端无关）
├── platform.c                        平台表：后端选择
├── posix/            posix_time.* posix_thread.* posix_module.c posix_poll.*
│                     非 Windows 的 OS 原语层（Wayland 与 Null 共用）
├── null/             null_init.c null_monitor.c null_window.c
│                     null_joystick.c/h  null_platform.h
├── win32/            win32_init.c win32_monitor.c win32_window.c
│                     win32_joystick.c/h  win32_platform.h
│                     win32_time.* win32_thread.* win32_module.c
└── wayland/          wl_init.c wl_monitor.c wl_window.c wl_platform.h
                      linux_joystick.c/h
```

规则：根目录的文件**不得包含**任何后端头文件；后端之间互不包含；`platform.h`
是唯一同时认识所有后端的地方。新增一个后端 = 新建 `src/<后端>/` 目录，
在 `platform.h` 里加状态宏与 include 分支，在 `src/CMakeLists.txt` 里加一段
`target_sources`。

## 公共接口 <a name="internals_public"></a>

`include/PLFW/plfw3.h` 是唯一的公共头，`include/PLFW/plfw3native.h` 暴露原生句柄。
所有导出符号都以 `PLFWAPI` 修饰（`__declspec(dllexport)` 或可见性属性），并在
`src/*.c` 里定义。公共头只依赖 `<stddef.h>`、`<stdint.h>`、`<stdarg.h>` 这类 C 标准
头与平台基础类型，不引入任何图形 API 头。

## 原生接口 <a name="internals_native"></a>

`plfw3native.h` 的实现散在 `src/win32/win32_window.c`（`plfwGetWin32Window`）与
`src/wayland/wl_window.c`（`plfwGetWaylandDisplay` / `plfwGetWaylandWindow`）里。每个访问器
都只在对应后端编译时才存在，所以缺后端时是链接错误而不是运行期 `NULL`——这是有意的。

## 内部接口 <a name="internals_internal"></a>

`src/internal.h` 定义所有内部结构：

- `_PLFWwindow`：原生句柄、输入状态、回调表、尺寸与缩放缓存、用户指针；
- `_PLFWmonitor`、`_PLFWvidmode`、`_PLFWcursor`；
- `_PLFWjoystick`：轴 / 按键 / 帽的缓冲区与映射；
- `_PLFWlibrary`：全局单例，含平台表、错误状态、计时器、手柄数组；
- `_PLFWallocator` 与 `_PLFW_REQUIRE_INIT` 之类的辅助宏。

**没有** `_GLFWcontext`、`_GLFWctxconfig`，也没有 `window->context`：这是它和上游
最大的结构差异，任何“给窗口挂上下文”的代码都不该存在。

## 平台层 <a name="internals_platform"></a>

`src/platform.h` 用 `_PLFW_WIN32` / `_PLFW_WAYLAND` / （Null 永远可用）定义
`_PLFW_PLATFORM_*` 状态宏，并把对应后端的头文件 include 进来。

`src/platform.c` 里有一张平台表（`_PLFWplatform` 结构数组），每项提供
`init` / `terminate` / `createWindow` / `destroyWindow` / `pollEvents` 等函数指针，
`plfwInit` 按 `PLFW_PLATFORM` 提示与 `plfwPlatformSupported` 的结果选一个。

- 平台表只剩 Win32、Wayland、Null 三项；
- 平台 ID 只有 `PLFW_ANY_PLATFORM`、`PLFW_PLATFORM_WIN32`、`PLFW_PLATFORM_WAYLAND`、
  `PLFW_PLATFORM_NULL` 四个；X11 / Cocoa 的常量已删除，它们的旧数值现在按未知 ID
  处理（`plfwInitHint` → `PLFW_INVALID_ENUM`，`plfwPlatformSupported` →
  `PLFW_FALSE`）；

## 事件接口 <a name="internals_event"></a>

后端在自己的消息循环里调用 `_plfwInput*` 系列函数（`_plfwInputKey`、`_plfwInputMouseClick`、
`_plfwInputCursorPos`、`_plfwInputWindowSize`、`_plfwInputWindowCloseRequest` 等），
由 `src/input.c` 与 `src/window.c` 统一维护状态并派发回调。加一个新后端只需实现
平台表里的函数，事件上报一律走这些 `_plfwInput*`，不要直接调用户回调。

`plfwPollEvents` → 平台表 `pollEvents` → 后端读原生事件 → `_plfwInput*`。阻塞版本
`plfwWaitEvents` 在 Linux 上靠 `posix/posix_poll.c` 的 `poll()`，Windows 上靠
`MsgWaitForMultipleObjects`；`plfwPostEmptyEvent` 通过一对内部事件 / pipe 唤醒。

## 静态分析工具 <a name="internals_static"></a>

- `src/mappings.h` 由 `src/mappings.h.in` + `CMake/generate-mappings.cmake` 生成，
  内置 SDL_GameControllerDB 映射。更新用 `cmake --build build --target update_mappings`。
- `tests/smoke.c` 是唯一的自动化测试，随 `ctest` 跑；它断言 X11 / Cocoa 后端不在
  构建里，防止误把后端加回来。
- 编译时 `-Wall` / `/W3` 默认打开；本仓库要求零警告。

## 配置宏 <a name="internals_config"></a>

宏 | 作用
--- | ----
`_PLFW_WIN32` | 编译 Win32 后端
`_PLFW_WAYLAND` | 编译 Wayland 后端
`_PLFW_BUILD_DLL` | 目标定义为 DLL（由 CMake 设置）
`HAVE_MEMFD_CREATE` | Linux 上用 `memfd_create` 分配匿名缓冲（自动探测）
`_PLFW_USE_HYBRID_HPG` | Win32 强制高性能 GPU（`PLFW_USE_HYBRID_HPG`）
`_CRT_SECURE_NO_WARNINGS` | MSVC 下屏蔽 CRT 弃用警告
`_DEFAULT_SOURCE` | Linux 上用 glibc 的 POSIX 扩展（`poll` 等）

Null 后端没有自己的宏：它总是被编译进去，只在显式请求 `PLFW_PLATFORM_NULL` 时启用。
