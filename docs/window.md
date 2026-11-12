# 窗口指南 {#window_guide}

[TOC]

窗口是这个库存在的理由：创建它、配置它、在它上面收事件、最后销毁它。因为库不再
创建图形上下文，**窗口里画什么由你自己决定**——D3D11/D3D12、Vulkan、GDI、
Direct2D，或者只把它当成事件来源。

## 窗口对象 {#window_object}

`GLFWwindow*` 是不透明句柄，可能代表一个窗口（Win32）或一个 `xdg_toplevel` 加一个
`wl_surface`（Wayland）。它内部还带输入状态、回调、缩放比例等。多个窗口各自独立。

## 窗口创建 {#window_creation}

```c
GLFWwindow* window = glfwCreateWindow(800, 480, "Title", NULL, NULL);
if (!window)
    return 1;   // 记得 glfwTerminate()
```

参数依次是内容区宽、高、标题、显示器、`share`。宽高必须大于 0，否则报
`GLFW_INVALID_VALUE` 并返回 `NULL`。`share` 在**本构建中被忽略**，只是为了让上游
代码继续编译，传 `NULL` 即可。

创建是全有或全无：失败时不会留下半个窗口。

## 窗口提示 {#window_hints}

提示在创建**之前**设置，@ref glfwWindowHint / @ref glfwWindowHintString 会写进一个
内部状态，随下一个 @ref glfwCreateWindow 生效并保持到被 @ref glfwDefaultWindowHints
重置或再次修改。

### 窗口相关提示 {#window_hints_wnd}

提示 | 默认值 | 说明
---- | ------ | ----
`GLFW_RESIZABLE` @anchor GLFW_RESIZABLE_hint | `GLFW_TRUE` | 用户能否缩放窗口
`GLFW_VISIBLE` @anchor GLFW_VISIBLE_hint | `GLFW_TRUE` | 窗口初始是否可见；`GLFW_FALSE` 时创建后不可见，需 `glfwShowWindow`
`GLFW_DECORATED` @anchor GLFW_DECORATED_hint | `GLFW_TRUE` | 是否有标题栏 / 边框 / 关闭按钮；`GLFW_FALSE` 得到无边框窗口
`GLFW_FOCUSED` @anchor GLFW_FOCUSED_hint | `GLFW_TRUE` | 是否在创建时获取输入焦点
`GLFW_AUTO_ICONIFY` @anchor GLFW_AUTO_ICONIFY_hint | `GLFW_TRUE` | 全屏窗口失焦时是否自动还原为窗口模式
`GLFW_FLOATING` @anchor GLFW_FLOATING_hint | `GLFW_FALSE` | 是否置顶
`GLFW_MAXIMIZED` @anchor GLFW_MAXIMIZED_hint | `GLFW_FALSE` | 是否以最大化状态创建
`GLFW_CENTER_CURSOR` @anchor GLFW_CENTER_CURSOR_hint | `GLFW_TRUE` | 全屏窗口创建时是否把光标居中
`GLFW_TRANSPARENT_FRAMEBUFFER` @anchor GLFW_TRANSPARENT_FRAMEBUFFER_hint | `GLFW_FALSE` | 请求帧缓冲透明（窗口背后可透出桌面）
`GLFW_FOCUS_ON_SHOW` @anchor GLFW_FOCUS_ON_SHOW_hint | `GLFW_TRUE` | `glfwShowWindow` 时是否获取焦点
`GLFW_MOUSE_PASSTHROUGH` @anchor GLFW_MOUSE_PASSTHROUGH_hint | `GLFW_FALSE` | 鼠标事件是否穿透到窗口下方
`GLFW_SCALE_TO_MONITOR` @anchor GLFW_SCALE_TO_MONITOR_hint | `GLFW_FALSE` | 是否按显示器内容缩放自动调整窗口尺寸
`GLFW_SCALE_FRAMEBUFFER` @anchor GLFW_SCALE_FRAMEBUFFER_hint | `GLFW_TRUE` | 帧缓冲是否随内容缩放放大（高 DPI）
`GLFW_POSITION_X` @anchor GLFW_POSITION_X_hint | `GLFW_ANY_POSITION` | 初始 X 位置；`GLFW_ANY_POSITION` 交由窗口系统摆放
`GLFW_POSITION_Y` @anchor GLFW_POSITION_Y_hint | `GLFW_ANY_POSITION` | 初始 Y 位置
`GLFW_REFRESH_RATE` @anchor GLFW_REFRESH_RATE_hint | `GLFW_DONT_CARE` | 全屏窗口期望刷新率，`GLFW_DONT_CARE` 为不限

