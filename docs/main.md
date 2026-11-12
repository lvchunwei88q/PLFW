# 简介 <a name="mainpage"></a>

GLFW 是一个自由、开源、跨平台的**窗口、输入与事件**库。本仓库是 GLFW 3.5.1 的
改造分支：图形初始化部分（OpenGL、OpenGL ES、EGL、WGL、GLX、NSGL、OSMesa、
Vulkan）已全部移除，库只负责创建窗口、接收输入、分发事件。

后端只保留 Windows 的 **Win32** 与 Linux 的 **Wayland**，外加 **Null**
（headless，用于无窗口系统的自动化测试）。X11 与 Cocoa 后端已经整体删除：
`GLFW_PLATFORM_X11` / `GLFW_PLATFORM_COCOA` 仍作为标识符存在，但
`glfwPlatformSupported` 对它们一律返回 `GLFW_FALSE`。

[上手](quick.md#quick_guide) 带新手从零写一个能跑的程序。

各部分的指南：

 - [API 总览](intro.md#intro_guide) – 初始化、错误处理与整体设计
 - [窗口指南](window.md#window_guide) – 窗口创建、窗口属性与窗口事件
 - [显示器指南](monitor.md#monitor_guide) – 显示器的枚举、视频模式与 gamma
 - [输入指南](input.md#input_guide) – 键盘、鼠标、手柄、时间、剪贴板与拖放
 - [兼容性](compat.md#compat_guide) – 本库依赖的平台能力与协议

要编译这个库，看 [compile_guide](compile.md#compile_guide)；要把它链进自己的程序，看 [build_guide](build.md#build_guide)。

[`glfw3.h`](../include/GLFW/glfw3.h) 是逐函数的详细文档。

因为库不再创建任何图形上下文，**怎么在窗口里画东西完全由应用决定**：D3D11/D3D12、
Vulkan、GDI、Direct2D，或者干脆把窗口当成一个纯事件来源。

文档就是 `docs/` 下的这些 Markdown 文件，用任意编辑器或 Markdown 渲染器直接阅读；仓库不随构建生成 HTML。
