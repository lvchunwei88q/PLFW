# 显示器指南 {#monitor_guide}

[TOC]

显示器对象由 GLFW 管理，应用不能创建或销毁它们。它们只用于查询视频模式、摆放
全屏窗口、读取工作区与 gamma。

## 显示器对象 {#monitor_object}

`GLFWmonitor*` 是不透明句柄。显示器热插拔时，GLFW 会销毁失效的句柄、为新显示器
创建新句柄，所以**不要在回调之外长期缓存显示器指针**。

### 取回显示器 {#monitor_monitors}

```c
int count;
GLFWmonitor** monitors = glfwGetMonitors(&count);
GLFWmonitor* primary = glfwGetPrimaryMonitor();
```

返回的数组由库持有，在下次调用前有效。主显示器由系统决定，可能不是你期望的那块。
每个显示器都有名字（[glfwGetMonitorName](@ref glfwGetMonitorName)），可以据此选择。

Wayland 下“主显示器”与显示器位置不可靠：协议不暴露全局坐标，位置恒为 `0,0`。

### 显示器配置变化 {#monitor_event}

```c
void monitorCallback(GLFWmonitor* monitor, int event);

glfwSetMonitorCallback(monitorCallback);
```

`event` 是 `GLFW_CONNECTED` 或 `GLFW_DISCONNECTED`。回调发生时管理器状态可能还在
更新，回调里只能记录，不要枚举显示器。

## 显示器属性 {#monitor_properties}

### 视频模式 {#monitor_modes}

```c
int count;
const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);
const GLFWvidmode* current = glfwGetVideoMode(monitor);
```

`GLFWvidmode` 的字段：`width`、`height`、`redBits`、`greenBits`、`blueBits`、
`refreshRate`（Hz）。`glfwGetVideoMode` 返回当前模式，`glfwGetVideoModes` 返回该
显示器支持的全部模式（按分辨率与刷新率排序）。

本构建没有帧缓冲位深提示，`*Bits` 只是平台报告的名义值，不能拿来要求特定格式；
Wayland 下这些字段通常固定为 8/8/8。

### 物理尺寸 {#monitor_size}

```c
int widthMM, heightMM;
glfwGetMonitorPhysicalSize(monitor, &widthMM, &heightMM);
```

单位毫米。很多后端拿不到真实尺寸，此时返回 0，不要用它在 0 值上做除法。

### 内容缩放 {#monitor_scale}

```c
float xscale, yscale;
glfwGetMonitorContentScale(monitor, &xscale, &yscale);
```

系统缩放比例（例如 Windows 150% 时为 1.5）。它决定窗口尺寸（逻辑像素）与帧缓冲
尺寸（真实像素）之间的换算。缩放可在运行期改变，变化会触发显示器回调与窗口的
内容缩放回调。

### 虚拟位置 {#monitor_pos}

```c
int xpos, ypos;
glfwGetMonitorPos(monitor, &xpos, &ypos);
```

显示器左上角在虚拟桌面中的位置，以主显示器左上角为原点（主显示器
`x = y = 0`）。Wayland 恒为 `0,0`。

### 工作区 {#monitor_workarea}

```c
int xpos, ypos, width, height;
glfwGetMonitorWorkarea(monitor, &xpos, &ypos, &width, &height);
```

去掉任务栏 / 顶栏 / Dock 之后可用的矩形，适合算“无边框全屏窗口”的位置。

### 可读名字 {#monitor_name}

```c
const char* name = glfwGetMonitorName(monitor);
```

例如 Windows 上的 `Generic PnP Monitor`、Weston 的 `weston headless`。指针由库持有。

### 用户指针 {#monitor_userptr}

```c
glfwSetMonitorUserPointer(monitor, pointer);
void* pointer = glfwGetMonitorUserPointer(monitor);
```

存自己的结构，便于在监视器回调里定位。

### Gamma {#monitor_gamma}

```c
glfwSetGamma(monitor, 1.0f);                 // 线性
const GLFWgammaramp* ramp = glfwGetGammaRamp(monitor);
glfwSetGammaRamp(monitor, ramp);
```

`GLFWgammaramp` 是三条 `unsigned short` 表（红、绿、蓝）与长度。`glfwSetGamma`
按给定 gamma 值生成一条近似线性化曲线，比手写表更方便。

支持情况：Win32 实现了 gamma。Wayland 没有 gamma 概念：`glfwGetGammaRamp` 报
`GLFW_FEATURE_UNAVAILABLE` 并返回 `NULL`，`glfwSetGamma` 拿不到原始 ramp 就直接返回
（窗口不会因此崩溃，设置也确实没生效）。程序应当把 gamma 当作可选能力，查询失败就
跳过。

## 全屏窗口与显示器

@ref glfwCreateWindow 的第 4 个参数传显示器就得到全屏窗口，见
[window_monitor](@ref window_monitor)；切换窗口模式用 [glfwSetWindowMonitor](@ref glfwSetWindowMonitor)。全屏窗口的尺寸
会成为“期望视频模式”，后端会选最接近的支持模式。
