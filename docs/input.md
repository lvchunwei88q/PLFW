# 输入指南 {#input_guide}

[TOC]

输入分两半：事件（回调，异步通知你发生了什么）和状态（查询函数，随时问“现在按着
什么”）。两者都只描述“用户做了什么”，库里没有任何输入与图形 API 的耦合。

## 事件 {#events}

事件由 [glfwPollEvents](@ref glfwPollEvents)、[glfwWaitEvents](@ref glfwWaitEvents)、@ref glfwWaitEventsTimeout 分发；
不调用它们就永远收不到回调。分发顺序见 [event_order](@ref event_order)。

```c
glfwPollEvents();            // 取回已有事件，立刻返回
glfwWaitEvents();            // 睡到有事件为止
glfwWaitEventsTimeout(0.5);  // 最多睡 0.5 秒
```

从别的线程唤醒 [glfwWaitEvents](@ref glfwWaitEvents)，用 [glfwPostEmptyEvent](@ref glfwPostEmptyEvent)。

## 键盘输入 {#input_keyboard}

### 按键事件 {#input_key}

```c
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

glfwSetKeyCallback(window, keyCallback);
```

- `key`：与布局无关的物理键常量（`GLFW_KEY_A` … `GLFW_KEY_Z`、
  `GLFW_KEY_ESCAPE`、`GLFW_KEY_F1`…），见 [keys](@ref keys)。
- `scancode`：平台原始扫描码。Win32 是 PS/2 set 1 扫描码，Wayland 是 evdev 码。
- `action`：`GLFW_PRESS`、`GLFW_REPEAT`、`GLFW_RELEASE`。
- `mods`：按下时取位 `GLFW_MOD_SHIFT`、`GLFW_MOD_CONTROL`、`GLFW_MOD_ALT`、
  `GLFW_MOD_SUPER`、`GLFW_MOD_CAPS_LOCK`、`GLFW_MOD_NUM_LOCK`。

默认按住不放会持续发 `GLFW_REPEAT`，数量取决于系统重复速率。需要“边沿触发”时只处理
`GLFW_PRESS`：

```c
if (action == GLFW_PRESS)
    togglePause();
```

想自己判断按住状态，用 [glfwGetKey](@ref glfwGetKey)：

```c
if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    moveForward();
```

它查询的是事件分发之后维护的状态，必须在 @ref glfwPollEvents 之后调用才有意义。

`GLFW_STICKY_KEYS` 输入模式可以让 `GLFW_RELEASE` 不漏：设为 `GLFW_TRUE` 后，每个
新按下的键会保留一次“按下”记录直到被 `glfwGetKey` 读到。

### 文本输入 {#input_char}

```c
void charCallback(GLFWwindow* window, unsigned int codepoint);
glfwSetCharCallback(window, charCallback);
```

`codepoint` 是 Unicode 码点（UTF-32），已经处理过键盘布局、`Shift`、死键与输入法
候选。**不要**从按键事件拼字符串：`GLFW_KEY_A` 在不同布局下打的可能是 “q”，而
输入法组合、emoji、中文根本无法用按键表达。

需要区分“字符来自输入法候选还是直接按键”时，用 [glfwSetCharModsCallback](@ref glfwSetCharModsCallback)，它在
字符之外还给出 `mods`。

### 键名与扫描码 {#input_key_name}

```c
const char* name = glfwGetKeyName(GLFW_KEY_A, 0);   // 当前布局下的名字
int scancode = glfwGetKeyScancode(GLFW_KEY_Q);
```

`glfwGetKeyName` 返回该键在当前键盘布局上印的字符（可能是 `"a"`、`"q"`、`"ä"`），
拿不到布局信息时返回 `NULL`。做“按键重绑定”界面时常用：显示布局相关名字，存
布局无关的 `key` 或 `scancode`。

### 修饰键锁定 {#input_mods}

`GLFW_LOCK_KEY_MODS` 输入模式打开后，`mods` 里会带上 `GLFW_MOD_CAPS_LOCK` 与
`GLFW_MOD_NUM_LOCK`。默认不带，因为很多游戏把 `mods` 当纯修饰键位掩码用。

## 鼠标输入 {#input_mouse}

### 光标位置 {#cursor_pos}

```c
double x, y;
glfwGetCursorPos(window, &x, &y);
glfwSetCursorPos(window, x, y);
```

单位是屏幕坐标（逻辑像素），原点在内容区左上角，浮点数以便做亚像素平滑。窗口外
的位置也可能被报告（按住拖动时）。Wayland 下只有当指针位于窗口内、且客户端拿到了
指针焦点时才有新值；`glfwSetCursorPos` 报 `GLFW_FEATURE_UNAVAILABLE`（
`wl_pointer` 没有“摆到某处”的请求）。

### 光标模式 {#cursor_mode}

```c
glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
```

