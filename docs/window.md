# 窗口指南 <a name="window_guide"></a>

窗口是这个库存在的理由：创建它、配置它、在它上面收事件、最后销毁它。因为库不再
创建图形上下文，**窗口里画什么由你自己决定**——D3D11/D3D12、Vulkan、GDI、
Direct2D，或者只把它当成事件来源。

## 窗口对象 <a name="window_object"></a>

`PLFWwindow*` 是不透明句柄，可能代表一个窗口（Win32）或一个 `xdg_toplevel` 加一个
`wl_surface`（Wayland）。它内部还带输入状态、回调、缩放比例等。多个窗口各自独立。

## 窗口创建 <a name="window_creation"></a>

```c
PLFWwindow* window = plfwCreateWindow(800, 480, "Title", NULL, NULL);
if (!window)
    return 1;   // 记得 plfwTerminate()
```

参数依次是内容区宽、高、标题、显示器、`share`。宽高必须大于 0，否则报
`PLFW_INVALID_VALUE` 并返回 `NULL`。`share` 在**本构建中被忽略**，只是为了让上游
代码继续编译，传 `NULL` 即可。

创建是全有或全无：失败时不会留下半个窗口。

## 窗口提示 <a name="window_hints"></a>

提示在创建**之前**设置，`plfwWindowHint` / `plfwWindowHintString` 会写进一个
内部状态，随下一个 `plfwCreateWindow` 生效并保持到被 `plfwDefaultWindowHints`
重置或再次修改。

### 窗口相关提示 <a name="window_hints_wnd"></a>

提示 | 默认值 | 说明
---- | ------ | ----
`PLFW_RESIZABLE` | `PLFW_TRUE` | 用户能否缩放窗口
`PLFW_VISIBLE` | `PLFW_TRUE` | 窗口初始是否可见；`PLFW_FALSE` 时创建后不可见，需 `plfwShowWindow`
`PLFW_DECORATED` | `PLFW_TRUE` | 是否有标题栏 / 边框 / 关闭按钮；`PLFW_FALSE` 得到无边框窗口
`PLFW_FOCUSED` | `PLFW_TRUE` | 是否在创建时获取输入焦点
`PLFW_AUTO_ICONIFY` | `PLFW_TRUE` | 全屏窗口失焦时是否自动还原为窗口模式
`PLFW_FLOATING` | `PLFW_FALSE` | 是否置顶
`PLFW_MAXIMIZED` | `PLFW_FALSE` | 是否以最大化状态创建
`PLFW_CENTER_CURSOR` | `PLFW_TRUE` | 全屏窗口创建时是否把光标居中
`PLFW_TRANSPARENT_FRAMEBUFFER` | `PLFW_FALSE` | 请求帧缓冲透明（窗口背后可透出桌面）
`PLFW_FOCUS_ON_SHOW` | `PLFW_TRUE` | `plfwShowWindow` 时是否获取焦点
`PLFW_MOUSE_PASSTHROUGH` | `PLFW_FALSE` | 鼠标事件是否穿透到窗口下方
`PLFW_SCALE_TO_MONITOR` | `PLFW_FALSE` | 是否按显示器内容缩放自动调整窗口尺寸
`PLFW_SCALE_FRAMEBUFFER` | `PLFW_TRUE` | 帧缓冲是否随内容缩放放大（高 DPI）
`PLFW_POSITION_X` | `PLFW_ANY_POSITION` | 初始 X 位置；`PLFW_ANY_POSITION` 交由窗口系统摆放
`PLFW_POSITION_Y` | `PLFW_ANY_POSITION` | 初始 Y 位置
`PLFW_REFRESH_RATE` | `PLFW_DONT_CARE` | 全屏窗口期望刷新率，`PLFW_DONT_CARE` 为不限

平台专用提示：

提示 | 默认值 | 说明
---- | ------ | ----
`PLFW_WIN32_KEYBOARD_MENU` | `PLFW_FALSE` | 是否把 `Alt` 单独按下交给窗口菜单（Win32）
`PLFW_WIN32_SHOWDEFAULT` | `PLFW_FALSE` | 是否让系统按 `STARTUPINFO` 里的位置与尺寸摆放窗口（Win32）

字符串提示（`plfwWindowHintString`）：

提示 | 默认值 | 说明
---- | ------ | ----
`PLFW_WAYLAND_APP_ID` | `NULL` | 传给合成器的 `app_id`，决定任务栏图标归组与桌面文件名；不设置时取可执行文件名

