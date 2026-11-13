# API 总览 <a name="intro_guide"></a>

本页讲三件事：如何初始化与收尾、错误怎么报、这个库在设计与兼容性上的约定。

本构建是 GLFW 3.5.1 的“只保留窗口 / 输入 / 事件”版本：没有任何 OpenGL、EGL、
Vulkan 支持，也只剩 Win32（Windows）与 Wayland（Linux）两个真实后端，另有
Null（headless）后端。

## 初始化与终止 <a name="intro_init"></a>

### 初始化 <a name="intro_init_init"></a>

```c
int plfwInit(void);
```

成功返回 `PLFW_TRUE`，失败返回 `PLFW_FALSE` 并设置错误码。整个进程只需调用一次，
且必须在主线程调用。成功之后窗口、输入、事件、显示器、手柄 API 才可用。

可以在初始化之前调用的函数很少：`plfwSetErrorCallback`、`plfwGetError`、
`plfwInitHint`、`plfwInitAllocator`。用它们来安装错误回调、选择后端、指定分配器。

### 初始化提示 <a name="init_hints"></a>

`plfwInitHint` 必须在 `plfwInit` **之前**调用，初始化之后再改无效。

提示 | 默认值 | 说明
---- | ------ | ----
`PLFW_JOYSTICK_HAT_BUTTONS` | `PLFW_TRUE` | 手柄十字键是否同时上报为 `PLFW_GAMEPAD_BUTTON_DPAD_*` 按键
`PLFW_PLATFORM` | `PLFW_ANY_PLATFORM` | 选择后端，见 [运行时后端选择](#platform)
`PLFW_WAYLAND_LIBDECOR` | `PLFW_WAYLAND_PREFER_LIBDECOR` | Wayland 下是否使用 libdecor 绘制窗口装饰；设为 `PLFW_WAYLAND_DISABLE_LIBDECOR` 则始终使用 PLFW 自带的简单装饰

### libdecor 提示 <a name="PLFW_WAYLAND_LIBDECOR_hint"></a>

Wayland 的窗口装饰有两种来源：合成器侧（`xdg-decoration`）与客户端侧（libdecor
或 PLFW 自带实现）。`PLFW_WAYLAND_LIBDECOR` 控制客户端侧：

- `PLFW_WAYLAND_PREFER_LIBDECOR`（默认）：有 libdecor 就用它，没有就退回 PLFW
  自带的简单装饰；
- `PLFW_WAYLAND_DISABLE_LIBDECOR`：始终使用 PLFW 自带的装饰，适合想完全控制标题栏
  绘制或不想链接 libdecor 的场景。

合成器提供 `xdg-decoration` 且客户端选择服务端装饰时，这个提示不起作用。

### 运行时后端选择 <a name="platform"></a>

这个构建里可选的后端只有三个：

- `PLFW_PLATFORM_WIN32` —— Windows，始终编译进去；
- `PLFW_PLATFORM_WAYLAND` —— Linux，在 `PLFW_BUILD_WAYLAND` 打开时编译进去；
- `PLFW_PLATFORM_NULL` —— 不依赖任何显示服务的空后端，用于 CI 与单元测试。

选择规则：

- `PLFW_ANY_PLATFORM`（默认）：只有一个真实后端时直接交给它初始化，失败时错误描述
  来自该后端；有多个时按表顺序逐个尝试。
- 显式指定 `PLFW_PLATFORM_WAYLAND` 等：该后端未编译或运行环境不具备时，报
  `PLFW_PLATFORM_UNAVAILABLE`。
- `PLFW_PLATFORM_NULL`：只在显式指定时启用。

`plfwPlatformSupported` 可以查询某个平台标识符是否可用。本构建对
`PLFW_PLATFORM_X11` 与 `PLFW_PLATFORM_COCOA` 一律返回 `PLFW_FALSE`：这两个常量
仍被保留，只是对应的后端已从源码树中删除。`plfwGetPlatform` 返回实际选中的后端。

Linux 上只剩 Wayland 之后，**不再有 `DISPLAY` 回退**：`DISPLAY` 指向不可用的 X
服务器不会有任何效果，`plfwInit` 会在 Wayland 连接阶段失败。需要无显示运行时，
显式使用 `PLFW_PLATFORM_NULL`。

### 自定义堆分配器 <a name="init_allocator"></a>

```c
plfwInitAllocator(&allocator);
```

`PLFWallocator` 里三个函数指针（`allocate` / `reallocate` / `deallocate`）加一个
`user` 指针，必须覆盖库内所有分配。用它可以在嵌入式或自管内存池的场景里接管
PLFW 的堆使用。必须在初始化之前设置；设为 `NULL` 恢复默认。

### 终止 <a name="intro_init_terminate"></a>

```c
void plfwTerminate(void);
```

销毁所有剩余窗口、显示器对象、光标与手柄状态，并卸载后端资源。之后要再次使用
PLFW 必须先重新 `plfwInit`。错误回调与自定义分配器不会被清掉。

## 错误处理 <a name="error_handling"></a>

PLFW 用错误码 + 错误描述报告失败，有两种等价的获取方式：

- 回调式：`plfwSetErrorCallback` 注册 `void (*)(int code, const char* description)`。
  报告是异步的，可能在任意一次 API 调用内部发生。
- 轮询式：`plfwGetError` 取最近一次错误，返回码并把描述写入出参；没有错误时返回
  `PLFW_NO_ERROR`。

错误码类别：`PLFW_NOT_INITIALIZED`、`PLFW_INVALID_ENUM`、`PLFW_INVALID_VALUE`、
`PLFW_OUT_OF_MEMORY`、`PLFW_PLATFORM_ERROR`、`PLFW_PLATFORM_UNAVAILABLE`、
`PLFW_FEATURE_UNAVAILABLE`、`PLFW_CURSOR_UNAVAILABLE`、`PLFW_FORMAT_UNAVAILABLE`。

有三个错误码是为了源码 / ABI 兼容而保留、**本构建不会再产生**的：

- `PLFW_NO_CURRENT_CONTEXT` —— 没有上下文可谈；
- `PLFW_API_UNAVAILABLE`、`PLFW_VERSION_UNAVAILABLE` —— 不再创建任何客户端 API。

后端能力缺失时报 `PLFW_FEATURE_UNAVAILABLE`，而不是静默忽略。典型例子：

- Wayland 不允许客户端放置窗口：`plfwSetWindowPos` 报该错误，窗口位置查询返回
  平台默认值；
- Wayland 合成器没有 seat / `wl_data_device_manager` 时，剪贴板读写报该错误（详见
  [clipboard](input.md#clipboard)）；
- 光标形状在当前光标主题里不存在时报 `PLFW_CURSOR_UNAVAILABLE`。

错误回调内部**不要调用 PLFW**（会重入），只做记录或转发。

## 坐标系 <a name="coordinate_systems"></a>

PLFW 有两套坐标，混用是常见 bug 来源：

- **屏幕坐标**：窗口位置与大小、光标位置、显示器位置、视频模式尺寸。单位是逻辑
  像素，与系统缩放（每显示器 DPI）无关，跨不同缩放比例的显示器也保持一致。
- **像素坐标**：`plfwGetFramebufferSize` 返回的帧缓冲尺寸，单位是真实像素。
  在 `PLFW_SCALE_FRAMEBUFFER` 为 `PLFW_TRUE`（默认）且系统缩放为 150% 时，
  窗口 800×480 对应帧缓冲 1200×720。

原点与方向：窗口内容区左上角为 `(0, 0)`，X 向右、Y 向下；显示器位置以主显示器
左上角为原点（Wayland 不提供显示器位置）。窗口位置、显示器矩形这类“区域”以
左上角定点，光标位置以像素中心定点，所以两者会差半个像素。

## 保证与限制 <a name="guarantees_limitations"></a>

### 指针生命周期 <a name="lifetime"></a>

返回 `const char*` / `const unsigned char*` / `const float*` 的函数（显示器名、
手柄名、手柄轴与按键数组、视频模式数组、游戏手柄名等），返回的指针由 PLFW 持有，
在下一次调用相关函数或对应对象被销毁前有效。不要 `free` 它，也不要长期保存。

`plfwWindowHintString` 与 `plfwSetWindowTitle` 之类的字符串在调用时立即拷贝。

### 可重入性 <a name="reentrancy"></a>

**除错误回调外，回调里不要调用 PLFW。** 事件分发期间库处于中间状态：在按键回调里
销毁窗口、在尺寸回调里再调用 `plfwPollEvents`、在监视器回调里枚举监视器，都会破坏
内部状态。调试时把操作排进队列，等回调返回后再处理。

### 线程安全 <a name="thread_safety"></a>

- 初始化、终止、窗口创建与销毁、事件处理、输入查询、剪贴板：**主线程**。
- `plfwGetTime`、`plfwGetTimerValue`、`plfwGetTimerFrequency`、
  `plfwPostEmptyEvent`：任意线程可调用。
- 本构建没有“当前上下文”这一全局槽位，所以不存在“某个上下文只能在创建它的线程
  使用”这类限制——库里根本没有上下文。

### 版本兼容 <a name="compatibility"></a>

3.x 系列在源码级保持兼容，但本构建有意删除了上下文与 X11/Cocoa 相关 API，因此
**不是**上游 GLFW 的 ABI 替代品：结构体布局、函数集合都变了。要用本库的代码需
重新编译，且不能引用已删除的符号。

### 事件顺序 <a name="event_order"></a>

事件按后端到达顺序分发，同一帧内大致遵守：

1. 光标进入 / 离开，然后是光标位置、鼠标按键、滚轮；
2. 按键 `PLFW_PRESS` 永远早于同一键的 `PLFW_REPEAT`，`PLFW_RELEASE` 最后；
3. 字符事件（`plfwSetCharCallback`）跟在产生它的按键事件之后；
4. 窗口尺寸、帧缓冲尺寸、内容缩放回调成组出现，顺序不保证；
5. 关闭请求先置标志，再调用关闭回调。

同一帧里不要假定“所有按键都在鼠标之前”或“尺寸回调一定在帧缓冲回调之前”。

## 版本与构建标识 <a name="intro_version"></a>

本构建**不提供版本查询 API**：`glfwGetVersion`、`glfwGetVersionString` 以及
`GLFW_VERSION_MAJOR` / `GLFW_VERSION_MINOR` / `GLFW_VERSION_REVISION` 三个宏已全部
删除。库不再回答“我是什么版本”，调用方也就不需要按版本号分支——能力差异直接用
`plfwPlatformSupported` 与
`plfwGetPlatform` 判断。

版本号只存在于仓库层面：`CMakeLists.txt` 里的 `project(PLFW VERSION 3.5.1)` 决定
`plfw3.pc` 的 `Version` 字段与 CMake 包配置的兼容性检查。二进制本身不带版本信息，
排查问题时记录平台后端、编译选项与提交哈希即可。