平台专用提示：

提示 | 默认值 | 说明
---- | ------ | ----
`GLFW_WIN32_KEYBOARD_MENU` @anchor GLFW_WIN32_KEYBOARD_MENU_hint | `GLFW_FALSE` | 是否把 `Alt` 单独按下交给窗口菜单（Win32）
`GLFW_WIN32_SHOWDEFAULT` @anchor GLFW_WIN32_SHOWDEFAULT_hint | `GLFW_FALSE` | 是否让系统按 `STARTUPINFO` 里的位置与尺寸摆放窗口（Win32）

字符串提示（[glfwWindowHintString](@ref glfwWindowHintString)）：

提示 | 默认值 | 说明
---- | ------ | ----
`GLFW_WAYLAND_APP_ID` @anchor GLFW_WAYLAND_APP_ID_hint | `NULL` | 传给合成器的 `app_id`，决定任务栏图标归组与桌面文件名；不设置时取可执行文件名

已删除的上游提示不再存在：`GLFW_X11_CLASS_NAME`、`GLFW_X11_INSTANCE_NAME`、
`GLFW_COCOA_RETINA_FRAMEBUFFER`、`GLFW_COCOA_FRAME_NAME`、
`GLFW_COCOA_CHDIR_RESOURCES`、`GLFW_COCOA_MENUBAR`，以及全部 OpenGL / Vulkan /
EGL / 角 API 相关提示。代码里还写着它们的需要一并删掉。

### 提示的取值语义 {#window_hints_values}

- 布尔提示：`GLFW_TRUE` 或 `GLFW_FALSE`，本构建里就是 1 和 0。
- `GLFW_POSITION_X/Y`：整数屏幕坐标，`GLFW_ANY_POSITION`（`0x80000000`）表示不限。
- `GLFW_REFRESH_RATE`：整数 Hz，`GLFW_DONT_CARE`（`-1`）表示不限。
- 越界值大多被夹到合法范围而不是报错，例如超高刷新率会被后端替换为最接近的模式。

### 硬约束 {#window_hints_hard}

`GLFW_SCALE_FRAMEBUFFER`、`GLFW_SCALE_TO_MONITOR`、`GLFW_TRANSPARENT_FRAMEBUFFER`
这类触摸到像素格式或系统外观的提示属于硬约束：平台不满足时创建会失败并报
`GLFW_PLATFORM_ERROR`，而不是悄悄换一个值。其余窗口提示只是“尽量照办”，实际值用
[glfwGetWindowAttrib](@ref glfwGetWindowAttrib)、[glfwGetWindowSize](@ref glfwGetWindowSize)、@ref glfwGetFramebufferSize
查询。

## 窗口销毁 {#window_destruction}

```c
glfwDestroyWindow(window);
```

窗口句柄立即失效，之后不会再有它的事件。不要销毁已经销毁的窗口，也不要在它自己的
回调里销毁它。程序退出前不需要逐个销毁：@ref glfwTerminate 会清理剩余的。

## 窗口事件 {#window_events}

每个窗口都有独立回调集合：