模式 | 行为
---- | ----
`GLFW_CURSOR_NORMAL` | 正常显示，可自由移动
`GLFW_CURSOR_HIDDEN` | 隐藏光标，但位置仍随物理鼠标变化
`GLFW_CURSOR_DISABLED` | 隐藏并“锁住”光标：位置不再随物理移动跳变，靠差值做视角控制
`GLFW_CURSOR_CAPTURED` | 隐藏并按需把光标限制在窗口内（后端支持时）

`GLFW_CURSOR_DISABLED` 是 FPS 视角控制的标准做法：读 @ref glfwGetCursorPos 的增量，
配合 @ref glfwSetCursorPos 归零。Wayland 用 `relative-pointer` 协议实现，合成器不
支持时退化为 `GLFW_CURSOR_HIDDEN`。

### 原始鼠标运动 {#raw_mouse_motion}

```c
if (glfwRawMouseMotionSupported())
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
```

打开后拿到未经系统“指针加速 / 精度修正”的原始位移，适合第一人称视角。不支持时
`glfwRawMouseMotionSupported` 返回 `GLFW_FALSE`，设置该模式会被忽略。

### 光标对象 {#cursor_object}

```c
GLFWcursor* cursor = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
glfwSetCursor(window, cursor);
glfwDestroyCursor(cursor);
```

`GLFWcursor*` 由你创建、由你销毁；把它设为窗口光标后，窗口只借用不拥有。传 `NULL`
给 @ref glfwSetCursor 恢复系统默认箭头。多个窗口可以共享同一个光标对象。

### 标准光标 {#cursor_standard}

```c
GLFW_ARROW_CURSOR, GLFW_IBEAM_CURSOR, GLFW_CROSSHAIR_CURSOR,
GLFW_POINTING_HAND_CURSOR, GLFW_RESIZE_EW_CURSOR, GLFW_RESIZE_NS_CURSOR,
GLFW_RESIZE_NWSE_CURSOR, GLFW_RESIZE_NESW_CURSOR, GLFW_RESIZE_ALL_CURSOR,
GLFW_NOT_ALLOWED_CURSOR, GLFW_HRESIZE_CURSOR, GLFW_VRESIZE_CURSOR
```

`glfwCreateStandardCursor` 从标准形状枚举（[shapes](@ref shapes)）里
挑一个。光标主题里没有这个形状时可能失败并报 `GLFW_CURSOR_UNAVAILABLE`，要有回退。

### 自定义光标 {#cursor_custom}

```c
GLFWimage image;
image.width = 32; image.height = 32; image.pixels = rgba;   // RGBA8，逐行，左上角起
GLFWcursor* cursor = glfwCreateCursor(&image, 1, 1);        // 热点 (1,1)
```

热点必须在图像范围内。Wayland 的 `wl_cursor` 只能用 `wl_shm` 提供像素，创建失败
时回退到标准光标即可。

### 进入与离开 {#cursor_enter}

```c
void cursorEnterCallback(GLFWwindow* window, int entered);
```

`entered` 为 `GLFW_TRUE` 表示指针进入内容区，`GLFW_FALSE` 表示离开。Wayland 下只有
指针获得焦点的窗口才会收到。

### 鼠标按键 {#input_mouse_button}

```c
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
```

`button` 是 `GLFW_MOUSE_BUTTON_LEFT`（1）、`MIDDLE`（3）、`RIGHT`（2）等，见
[buttons](@ref buttons)。`action` 只有 `GLFW_PRESS` / `GLFW_RELEASE`，没有重复。

按住状态用 @ref glfwGetMouseButton 查询，语义同 [glfwGetKey](@ref glfwGetKey)。
`GLFW_STICKY_MOUSE_BUTTONS` 模式保证一次按下不会被漏掉。

### 滚轮 {#scrolling}

```c
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
```

正值 `yoffset` 表示向上 / 向前滚。触控板会给小数偏移；某些平台（Wayland 的
`axis_value120`）按 120 单位一“格”给出，GLFW 已归一化为以 1 格为单位的浮点。

## 手柄 {#joysticks}

手柄不绑定窗口，是全局对象，用数字 `jid`（0–15）标识。@ref glfwJoystickPresent
判断某个槽位是否有设备。

```c
for (int jid = GLFW_JOYSTICK_1; jid <= GLFW_JOYSTICK_LAST; jid++)
{
    if (!glfwJoystickPresent(jid))
        continue;
    printf("%d: %s\n", jid, glfwGetJoystickName(jid));
}
```

### 轴 {#joystick_axis}

```c
int count;
const float* axes = glfwGetJoystickAxes(jid, &count);
```

每个轴归一化到 `[-1, 1]`（扳机类可能是 `[0, 1]`）。“这个轴是 X 还是 Y”由设备决定，
不保证顺序；跨设备统一语义请用 [gamepad](@ref gamepad)。

### 按键 {#joystick_button}

```c
int count;
const unsigned char* buttons = glfwGetJoystickButtons(jid, &count);
```

每项 `GLFW_PRESS` 或 `GLFW_RELEASE`。同样没有跨设备的标准顺序。

