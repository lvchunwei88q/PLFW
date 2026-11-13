# 输入指南 <a name="input_guide"></a>

输入分两半：事件（回调，异步通知你发生了什么）和状态（查询函数，随时问“现在按着
什么”）。两者都只描述“用户做了什么”，库里没有任何输入与图形 API 的耦合。

## 事件 <a name="events"></a>

事件由 `plfwPollEvents`、`plfwWaitEvents`、`plfwWaitEventsTimeout` 分发；
不调用它们就永远收不到回调。分发顺序见 [event_order](intro.md#event_order)。

```c
plfwPollEvents();            // 取回已有事件，立刻返回
plfwWaitEvents();            // 睡到有事件为止
plfwWaitEventsTimeout(0.5);  // 最多睡 0.5 秒
```

从别的线程唤醒 `plfwWaitEvents`，用 `plfwPostEmptyEvent`。

## 键盘输入 <a name="input_keyboard"></a>

### 按键事件 <a name="input_key"></a>

```c
void keyCallback(PLFWwindow* window, int key, int scancode, int action, int mods);

plfwSetKeyCallback(window, keyCallback);
```

- `key`：与布局无关的物理键常量（`PLFW_KEY_A` … `PLFW_KEY_Z`、
  `PLFW_KEY_ESCAPE`、`PLFW_KEY_F1`…），见 [keys](../include/PLFW/plfw3.h)。
- `scancode`：平台原始扫描码。Win32 是 PS/2 set 1 扫描码，Wayland 是 evdev 码。
- `action`：`PLFW_PRESS`、`PLFW_REPEAT`、`PLFW_RELEASE`。
- `mods`：按下时取位 `PLFW_MOD_SHIFT`、`PLFW_MOD_CONTROL`、`PLFW_MOD_ALT`、
  `PLFW_MOD_SUPER`、`PLFW_MOD_CAPS_LOCK`、`PLFW_MOD_NUM_LOCK`。

默认按住不放会持续发 `PLFW_REPEAT`，数量取决于系统重复速率。需要“边沿触发”时只处理
`PLFW_PRESS`：

```c
if (action == PLFW_PRESS)
    togglePause();
```

想自己判断按住状态，用 `plfwGetKey`：

```c
if (plfwGetKey(window, PLFW_KEY_W) == PLFW_PRESS)
    moveForward();
```

它查询的是事件分发之后维护的状态，必须在 `plfwPollEvents` 之后调用才有意义。

`PLFW_STICKY_KEYS` 输入模式可以让 `PLFW_RELEASE` 不漏：设为 `PLFW_TRUE` 后，每个
新按下的键会保留一次“按下”记录直到被 `plfwGetKey` 读到。

### 文本输入 <a name="input_char"></a>

```c
void charCallback(PLFWwindow* window, unsigned int codepoint);
plfwSetCharCallback(window, charCallback);
```

`codepoint` 是 Unicode 码点（UTF-32），已经处理过键盘布局、`Shift`、死键与输入法
候选。**不要**从按键事件拼字符串：`PLFW_KEY_A` 在不同布局下打的可能是 “q”，而
输入法组合、emoji、中文根本无法用按键表达。

需要区分“字符来自输入法候选还是直接按键”时，用 `plfwSetCharModsCallback`，它在
字符之外还给出 `mods`。

### 键名与扫描码 <a name="input_key_name"></a>

```c
const char* name = plfwGetKeyName(PLFW_KEY_A, 0);   // 当前布局下的名字
int scancode = plfwGetKeyScancode(PLFW_KEY_Q);
```

`plfwGetKeyName` 返回该键在当前键盘布局上印的字符（可能是 `"a"`、`"q"`、`"ä"`），
拿不到布局信息时返回 `NULL`。做“按键重绑定”界面时常用：显示布局相关名字，存
布局无关的 `key` 或 `scancode`。

### 修饰键锁定 <a name="input_mods"></a>

`PLFW_LOCK_KEY_MODS` 输入模式打开后，`mods` 里会带上 `PLFW_MOD_CAPS_LOCK` 与
`PLFW_MOD_NUM_LOCK`。默认不带，因为很多游戏把 `mods` 当纯修饰键位掩码用。

## 鼠标输入 <a name="input_mouse"></a>

### 光标位置 <a name="cursor_pos"></a>

```c
double x, y;
plfwGetCursorPos(window, &x, &y);
plfwSetCursorPos(window, x, y);
```

单位是屏幕坐标（逻辑像素），原点在内容区左上角，浮点数以便做亚像素平滑。窗口外
的位置也可能被报告（按住拖动时）。Wayland 下只有当指针位于窗口内、且客户端拿到了
指针焦点时才有新值；`plfwSetCursorPos` 报 `PLFW_FEATURE_UNAVAILABLE`（
`wl_pointer` 没有“摆到某处”的请求）。

### 光标模式 <a name="cursor_mode"></a>

```c
plfwSetInputMode(window, PLFW_CURSOR, PLFW_CURSOR_DISABLED);
```

模式 | 行为
---- | ----
`PLFW_CURSOR_NORMAL` | 正常显示，可自由移动
`PLFW_CURSOR_HIDDEN` | 隐藏光标，但位置仍随物理鼠标变化
`PLFW_CURSOR_DISABLED` | 隐藏并“锁住”光标：位置不再随物理移动跳变，靠差值做视角控制
`PLFW_CURSOR_CAPTURED` | 隐藏并按需把光标限制在窗口内（后端支持时）

`PLFW_CURSOR_DISABLED` 是 FPS 视角控制的标准做法：读 `plfwGetCursorPos` 的增量，
配合 `plfwSetCursorPos` 归零。Wayland 用 `relative-pointer` 协议实现，合成器不
支持时退化为 `PLFW_CURSOR_HIDDEN`。

### 原始鼠标运动 <a name="raw_mouse_motion"></a>

```c
if (plfwRawMouseMotionSupported())
    plfwSetInputMode(window, PLFW_RAW_MOUSE_MOTION, PLFW_TRUE);
```

打开后拿到未经系统“指针加速 / 精度修正”的原始位移，适合第一人称视角。不支持时
`plfwRawMouseMotionSupported` 返回 `PLFW_FALSE`，设置该模式会被忽略。

### 光标对象 <a name="cursor_object"></a>

```c
PLFWcursor* cursor = plfwCreateStandardCursor(PLFW_HRESIZE_CURSOR);
plfwSetCursor(window, cursor);
plfwDestroyCursor(cursor);
```

`PLFWcursor*` 由你创建、由你销毁；把它设为窗口光标后，窗口只借用不拥有。传 `NULL`
给 `plfwSetCursor` 恢复系统默认箭头。多个窗口可以共享同一个光标对象。

### 标准光标 <a name="cursor_standard"></a>

```c
PLFW_ARROW_CURSOR, PLFW_IBEAM_CURSOR, PLFW_CROSSHAIR_CURSOR,
PLFW_POINTING_HAND_CURSOR, PLFW_RESIZE_EW_CURSOR, PLFW_RESIZE_NS_CURSOR,
PLFW_RESIZE_NWSE_CURSOR, PLFW_RESIZE_NESW_CURSOR, PLFW_RESIZE_ALL_CURSOR,
PLFW_NOT_ALLOWED_CURSOR, PLFW_HRESIZE_CURSOR, PLFW_VRESIZE_CURSOR
```

`plfwCreateStandardCursor` 从标准形状枚举（[shapes](../include/PLFW/plfw3.h)）里
挑一个。光标主题里没有这个形状时可能失败并报 `PLFW_CURSOR_UNAVAILABLE`，要有回退。

### 自定义光标 <a name="cursor_custom"></a>

```c
PLFWimage image;
image.width = 32; image.height = 32; image.pixels = rgba;   // RGBA8，逐行，左上角起
PLFWcursor* cursor = plfwCreateCursor(&image, 1, 1);        // 热点 (1,1)
```

热点必须在图像范围内。Wayland 的 `wl_cursor` 只能用 `wl_shm` 提供像素，创建失败
时回退到标准光标即可。

### 进入与离开 <a name="cursor_enter"></a>

```c
void cursorEnterCallback(PLFWwindow* window, int entered);
```

`entered` 为 `PLFW_TRUE` 表示指针进入内容区，`PLFW_FALSE` 表示离开。Wayland 下只有
指针获得焦点的窗口才会收到。

### 鼠标按键 <a name="input_mouse_button"></a>

```c
void mouseButtonCallback(PLFWwindow* window, int button, int action, int mods);
```

`button` 是 `PLFW_MOUSE_BUTTON_LEFT`（1）、`MIDDLE`（3）、`RIGHT`（2）等，见
[buttons](../include/PLFW/plfw3.h)。`action` 只有 `PLFW_PRESS` / `PLFW_RELEASE`，没有重复。

按住状态用 `plfwGetMouseButton` 查询，语义同 `plfwGetKey`。
`PLFW_STICKY_MOUSE_BUTTONS` 模式保证一次按下不会被漏掉。

### 滚轮 <a name="scrolling"></a>

```c
void scrollCallback(PLFWwindow* window, double xoffset, double yoffset);
```

正值 `yoffset` 表示向上 / 向前滚。触控板会给小数偏移；某些平台（Wayland 的
`axis_value120`）按 120 单位一“格”给出，PLFW 已归一化为以 1 格为单位的浮点。

## 手柄 <a name="joysticks"></a>

手柄不绑定窗口，是全局对象，用数字 `jid`（0–15）标识。`plfwJoystickPresent`
判断某个槽位是否有设备。

```c
for (int jid = PLFW_JOYSTICK_1; jid <= PLFW_JOYSTICK_LAST; jid++)
{
    if (!plfwJoystickPresent(jid))
        continue;
    printf("%d: %s\n", jid, plfwGetJoystickName(jid));
}
```

### 轴 <a name="joystick_axis"></a>

```c
int count;
const float* axes = plfwGetJoystickAxes(jid, &count);
```

每个轴归一化到 `[-1, 1]`（扳机类可能是 `[0, 1]`）。“这个轴是 X 还是 Y”由设备决定，
不保证顺序；跨设备统一语义请用 [gamepad](#gamepad)。

### 按键 <a name="joystick_button"></a>

```c
int count;
const unsigned char* buttons = plfwGetJoystickButtons(jid, &count);
```

每项 `PLFW_PRESS` 或 `PLFW_RELEASE`。同样没有跨设备的标准顺序。

### 十字键 <a name="joystick_hat"></a>

```c
int count;
const unsigned char* hats = plfwGetJoystickHats(jid, &count);
```

每项是位掩码：`PLFW_HAT_UP`、`PLFW_HAT_RIGHT`、`PLFW_HAT_DOWN`、`PLFW_HAT_LEFT`
的组合。`PLFW_JOYSTICK_HAT_BUTTONS` 初始化提示为 `PLFW_TRUE`（默认）时，十字键同时
作为 `PLFW_GAMEPAD_BUTTON_DPAD_*` 出现在按键数组里。

### 名字与 GUID <a name="joystick_name"></a>

```c
const char* name = plfwGetJoystickName(jid);
const char* guid = plfwGetJoystickGUID(jid);
```

名字是可读描述，GUID 是稳定的十六进制标识（SDLR 格式），用于持久化配置。两者都由
库持有。

### 用户指针 <a name="joystick_userptr"></a>

```c
plfwSetJoystickUserPointer(jid, myState);
```

注意它按 `jid` 存，设备换了槽位就会跟错，配合 GUID 校验使用。

### 连接事件 <a name="joystick_event"></a>

```c
void joystickCallback(int jid, int event);
plfwSetJoystickCallback(joystickCallback);
```

`event` 为 `PLFW_CONNECTED` 或 `PLFW_DISCONNECTED`。`jid` 在断开后会被复用，所以
断开时要用 `jid` 做清理，不要留着指针。

Linux 下 Wayland 后端的设备来自 `/dev/input`（evdev）；Windows 下来自 XInput。
两者都要求进程有访问设备的权限，容器里通常没有。

## 游戏手柄 <a name="gamepad"></a>

游戏手柄是“标准化之后的手柄”：映射表把某设备的轴与按键对准语义位置，于是同一套
代码能跑 Xbox、DS4、Switch Pro 等。

```c
if (plfwJoystickIsGamepad(jid))
{
    PLFWgamepadstate state;
    if (plfwGetGamepadState(jid, &state))
        use(state);
}
```

### 轴 <a name="gamepad_axes"></a>

`state.axes[]` 下标：`PLFW_GAMEPAD_AXIS_LEFT_X/Y`、`RIGHT_X/Y`、`LEFT_TRIGGER`、
`RIGHT_TRIGGER`。

### 按键 <a name="gamepad_buttons"></a>

`state.buttons[]` 下标：`A`、`B`、`X`、`Y`、`LEFT_BUMPER`、`RIGHT_BUMPER`、`BACK`、
`START`、`GUIDE`、`LEFT_THUMB`、`RIGHT_THUMB`、`DPAD_UP/RIGHT/DOWN/LEFT`。值为
`PLFW_PRESS` / `PLFW_RELEASE`。

### 映射 <a name="gamepad_mapping"></a>

```c
plfwUpdateGamepadMappings(mappingString);
```

`mappingString` 是 SDL_GameControllerDB 格式的一行或多行文本（`guid,name,axes...`
以 `
` 分隔），返回成功映射的条数。内置映射覆盖常见设备，私有或新设备可以运行时
追加；追加后再查询同一 `jid` 就能拿到标准化状态。

## 时间 <a name="time"></a>

```c
double seconds = plfwGetTime();
uint64_t ticks = plfwGetTimerValue();
uint64_t freq  = plfwGetTimerFrequency();
```

`plfwGetTime` 是从 `plfwInit` 起的秒数，双精度；`plfwSetTime` 归零。
`plfwGetTimerValue` / `plfwGetTimerFrequency` 是原始的高精度计数与频率，适合做
差值计算而不受双精度累积误差影响。

三者都可在任意线程调用，且保证单调（不受系统时间调整影响）。Wayland 下用
`CLOCK_MONOTONIC` 实现，Win32 下用
`QueryPerformanceCounter`。

## 剪贴板 <a name="clipboard"></a>

```c
plfwSetClipboardString(window, "text");
const char* text = plfwGetClipboardString(window);
```

UTF-8 纯文本，只支持 `text/plain;charset=utf-8`。`plfwGetClipboardString` 返回的
指针在下次剪贴板操作前有效。

Wayland 的剪贴板必须经由 `wl_seat` 上的 `wl_data_device` 才能读写：**合成器没有
提供 seat 或没有 `wl_data_device_manager` 时，这两个调用不再崩溃，而是报
`PLFW_FEATURE_UNAVAILABLE`** 并返回 `NULL`（读）/ 什么也不做（写）。没有窗口时
应传 `NULL`，此时 Wayland 无法选定 `wl_surface`，同样报该错误。

## 文件拖放 <a name="path_drop"></a>

```c
void dropCallback(PLFWwindow* window, int count, const char** paths);
plfwSetDropCallback(window, dropCallback);
```

`paths` 是 UTF-8 绝对路径数组，仅在回调期间有效，要留就自己拷贝。Windows 用
`WM_DROPFILES`，Wayland 用 `wl_data_device` 的 `text/uri-list`；Wayland 合成器
不提供数据设备时不会收到拖放事件。