回调 | 触发时机
---- | --------
@ref glfwSetWindowPosCallback | 窗口位置改变
@ref glfwSetWindowSizeCallback | 内容区尺寸改变
@ref glfwSetWindowCloseCallback | 用户请求关闭（标志已置上）
@ref glfwSetWindowRefreshCallback | 内容需要重绘（窗口露出、从最小化恢复）
@ref glfwSetWindowFocusCallback | 获得 / 失去输入焦点
@ref glfwSetWindowIconifyCallback | 最小化 / 还原
@ref glfwSetWindowMaximizeCallback | 最大化 / 还原
@ref glfwSetFramebufferSizeCallback | 帧缓冲像素尺寸改变（含 DPI 变化）
@ref glfwSetWindowContentScaleCallback | 内容缩放比例改变
[glfwSetKeyCallback](@ref glfwSetKeyCallback)、[glfwSetCharCallback](@ref glfwSetCharCallback)、@ref glfwSetCharModsCallback | 键盘与文字输入，见 @ref input_guide
[glfwSetMouseButtonCallback](@ref glfwSetMouseButtonCallback)、[glfwSetCursorPosCallback](@ref glfwSetCursorPosCallback)、[glfwSetCursorEnterCallback](@ref glfwSetCursorEnterCallback)、@ref glfwSetScrollCallback | 鼠标，见 @ref input_guide
@ref glfwSetDropCallback | 文件拖放，见 @ref path_drop

回调里不要调用 GLFW，也不要销毁窗口。

## 窗口属性 {#window_properties}

### 用户指针 {#window_userptr}

```c
glfwSetWindowUserPointer(window, state);
State* state = glfwGetWindowUserPointer(window);
```

放自己的结构（渲染器、输入缓冲等），在回调里第一步取回来。

### 关闭标志 {#window_close}

```c
if (glfwWindowShouldClose(window)) break;
glfwSetWindowShouldClose(window, GLFW_TRUE);
```

标志只是标志，窗口不会被自动销毁；由你的主循环判断并退出。用户点关闭按钮或按下
系统关闭快捷键时由后端置上它，随后调用关闭回调；在回调用把它清回 `GLFW_FALSE`
就能“取消关闭”（例如弹确认框）。

### 窗口尺寸 {#window_size}

```c
int width, height;
glfwGetWindowSize(window, &width, &height);
glfwSetWindowSize(window, 1024, 768);
```

单位是屏幕坐标（逻辑像素），不含边框。值为 0 或负数时 `glfwSetWindowSize` 报
`GLFW_INVALID_VALUE`。窗口尺寸变化会触发尺寸回调。

### 帧缓冲尺寸 {#window_fbsize}

```c
int width, height;
glfwGetFramebufferSize(window, &width, &height);
```

真实像素尺寸，等于窗口尺寸 × 内容缩放（`GLFW_SCALE_FRAMEBUFFER` 为 `GLFW_TRUE`
时）。它才是你 D3D/Vulkan/GDI 表面该用的分辨率。变化时调用帧缓冲尺寸回调，
**不要**在尺寸回调里假定帧缓冲也变了。

#### 帧缓冲属性 {#window_attribs_fb}

本构建没有帧缓冲格式提示：`GLFW_RED_BITS`、`GLFW_GREEN_BITS`、`GLFW_BLUE_BITS`、
`GLFW_ALPHA_BITS`、`GLFW_DEPTH_BITS`、`GLFW_STENCIL_BITS`、`GLFW_SRGB_CAPABLE`、
`GLFW_DOUBLEBUFFER` 全部已删除。因此 @ref glfwGetWindowAttrib **不接受任何帧缓冲
属性**——能查到的帧缓冲信息只有这里的像素尺寸与 @ref glfwGetWindowContentScale 的
缩放比例。位深和像素格式由你选的图形 API 在创建自己的表面时决定。

### 内容缩放 {#window_scale}

```c
float xscale, yscale;
glfwGetWindowContentScale(window, &xscale, &yscale);
```

