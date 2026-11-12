# 兼容性 <a name="compat_guide"></a>

本页列出这个构建依赖哪些平台能力，以及缺了它们会发生什么。因为图形上下文层已经
删除，过去关于 OpenGL / EGL / Vulkan 的兼容矩阵不再适用。

## Windows（Win32 后端） <a name="compat_win32"></a>

依赖 | 说明 | 缺失后果
---- | ---- | ----
Windows 7 及以上 | 用到 `Dwmapi`、`SetProcessDpiAwareness` 等 API | 更老的系统不支持
`user32.dll` | 窗口、消息循环、输入 | 无法运行
`gdi32.dll` | 光标、显示模式、DIB 光标 | 链接失败
`dwmapi.dll` | 透明帧缓冲（`GLFW_TRANSPARENT_FRAMEBUFFER`） | 该提示无效
`XInput` | 手柄 | 手柄不可用
`/dev/input` 等价能力 | — | Win32 不用
Raw Input | `GLFW_RAW_MOUSE_MOTION` | 模式下退化为普通鼠标
高 DPI API | 内容缩放（`glfwGetWindowContentScale`） | 缩放恒为 `1.0`
显示模式切换 | 全屏窗口 | 退化为窗口模式

Windows 上只有 Win32 后端，没有别的选择；`GLFW_BUILD_WIN32=OFF` 会得到一个只剩 Null
的库。

## Wayland <a name="compat_wayland"></a>

### 必需协议

协议 | 用途
---- | ----
`wl_compositor` / `wl_surface` | 创建表面
`wl_seat` | 输入设备入口；**没有 seat 时剪贴板与输入都不可用**
`wl_pointer` / `wl_keyboard` | 鼠标 / 键盘
`xdg_wm_base` | 顶层窗口、最大化、全屏、装饰触发
`wl_shm` | 光标图像与共享内存缓冲
`wl_output` | 显示器

### 可选协议

协议 | 缺失后果
---- | ----
`xdg_decoration` | 无服务端装饰；用 libdecor 或 GLFW 自带装饰替代
`libdecor` | 装饰退化为 GLFW 自带的简单标题栏
`relative_pointer` | `GLFW_CURSOR_DISABLED` 退化为隐藏光标
`pointer_constraints` | 光标无法锁定在窗口内
`fractional_scale` | 分数缩放退化为整数缩放，`glfwGetWindowContentScale` 精度下降
`viewporter` | 全屏缩放路径退化
`idle_inhibit` | 无法阻止熄屏
`xdg_activation` | `glfwRequestWindowAttention` 无效
`wl_data_device` | 剪贴板与文件拖放都不可用（报 `GLFW_FEATURE_UNAVAILABLE`）

### 明确不支持

- **窗口摆放**：`glfwSetWindowPos` 报 `GLFW_FEATURE_UNAVAILABLE`，位置查询返回合成器
  给的值；
- **显示器全局坐标**：`glfwGetMonitorPos` 恒为 `0, 0`；
- **窗口图标**：`glfwSetWindowIcon` 被忽略，图标来自 `.desktop` 与 `app_id`；
- **gamma**：`glfwGetGammaRamp` 报 `GLFW_FEATURE_UNAVAILABLE` 并返回 `NULL`，
  `glfwSetGamma` 随之直接返回（不生效也不崩溃）；
- **指针摆放**：`glfwSetCursorPos` 报 `GLFW_FEATURE_UNAVAILABLE`；
- **抢焦点**：`glfwFocusWindow` 只是请求，合成器可拒绝。

### 关于 XWayland

本构建的 Wayland 后端不依赖 XWayland，也不提供 X11 后端。运行在 XWayland 下的程序
会被当成 X11 客户端，而本库没有 X11 支持，所以它无法在本库创建的窗口上工作——别把
它当成回退路径。需要无显示环境时用 `GLFW_PLATFORM_NULL`。

## Null 后端

不依赖任何显示服务，也没有真实窗口：`glfwCreateWindow` 返回一个桩对象，输入查询恒为
“未按下”，显示器列表可能为空，`glfwGetVideoMode` 返回 `NULL`。它只用于：

- 无显示环境的 CI（`xvfb` 都不需要）；
- 单元测试里验证 API 的状态机；
- 打包检查（确认代码在没有窗口系统时不会崩）。

`GLFW_PLATFORM_NULL` 必须显式指定，`GLFW_ANY_PLATFORM` 不会选到它。
