# API 总览 {#intro_guide}

[TOC]

本页讲三件事：如何初始化与收尾、错误怎么报、这个库在设计与兼容性上的约定。

本构建是 GLFW 3.5.1 的“只保留窗口 / 输入 / 事件”版本：没有任何 OpenGL、EGL、
Vulkan 支持，也只剩 Win32（Windows）与 Wayland（Linux）两个真实后端，另有
Null（headless）后端。

## 初始化与终止 {#intro_init}

### 初始化 {#intro_init_init}

```c
int glfwInit(void);
```

成功返回 `GLFW_TRUE`，失败返回 `GLFW_FALSE` 并设置错误码。整个进程只需调用一次，
且必须在主线程调用。成功之后窗口、输入、事件、显示器、手柄 API 才可用。

可以在初始化之前调用的函数很少：[glfwSetErrorCallback](@ref glfwSetErrorCallback)、[glfwGetError](@ref glfwGetError)、
[glfwGetVersion](@ref glfwGetVersion)、[glfwGetVersionString](@ref glfwGetVersionString)、[glfwInitHint](@ref glfwInitHint)、
[glfwInitAllocator](@ref glfwInitAllocator)。用它们来安装错误回调、选择后端、指定分配器。

### 初始化提示 {#init_hints}

@ref glfwInitHint 必须在 @ref glfwInit **之前**调用，初始化之后再改无效。

提示 | 默认值 | 说明
---- | ------ | ----
`GLFW_JOYSTICK_HAT_BUTTONS` | `GLFW_TRUE` | 手柄十字键是否同时上报为 `GLFW_GAMEPAD_BUTTON_DPAD_*` 按键
`GLFW_PLATFORM` | `GLFW_ANY_PLATFORM` | 选择后端，见 @ref platform
`GLFW_WAYLAND_LIBDECOR` | `GLFW_WAYLAND_PREFER_LIBDECOR` | Wayland 下是否使用 libdecor 绘制窗口装饰；设为 `GLFW_WAYLAND_DISABLE_LIBDECOR` 则始终使用 GLFW 自带的简单装饰

### libdecor 提示 {#GLFW_WAYLAND_LIBDECOR_hint}

Wayland 的窗口装饰有两种来源：合成器侧（`xdg-decoration`）与客户端侧（libdecor
或 GLFW 自带实现）。`GLFW_WAYLAND_LIBDECOR` 控制客户端侧：

- `GLFW_WAYLAND_PREFER_LIBDECOR`（默认）：有 libdecor 就用它，没有就退回 GLFW
  自带的简单装饰；
- `GLFW_WAYLAND_DISABLE_LIBDECOR`：始终使用 GLFW 自带的装饰，适合想完全控制标题栏
  绘制或不想链接 libdecor 的场景。

合成器提供 `xdg-decoration` 且客户端选择服务端装饰时，这个提示不起作用。

### 运行时后端选择 {#platform}

这个构建里可选的后端只有三个：

- `GLFW_PLATFORM_WIN32` —— Windows，始终编译进去；
- `GLFW_PLATFORM_WAYLAND` —— Linux，在 `GLFW_BUILD_WAYLAND` 打开时编译进去；
- `GLFW_PLATFORM_NULL` —— 不依赖任何显示服务的空后端，用于 CI 与单元测试。

选择规则：

- `GLFW_ANY_PLATFORM`（默认）：只有一个真实后端时直接交给它初始化，失败时错误描述
  来自该后端；有多个时按表顺序逐个尝试。
- 显式指定 `GLFW_PLATFORM_WAYLAND` 等：该后端未编译或运行环境不具备时，报
  `GLFW_PLATFORM_UNAVAILABLE`。
- `GLFW_PLATFORM_NULL`：只在显式指定时启用。

@ref glfwPlatformSupported 可以查询某个平台标识符是否可用。本构建对
`GLFW_PLATFORM_X11` 与 `GLFW_PLATFORM_COCOA` 一律返回 `GLFW_FALSE`：这两个常量
仍被保留，只是对应的后端已从源码树中删除。@ref glfwGetPlatform 返回实际选中的后端。

Linux 上只剩 Wayland 之后，**不再有 `DISPLAY` 回退**：`DISPLAY` 指向不可用的 X
服务器不会有任何效果，`glfwInit` 会在 Wayland 连接阶段失败。需要无显示运行时，
显式使用 `GLFW_PLATFORM_NULL`。

### 自定义堆分配器 {#init_allocator}

```c
glfwInitAllocator(&allocator);
```

`GLFWallocator` 里三个函数指针（`allocate` / `reallocate` / `deallocate`）加一个
`user` 指针，必须覆盖库内所有分配。用它可以在嵌入式或自管内存池的场景里接管
GLFW 的堆使用。必须在初始化之前设置；设为 `NULL` 恢复默认。

### 终止 {#intro_init_terminate}

```c
void glfwTerminate(void);
```

销毁所有剩余窗口、显示器对象、光标与手柄状态，并卸载后端资源。之后要再次使用
GLFW 必须先重新 [glfwInit](@ref glfwInit)。错误回调与自定义分配器不会被清掉。

## 错误处理 {#error_handling}

GLFW 用错误码 + 错误描述报告失败，有两种等价的获取方式：

- 回调式：@ref glfwSetErrorCallback 注册 `void (*)(int code, const char* description)`。
  报告是异步的，可能在任意一次 API 调用内部发生。
- 轮询式：@ref glfwGetError 取最近一次错误，返回码并把描述写入出参；没有错误时返回
  `GLFW_NO_ERROR`。