### 十字键 {#joystick_hat}

```c
int count;
const unsigned char* hats = glfwGetJoystickHats(jid, &count);
```

每项是位掩码：`GLFW_HAT_UP`、`GLFW_HAT_RIGHT`、`GLFW_HAT_DOWN`、`GLFW_HAT_LEFT`
的组合。`GLFW_JOYSTICK_HAT_BUTTONS` 初始化提示为 `GLFW_TRUE`（默认）时，十字键同时
作为 `GLFW_GAMEPAD_BUTTON_DPAD_*` 出现在按键数组里。

### 名字与 GUID {#joystick_name}

```c
const char* name = glfwGetJoystickName(jid);
const char* guid = glfwGetJoystickGUID(jid);
```

名字是可读描述，GUID 是稳定的十六进制标识（SDLR 格式），用于持久化配置。两者都由
库持有。

### 用户指针 {#joystick_userptr}

```c
glfwSetJoystickUserPointer(jid, myState);
```

注意它按 `jid` 存，设备换了槽位就会跟错，配合 GUID 校验使用。

### 连接事件 {#joystick_event}

```c
void joystickCallback(int jid, int event);
glfwSetJoystickCallback(joystickCallback);
```

`event` 为 `GLFW_CONNECTED` 或 `GLFW_DISCONNECTED`。`jid` 在断开后会被复用，所以
断开时要用 `jid` 做清理，不要留着指针。

Linux 下 Wayland 后端的设备来自 `/dev/input`（evdev）；Windows 下来自 XInput。
两者都要求进程有访问设备的权限，容器里通常没有。

## 游戏手柄 {#gamepad}

游戏手柄是“标准化之后的手柄”：映射表把某设备的轴与按键对准语义位置，于是同一套
代码能跑 Xbox、DS4、Switch Pro 等。

```c
if (glfwJoystickIsGamepad(jid))
{
    GLFWgamepadstate state;
    if (glfwGetGamepadState(jid, &state))
        use(state);
}
```

### 轴 {#gamepad_axes}

`state.axes[]` 下标：`GLFW_GAMEPAD_AXIS_LEFT_X/Y`、`RIGHT_X/Y`、`LEFT_TRIGGER`、
`RIGHT_TRIGGER`。

### 按键 {#gamepad_buttons}

`state.buttons[]` 下标：`A`、`B`、`X`、`Y`、`LEFT_BUMPER`、`RIGHT_BUMPER`、`BACK`、
`START`、`GUIDE`、`LEFT_THUMB`、`RIGHT_THUMB`、`DPAD_UP/RIGHT/DOWN/LEFT`。值为
`GLFW_PRESS` / `GLFW_RELEASE`。

### 映射 {#gamepad_mapping}

```c
glfwUpdateGamepadMappings(mappingString);
```

`mappingString` 是 SDL_GameControllerDB 格式的一行或多行文本（`guid,name,axes...`
以 `
` 分隔），返回成功映射的条数。内置映射覆盖常见设备，私有或新设备可以运行时
追加；追加后再查询同一 `jid` 就能拿到标准化状态。

## 时间 {#time}

```c
double seconds = glfwGetTime();
uint64_t ticks = glfwGetTimerValue();
uint64_t freq  = glfwGetTimerFrequency();
```

`glfwGetTime` 是从 @ref glfwInit 起的秒数，双精度；`glfwSetTime` 归零。
`glfwGetTimerValue` / `glfwGetTimerFrequency` 是原始的高精度计数与频率，适合做
差值计算而不受双精度累积误差影响。

三者都可在任意线程调用，且保证单调（不受系统时间调整影响）。Wayland 下用
`CLOCK_MONOTONIC` 实现，Win32 下用
`QueryPerformanceCounter`。

## 剪贴板 {#clipboard}

```c
glfwSetClipboardString(window, "text");
const char* text = glfwGetClipboardString(window);
```

UTF-8 纯文本，只支持 `text/plain;charset=utf-8`。`glfwGetClipboardString` 返回的
指针在下次剪贴板操作前有效。

Wayland 的剪贴板必须经由 `wl_seat` 上的 `wl_data_device` 才能读写：**合成器没有
提供 seat 或没有 `wl_data_device_manager` 时，这两个调用不再崩溃，而是报
`GLFW_FEATURE_UNAVAILABLE`** 并返回 `NULL`（读）/ 什么也不做（写）。没有窗口时
应传 `NULL`，此时 Wayland 无法选定 `wl_surface`，同样报该错误。

## 文件拖放 {#path_drop}

```c
void dropCallback(GLFWwindow* window, int count, const char** paths);
glfwSetDropCallback(window, dropCallback);
```

`paths` 是 UTF-8 绝对路径数组，仅在回调期间有效，要留就自己拷贝。Windows 用
`WM_DROPFILES`，Wayland 用 `wl_data_device` 的 `text/uri-list`；Wayland 合成器
不提供数据设备时不会收到拖放事件。