已删除的上游提示不再存在：`GLFW_X11_CLASS_NAME`、`GLFW_X11_INSTANCE_NAME`、
`GLFW_COCOA_RETINA_FRAMEBUFFER`、`GLFW_COCOA_FRAME_NAME`、
`GLFW_COCOA_CHDIR_RESOURCES`、`GLFW_COCOA_MENUBAR`，以及全部 OpenGL / Vulkan /
EGL / 角 API 相关提示。代码里还写着它们的需要一并删掉。

### 提示的取值语义 <a name="window_hints_values"></a>

- 布尔提示：`PLFW_TRUE` 或 `PLFW_FALSE`，本构建里就是 1 和 0。
- `PLFW_POSITION_X/Y`：整数屏幕坐标，`PLFW_ANY_POSITION`（`0x80000000`）表示不限。
- `PLFW_REFRESH_RATE`：整数 Hz，`PLFW_DONT_CARE`（`-1`）表示不限。
- 越界值大多被夹到合法范围而不是报错，例如超高刷新率会被后端替换为最接近的模式。

### 硬约束 <a name="window_hints_hard"></a>

`PLFW_SCALE_FRAMEBUFFER`、`PLFW_SCALE_TO_MONITOR`、`PLFW_TRANSPARENT_FRAMEBUFFER`
这类触摸到像素格式或系统外观的提示属于硬约束：平台不满足时创建会失败并报
`PLFW_PLATFORM_ERROR`，而不是悄悄换一个值。其余窗口提示只是“尽量照办”，实际值用
`plfwGetWindowAttrib`、`plfwGetWindowSize`、`plfwGetFramebufferSize`
查询。

## 窗口销毁 <a name="window_destruction"></a>

```c
plfwDestroyWindow(window);
```

窗口句柄立即失效，之后不会再有它的事件。不要销毁已经销毁的窗口，也不要在它自己的
回调里销毁它。程序退出前不需要逐个销毁：`plfwTerminate` 会清理剩余的。

## 窗口事件 <a name="window_events"></a>

每个窗口都有独立回调集合：

