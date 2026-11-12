# 上手 {#quick_guide}

[TOC]

本指南带你从零写一个完整的小程序：创建一个窗口，处理键盘与关闭事件，读取计时器，
然后干净退出。它只用 `glfw3.h` 里现存的 API——本库**不创建任何图形上下文**，
窗口里画什么由你自己决定。

## 分步说明 {#quick_steps}

### 包含 GLFW 头文件 {#quick_include}

```c
#include <GLFW/glfw3.h>
```

这个头文件包含全部常量、类型与函数声明。它**不再引入任何 GL、EGL、Vulkan、
OSMesa 头文件**，所以不会和你的 D3D、Vulkan 或 GDI 头文件抢占定义。需要拿原生
窗口句柄（`HWND` / `struct wl_surface*`）时，再单独包含：

```c
#define GLFW_EXPOSE_NATIVE_WAYLAND   // 或 GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
```

### 初始化与终止 {#quick_init_term}

```c
if (!glfwInit())
    return 1;

...   // 使用 API

glfwTerminate();
```

@ref glfwInit 成功返回 `GLFW_TRUE`，失败返回 `GLFW_FALSE` 并设置错误。
@ref glfwTerminate 会销毁所有剩余窗口、显示器与手柄对象。窗口在终止时不会自动
“优雅关闭”，它只是被销毁，所以不要跳过这一步。

### 设置错误回调 {#quick_capture_error}

错误不会自动打印，必须自己接：

```c
static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "GLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

glfwSetErrorCallback(errorCallback);
```

回调里的 `code` 是 `GLFW_*_UNAVAILABLE`、`GLFW_PLATFORM_ERROR` 这类类别，具体
含义见 [error_handling](@ref error_handling)。回调里不要调用 GLFW，只能记录。

### 创建窗口 {#quick_create_window}

```c
GLFWwindow* window = glfwCreateWindow(800, 480, "GLFW window", NULL, NULL);
```

- 第 2、3 个参数是初始内容区宽高与标题；宽高必须大于 0。
- 第 4 个参数是显示器：传 `NULL` 得到窗口模式，传 `glfwGetPrimaryMonitor()`
  得到全屏窗口。
- 第 5 个参数 `share` 在**这个构建里被忽略**：保留它是为了让上游代码继续编译，
  传什么都一样，建议直接写 `NULL`。

窗口创建失败返回 `NULL`，此时别忘了 [glfwTerminate](@ref glfwTerminate)。创建后的窗口默认可见、
可缩放、有装饰、接收焦点；这些都可以用 [窗口提示](@ref window_hints) 在创建前
改掉。

### 检查关闭标志 {#quick_window_close}

```c
while (!glfwWindowShouldClose(window))
{
    ...
}
```

用户点击关闭按钮、按 Alt+F4、或在 Wayland 下由合成器发出关闭请求时，GLFW 会
置上这个标志。你也可以自己置：

```c
glfwSetWindowShouldClose(window, GLFW_TRUE);
```

想先问一句“真的要关吗”，用 @ref glfwSetWindowCloseCallback 接住关闭请求，在里面
决定是否让标志保持 `GLFW_FALSE`。

### 接收键盘输入 {#quick_key_input}

```c
static void keyCallback(GLFWwindow* window, int key, int scancode,
                        int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

glfwSetKeyCallback(window, keyCallback);
```

`action` 是 `GLFW_PRESS` / `GLFW_RELEASE` / `GLFW_REPEAT`。`key` 是与布局无关的
物理键常量（见 [keys](@ref keys)），`scancode` 是平台原始扫描码；要拿到“这个键在这个
键盘布局上印的字符”，用 [glfwGetKeyName](@ref glfwGetKeyName)。文字输入（输入法、组合键）走
[glfwSetCharCallback](@ref glfwSetCharCallback)，不要从按键拼字符串。

### 读计时器 {#quick_timer}

```c
const double now = glfwGetTime();
```

@ref glfwGetTime 返回自 @ref glfwInit 起的秒数（双精度），可以直接拿来做帧间隔；
精度用 @ref glfwGetTimerFrequency 判断。@ref glfwSetTime 可以把它归零。

### 处理事件 {#quick_process_events}

```c
glfwPollEvents();          // 不阻塞：把已有事件取回来分发
glfwWaitEvents();          // 阻塞：没事件就睡，省电
glfwWaitEventsTimeout(0.1); // 最多阻塞 0.1 秒
```

事件是按到达顺序分发的（见 [event_order](@ref event_order)）。窗口内容需要重绘时，GLFW 会调用
[glfwSetWindowRefreshCallback](@ref glfwSetWindowRefreshCallback)；渲染循环里通常把它和 `glfwPollEvents` 结合使用。
从别的线程唤醒阻塞中的主循环，用 [glfwPostEmptyEvent](@ref glfwPostEmptyEvent)。

## 完整示例 {#quick_example}

下面这段可以直接编译运行（Windows 用 `gcc demo.c -lglfw3 -lgdi32`，
Linux 见 [build_guide](@ref build_guide)）。它创建 800×480 窗口，打印每一次按键与光标位置，
每秒打印一次时间，`Escape` 或关闭窗口退出。

```c
#include <GLFW/glfw3.h>
#include <stdio.h>

static void errorCallback(int code, const char* description)
{
    fprintf(stderr, "GLFW error 0x%08X: %s\n", (unsigned int) code, description);
}

static void keyCallback(GLFWwindow* window, int key, int scancode,
                        int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void cursorCallback(GLFWwindow* window, double x, double y)
{
    printf("cursor %.1f %.1f\n", x, y);
}

int main(void)
{
    glfwSetErrorCallback(errorCallback);

    if (!glfwInit())
        return 1;

    GLFWwindow* window = glfwCreateWindow(800, 480,
                                          "GLFW window (no graphics API)",
                                          NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, cursorCallback);

    double lastReport = 0.0;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        const double now = glfwGetTime();
        if (now - lastReport >= 1.0)
        {
            int width, height, fbWidth, fbHeight;
            glfwGetWindowSize(window, &width, &height);
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

            printf("t=%.2fs window=%dx%d framebuffer=%dx%d\n",
                   now, width, height, fbWidth, fbHeight);
            lastReport = now;
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
```

示例的其余部分见 `examples/window-input.c`（全量输入事件）与
`examples/multi-window.c`（一个循环带三个窗口、原生句柄、跨窗口剪贴板）。