窗口当前所在显示器的缩放比例，用于决定 UI 的物理大小（例如 16 逻辑像素的字体在
1.5 缩放下要按 24 像素排版）。可运行期改变，跟着内容缩放回调刷新。

### 尺寸限制 {#window_sizelimits}

```c
glfwSetWindowSizeLimits(window, 320, 240, GLFW_DONT_CARE, GLFW_DONT_CARE);
glfwSetWindowAspectRatio(window, 4, 3);              // 固定 4:3
glfwSetWindowAspectRatio(window, GLFW_DONT_CARE, GLFW_DONT_CARE);  // 取消
```

最小 / 最大值用 `GLFW_DONT_CARE` 表示不限；宽高比用 `GLFW_DONT_CARE, GLFW_DONT_CARE`
取消。两者都能在运行期修改。

### 窗口位置 {#window_pos}

```c
int x, y;
glfwGetWindowPos(window, &x, &y);
glfwSetWindowPos(window, 100, 100);
```

屏幕坐标，指内容区左上角在虚拟桌面中的位置。Wayland 不允许客户端摆放窗口：
查询返回合成器给的位置（往往是 `0,0`），设置报 `GLFW_FEATURE_UNAVAILABLE`。程序
要让“记住窗口位置”这类功能只在支持的后端生效。

### 窗口标题 {#window_title}

```c
glfwSetWindowTitle(window, "New title");
const char* title = glfwGetWindowTitle(window);
```

Wayland 下标题即 `xdg_toplevel` 的 title；`GLFW_WAYLAND_APP_ID` 决定的是
`app_id`，两者不同。

### 窗口图标 {#window_icon}

```c
GLFWimage images[1];
images[0].width = 32; images[0].height = 32; images[0].pixels = pixels;
glfwSetWindowIcon(window, 1, images);
```

`GLFWimage` 是 RGBA8、逐行、左上角起。Wayland 的 `xdg_toplevel` 没有图标概念：
调用被接受但不会产生任何效果（任务栏图标由 `.desktop` 文件与 `app_id` 决定）。

### 窗口显示器 {#window_monitor}

```c
GLFWmonitor* monitor = glfwGetWindowMonitor(window);
```

窗口模式返回 `NULL`，全屏模式返回显示器句柄。注意“无边框全屏”只是去掉了装饰的
普通窗口，`glfwGetWindowMonitor` 对它仍返回 `NULL`，见
[window_windowed_full_screen](@ref window_windowed_full_screen)。

### 全屏窗口 {#window_full_screen}

```c
GLFWmonitor* monitor = glfwGetPrimaryMonitor();
const GLFWvidmode* mode = glfwGetVideoMode(monitor);

glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Full screen",
                                     monitor, NULL);
```

全屏窗口的尺寸会成为“期望视频模式”，后端选最接近的支持模式。不要自己 `malloc`
视频模式数组：用 [glfwGetVideoMode](@ref glfwGetVideoMode)。

运行期切换：

```c
glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                     mode->refreshRate);
glfwSetWindowMonitor(window, NULL, x, y, width, height, 0);   // 回窗口模式
```

### 无边框全屏 {#window_windowed_full_screen}

```c
glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
GLFWwindow* window = glfwCreateWindow(sw, sh, "Borderless", NULL, NULL);
glfwSetWindowPos(window, 0, 0);
glfwSetWindowSize(window, sw, sh);
```

好处是切换快、能和普通窗口共存、可在多显示器上摆。代价是它仍是窗口模式窗口：
`glfwGetWindowMonitor` 返回 `NULL`，是否真的盖住任务栏取决于后端与用户设置，
显式模式切换也拿不到独占刷新率。

### 最小化与还原 {#window_iconify}

```c
glfwIconifyWindow(window);
glfwRestoreWindow(window);
```

状态查询用 `glfwGetWindowAttrib(window, GLFW_ICONIFIED)`。Wayland 合成器可能
不实现最小化，此时调用被忽略。

### 最大化与还原 {#window_maximize}