回调 | 触发时机
---- | --------
`plfwSetWindowPosCallback` | 窗口位置改变
`plfwSetWindowSizeCallback` | 内容区尺寸改变
`plfwSetWindowCloseCallback` | 用户请求关闭（标志已置上）
`plfwSetWindowRefreshCallback` | 内容需要重绘（窗口露出、从最小化恢复）
`plfwSetWindowFocusCallback` | 获得 / 失去输入焦点
`plfwSetWindowIconifyCallback` | 最小化 / 还原
`plfwSetWindowMaximizeCallback` | 最大化 / 还原
`plfwSetFramebufferSizeCallback` | 帧缓冲像素尺寸改变（含 DPI 变化）
`plfwSetWindowContentScaleCallback` | 内容缩放比例改变
`plfwSetKeyCallback`、`plfwSetCharCallback`、`plfwSetCharModsCallback` | 键盘与文字输入，见 [输入指南](input.md#input_guide)
`plfwSetMouseButtonCallback`、`plfwSetCursorPosCallback`、`plfwSetCursorEnterCallback`、`plfwSetScrollCallback` | 鼠标，见 [输入指南](input.md#input_guide)
`plfwSetDropCallback` | 文件拖放，见 [文件拖放](input.md#path_drop)

回调里不要调用 PLFW，也不要销毁窗口。

## 窗口属性 <a name="window_properties"></a>

### 用户指针 <a name="window_userptr"></a>

```c
plfwSetWindowUserPointer(window, state);
State* state = plfwGetWindowUserPointer(window);
```

放自己的结构（渲染器、输入缓冲等），在回调里第一步取回来。

### 关闭标志 <a name="window_close"></a>

```c
if (plfwWindowShouldClose(window)) break;
plfwSetWindowShouldClose(window, PLFW_TRUE);
```

标志只是标志，窗口不会被自动销毁；由你的主循环判断并退出。用户点关闭按钮或按下
系统关闭快捷键时由后端置上它，随后调用关闭回调；在回调用把它清回 `PLFW_FALSE`
就能“取消关闭”（例如弹确认框）。

### 窗口尺寸 <a name="window_size"></a>

```c
int width, height;
plfwGetWindowSize(window, &width, &height);
plfwSetWindowSize(window, 1024, 768);
```

单位是屏幕坐标（逻辑像素），不含边框。值为 0 或负数时 `plfwSetWindowSize` 报
`PLFW_INVALID_VALUE`。窗口尺寸变化会触发尺寸回调。

### 帧缓冲尺寸 <a name="window_fbsize"></a>

```c
int width, height;
plfwGetFramebufferSize(window, &width, &height);
```

真实像素尺寸，等于窗口尺寸 × 内容缩放（`PLFW_SCALE_FRAMEBUFFER` 为 `PLFW_TRUE`
时）。它才是你 D3D/Vulkan/GDI 表面该用的分辨率。变化时调用帧缓冲尺寸回调，
**不要**在尺寸回调里假定帧缓冲也变了。

#### 帧缓冲属性 <a name="window_attribs_fb"></a>

本构建没有帧缓冲格式提示：`GLFW_RED_BITS`、`GLFW_GREEN_BITS`、`GLFW_BLUE_BITS`、
`GLFW_ALPHA_BITS`、`GLFW_DEPTH_BITS`、`GLFW_STENCIL_BITS`、`GLFW_SRGB_CAPABLE`、
`GLFW_DOUBLEBUFFER` 全部已删除。因此 `plfwGetWindowAttrib` **不接受任何帧缓冲
属性**——能查到的帧缓冲信息只有这里的像素尺寸与 `plfwGetWindowContentScale` 的
缩放比例。位深和像素格式由你选的图形 API 在创建自己的表面时决定。

### 内容缩放 <a name="window_scale"></a>

```c
float xscale, yscale;
plfwGetWindowContentScale(window, &xscale, &yscale);
```

窗口当前所在显示器的缩放比例，用于决定 UI 的物理大小（例如 16 逻辑像素的字体在
1.5 缩放下要按 24 像素排版）。可运行期改变，跟着内容缩放回调刷新。

### 尺寸限制 <a name="window_sizelimits"></a>

```c
plfwSetWindowSizeLimits(window, 320, 240, PLFW_DONT_CARE, PLFW_DONT_CARE);
plfwSetWindowAspectRatio(window, 4, 3);              // 固定 4:3
plfwSetWindowAspectRatio(window, PLFW_DONT_CARE, PLFW_DONT_CARE);  // 取消
```

最小 / 最大值用 `PLFW_DONT_CARE` 表示不限；宽高比用 `PLFW_DONT_CARE, PLFW_DONT_CARE`
取消。两者都能在运行期修改。

### 窗口位置 <a name="window_pos"></a>

```c
int x, y;
plfwGetWindowPos(window, &x, &y);
plfwSetWindowPos(window, 100, 100);
```

屏幕坐标，指内容区左上角在虚拟桌面中的位置。Wayland 不允许客户端摆放窗口：
查询返回合成器给的位置（往往是 `0,0`），设置报 `PLFW_FEATURE_UNAVAILABLE`。程序
要让“记住窗口位置”这类功能只在支持的后端生效。

### 窗口标题 <a name="window_title"></a>

```c
plfwSetWindowTitle(window, "New title");
const char* title = plfwGetWindowTitle(window);
```

Wayland 下标题即 `xdg_toplevel` 的 title；`PLFW_WAYLAND_APP_ID` 决定的是
`app_id`，两者不同。

### 窗口图标 <a name="window_icon"></a>

```c
PLFWimage images[1];
images[0].width = 32; images[0].height = 32; images[0].pixels = pixels;
plfwSetWindowIcon(window, 1, images);
```

`PLFWimage` 是 RGBA8、逐行、左上角起。Wayland 的 `xdg_toplevel` 没有图标概念：
调用被接受但不会产生任何效果（任务栏图标由 `.desktop` 文件与 `app_id` 决定）。

### 窗口显示器 <a name="window_monitor"></a>

```c
PLFWmonitor* monitor = plfwGetWindowMonitor(window);
```

窗口模式返回 `NULL`，全屏模式返回显示器句柄。注意“无边框全屏”只是去掉了装饰的
普通窗口，`plfwGetWindowMonitor` 对它仍返回 `NULL`，见
[window_windowed_full_screen](#window_windowed_full_screen)。

### 全屏窗口 <a name="window_full_screen"></a>

```c
PLFWmonitor* monitor = plfwGetPrimaryMonitor();
const PLFWvidmode* mode = plfwGetVideoMode(monitor);

plfwWindowHint(PLFW_REFRESH_RATE, mode->refreshRate);
PLFWwindow* window = plfwCreateWindow(mode->width, mode->height, "Full screen",
                                     monitor, NULL);
```

全屏窗口的尺寸会成为“期望视频模式”，后端选最接近的支持模式。不要自己 `malloc`
视频模式数组：用 `plfwGetVideoMode`。

运行期切换：

```c
plfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                     mode->refreshRate);
plfwSetWindowMonitor(window, NULL, x, y, width, height, 0);   // 回窗口模式
```

### 无边框全屏 <a name="window_windowed_full_screen"></a>

```c
plfwWindowHint(PLFW_DECORATED, PLFW_FALSE);
PLFWwindow* window = plfwCreateWindow(sw, sh, "Borderless", NULL, NULL);
plfwSetWindowPos(window, 0, 0);
plfwSetWindowSize(window, sw, sh);
```

好处是切换快、能和普通窗口共存、可在多显示器上摆。代价是它仍是窗口模式窗口：
`plfwGetWindowMonitor` 返回 `NULL`，是否真的盖住任务栏取决于后端与用户设置，
显式模式切换也拿不到独占刷新率。

### 最小化与还原 <a name="window_iconify"></a>

```c
plfwIconifyWindow(window);
plfwRestoreWindow(window);
```

状态查询用 `plfwGetWindowAttrib(window, PLFW_ICONIFIED)`。Wayland 合成器可能
不实现最小化，此时调用被忽略。

### 最大化与还原 <a name="window_maximize"></a>

```c
plfwMaximizeWindow(window);
plfwRestoreWindow(window);
```

查询用 `PLFW_MAXIMIZED`。它与最小化互斥：最大化状态下 `plfwRestoreWindow` 还原到
最大化之前的尺寸。

### 显示与隐藏 <a name="window_hide"></a>

```c
plfwShowWindow(window);
plfwHideWindow(window);
```

用 `PLFW_FALSE` 的 `PLFW_VISIBLE` 提示创建的窗口，必须在准备好第一帧之后再显示，
避免白屏闪烁。Wayland 下创建时窗口就已经有 surface，隐藏只是不 attach 内容。

### 输入焦点 <a name="window_focus"></a>

```c
plfwFocusWindow(window);
```

请求焦点是“请求”，不是命令：窗口管理器可以拒绝（Wayland 明确不允许抢焦点），
在后台进程里调用往往无效。查询用 `PLFW_FOCUSED`。

### 请求注意 <a name="window_attention"></a>

```c
plfwRequestWindowAttention(window);
```

标题栏闪烁、任务栏高亮之类的“看这里”提示，不改变焦点。Wayland 下等价于
`xdg_activation` 请求，是否生效由合成器决定。

### 重绘 <a name="window_refresh"></a>

窗口内容需要重画时会调用 `plfwSetWindowRefreshCallback`。只在事件驱动（
`plfwWaitEvents`）的程序里才真正需要它；连续渲染的程序忽略即可。

### 透明度 <a name="window_transparency"></a>

```c
float opacity = plfwGetWindowOpacity(window);
plfwSetWindowOpacity(window, 0.85f);
```

整窗不透明度，`0` 全透明、`1` 不透明。与 `PLFW_TRANSPARENT_FRAMEBUFFER` 不同：后者
让帧缓冲像素的 alpha 生效，前者只是整窗淡出。Wayland 合成器可能不实现。

### 属性读写 <a name="window_attribs"></a>

```c
int focused = plfwGetWindowAttrib(window, PLFW_FOCUSED);
plfwSetWindowAttrib(window, PLFW_RESIZABLE, PLFW_FALSE);
```

可用于 `plfwGetWindowAttrib` 的属性：

属性 | 含义 | 可写
---- | ---- | ----
`PLFW_FOCUSED` | 是否有输入焦点 | 否
`PLFW_ICONIFIED` | 是否已最小化 | 否
`PLFW_VISIBLE` | 是否可见 | 否
`PLFW_MAXIMIZED` | 是否最大化 | 否
`PLFW_HOVERED` | 指针是否在内容区内 | 否
`PLFW_RESIZABLE` | 用户能否缩放 | 是
`PLFW_DECORATED` | 是否有装饰 | 是
`PLFW_FLOATING` | 是否置顶 | 是
`PLFW_AUTO_ICONIFY` | 全屏失焦是否自动还原 | 是
`PLFW_FOCUS_ON_SHOW` | 显示时是否取焦点 | 是
`PLFW_MOUSE_PASSTHROUGH` | 鼠标是否穿透 | 是
`PLFW_TRANSPARENT_FRAMEBUFFER` | 帧缓冲是否透明 | 否

“可写”为“是”的才能传给 `plfwSetWindowAttrib`；传其余属性报
`PLFW_INVALID_ENUM`，传非法值报 `PLFW_INVALID_VALUE`——请求透明帧缓冲之类必须在
创建前用提示表达。