错误码类别：`GLFW_NOT_INITIALIZED`、`GLFW_INVALID_ENUM`、`GLFW_INVALID_VALUE`、
`GLFW_OUT_OF_MEMORY`、`GLFW_PLATFORM_ERROR`、`GLFW_PLATFORM_UNAVAILABLE`、
`GLFW_FEATURE_UNAVAILABLE`、`GLFW_CURSOR_UNAVAILABLE`、`GLFW_FORMAT_UNAVAILABLE`。

有三个错误码是为了源码 / ABI 兼容而保留、**本构建不会再产生**的：

- `GLFW_NO_CURRENT_CONTEXT` —— 没有上下文可谈；
- `GLFW_API_UNAVAILABLE`、`GLFW_VERSION_UNAVAILABLE` —— 不再创建任何客户端 API。

后端能力缺失时报 `GLFW_FEATURE_UNAVAILABLE`，而不是静默忽略。典型例子：

- Wayland 不允许客户端放置窗口：@ref glfwSetWindowPos 报该错误，窗口位置查询返回
  平台默认值；
- Wayland 合成器没有 seat / `wl_data_device_manager` 时，剪贴板读写报该错误（详见
  [clipboard](@ref clipboard)）；
- 光标形状在当前光标主题里不存在时报 `GLFW_CURSOR_UNAVAILABLE`。

错误回调内部**不要调用 GLFW**（会重入），只做记录或转发。

## 坐标系 {#coordinate_systems}

GLFW 有两套坐标，混用是常见 bug 来源：

- **屏幕坐标**：窗口位置与大小、光标位置、显示器位置、视频模式尺寸。单位是逻辑
  像素，与系统缩放（每显示器 DPI）无关，跨不同缩放比例的显示器也保持一致。
- **像素坐标**：@ref glfwGetFramebufferSize 返回的帧缓冲尺寸，单位是真实像素。
  在 `GLFW_SCALE_FRAMEBUFFER` 为 `GLFW_TRUE`（默认）且系统缩放为 150% 时，
  窗口 800×480 对应帧缓冲 1200×720。

原点与方向：窗口内容区左上角为 `(0, 0)`，X 向右、Y 向下；显示器位置以主显示器
左上角为原点（Wayland 不提供显示器位置）。窗口位置、显示器矩形这类“区域”以
左上角定点，光标位置以像素中心定点，所以两者会差半个像素。

## 保证与限制 {#guarantees_limitations}

### 指针生命周期 {#lifetime}

返回 `const char*` / `const unsigned char*` / `const float*` 的函数（显示器名、
手柄名、手柄轴与按键数组、视频模式数组、游戏手柄名等），返回的指针由 GLFW 持有，
在下一次调用相关函数或对应对象被销毁前有效。不要 `free` 它，也不要长期保存。

`glfwWindowHintString` 与 `glfwSetWindowTitle` 之类的字符串在调用时立即拷贝。

### 可重入性 {#reentrancy}

**除错误回调外，回调里不要调用 GLFW。** 事件分发期间库处于中间状态：在按键回调里
销毁窗口、在尺寸回调里再调用 `glfwPollEvents`、在监视器回调里枚举监视器，都会破坏
内部状态。调试时把操作排进队列，等回调返回后再处理。

### 线程安全 {#thread_safety}

- 初始化、终止、窗口创建与销毁、事件处理、输入查询、剪贴板：**主线程**。
- [glfwGetTime](@ref glfwGetTime)、[glfwGetTimerValue](@ref glfwGetTimerValue)、[glfwGetTimerFrequency](@ref glfwGetTimerFrequency)、
  [glfwPostEmptyEvent](@ref glfwPostEmptyEvent)：任意线程可调用。
- 本构建没有“当前上下文”这一全局槽位，所以不存在“某个上下文只能在创建它的线程
  使用”这类限制——库里根本没有上下文。

### 版本兼容 {#compatibility}

3.x 系列在源码级保持兼容，但本构建有意删除了上下文与 X11/Cocoa 相关 API，因此
**不是**上游 GLFW 的 ABI 替代品：结构体布局、函数集合都变了。要用本库的代码需
重新编译，且不能引用已删除的符号。

### 事件顺序 {#event_order}

事件按后端到达顺序分发，同一帧内大致遵守：

1. 光标进入 / 离开，然后是光标位置、鼠标按键、滚轮；
2. 按键 `GLFW_PRESS` 永远早于同一键的 `GLFW_REPEAT`，`GLFW_RELEASE` 最后；
3. 字符事件（`glfwSetCharCallback`）跟在产生它的按键事件之后；
4. 窗口尺寸、帧缓冲尺寸、内容缩放回调成组出现，顺序不保证；
5. 关闭请求先置标志，再调用关闭回调。

同一帧里不要假定“所有按键都在鼠标之前”或“尺寸回调一定在帧缓冲回调之前”。

## 版本管理 {#intro_version}

### 编译期版本 {#intro_version_compile}

`GLFW_VERSION_MAJOR`、`GLFW_VERSION_MINOR`、`GLFW_VERSION_REVISION` 是头文件里的
整数常量，本仓库为 `3`、`5`、`1`。

### 运行期版本 {#intro_version_runtime}

```c
int major, minor, rev;
glfwGetVersion(&major, &minor, &rev);
```

用它可以挡住“头文件比库新”的链接事故：头文件版本 ≥ 库版本时才有意义。

### 版本串 {#intro_version_string}

```c
const char* str = glfwGetVersionString();
```

本构建会打印类似 `3.5.1 Win32 Null window/input only MinGW-w64` 或
`3.5.1 Wayland Null window/input only monotonic` 的字符串：包含实际编译进去的
后端列表、固定后缀 `window/input only`、编译器与计时器信息。它适合写进日志、
崩溃报告和问题单。