```c
glfwMaximizeWindow(window);
glfwRestoreWindow(window);
```

查询用 `GLFW_MAXIMIZED`。它与最小化互斥：最大化状态下 `glfwRestoreWindow` 还原到
最大化之前的尺寸。

### 显示与隐藏 {#window_hide}

```c
glfwShowWindow(window);
glfwHideWindow(window);
```

用 `GLFW_FALSE` 的 `GLFW_VISIBLE` 提示创建的窗口，必须在准备好第一帧之后再显示，
避免白屏闪烁。Wayland 下创建时窗口就已经有 surface，隐藏只是不 attach 内容。

### 输入焦点 {#window_focus}

```c
glfwFocusWindow(window);
```

请求焦点是“请求”，不是命令：窗口管理器可以拒绝（Wayland 明确不允许抢焦点），
在后台进程里调用往往无效。查询用 `GLFW_FOCUSED`。

### 请求注意 {#window_attention}

```c
glfwRequestWindowAttention(window);
```

标题栏闪烁、任务栏高亮之类的“看这里”提示，不改变焦点。Wayland 下等价于
`xdg_activation` 请求，是否生效由合成器决定。

### 重绘 {#window_refresh}

窗口内容需要重画时会调用 [glfwSetWindowRefreshCallback](@ref glfwSetWindowRefreshCallback)。只在事件驱动（
`glfwWaitEvents`）的程序里才真正需要它；连续渲染的程序忽略即可。

### 透明度 {#window_transparency}

```c
float opacity = glfwGetWindowOpacity(window);
glfwSetWindowOpacity(window, 0.85f);
```

整窗不透明度，`0` 全透明、`1` 不透明。与 `GLFW_TRANSPARENT_FRAMEBUFFER` 不同：后者
让帧缓冲像素的 alpha 生效，前者只是整窗淡出。Wayland 合成器可能不实现。

### 属性读写 {#window_attribs}

```c
int focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);
```

可用于 @ref glfwGetWindowAttrib 的属性：

属性 | 含义 | 可写
---- | ---- | ----
`GLFW_FOCUSED` @anchor GLFW_FOCUSED_attrib | 是否有输入焦点 | 否
`GLFW_ICONIFIED` @anchor GLFW_ICONIFIED_attrib | 是否已最小化 | 否
`GLFW_VISIBLE` @anchor GLFW_VISIBLE_attrib | 是否可见 | 否
`GLFW_MAXIMIZED` @anchor GLFW_MAXIMIZED_attrib | 是否最大化 | 否
`GLFW_HOVERED` @anchor GLFW_HOVERED_attrib | 指针是否在内容区内 | 否
`GLFW_RESIZABLE` @anchor GLFW_RESIZABLE_attrib | 用户能否缩放 | 是
`GLFW_DECORATED` @anchor GLFW_DECORATED_attrib | 是否有装饰 | 是
`GLFW_FLOATING` @anchor GLFW_FLOATING_attrib | 是否置顶 | 是
`GLFW_AUTO_ICONIFY` @anchor GLFW_AUTO_ICONIFY_attrib | 全屏失焦是否自动还原 | 是
`GLFW_FOCUS_ON_SHOW` @anchor GLFW_FOCUS_ON_SHOW_attrib | 显示时是否取焦点 | 是
`GLFW_MOUSE_PASSTHROUGH` @anchor GLFW_MOUSE_PASSTHROUGH_attrib | 鼠标是否穿透 | 是
`GLFW_TRANSPARENT_FRAMEBUFFER` @anchor GLFW_TRANSPARENT_FRAMEBUFFER_attrib | 帧缓冲是否透明 | 否

“可写”为“是”的才能传给 [glfwSetWindowAttrib](@ref glfwSetWindowAttrib)；传其余属性报
`GLFW_INVALID_ENUM`，传非法值报 `GLFW_INVALID_VALUE`——请求透明帧缓冲之类必须在
创建前用提示表达。
