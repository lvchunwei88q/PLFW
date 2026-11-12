# 简介 {#mainpage}

GLFW 是一个自由、开源、跨平台的**窗口、输入与事件**库。本仓库是 GLFW 3.5.1 的
改造分支：图形初始化部分（OpenGL、OpenGL ES、EGL、WGL、GLX、NSGL、OSMesa、
Vulkan）已全部移除，库只负责创建窗口、接收输入、分发事件。

后端只保留 Windows 的 **Win32** 与 Linux 的 **Wayland**，外加 **Null**
（headless，用于无窗口系统的自动化测试）。X11 与 Cocoa 后端已经整体删除：
`GLFW_PLATFORM_X11` / `GLFW_PLATFORM_COCOA` 仍作为标识符存在，但
@ref glfwPlatformSupported 对它们一律返回 `GLFW_FALSE`。

@ref quick_guide 带新手从零写一个能跑的程序。

各部分的指南：

 - @ref intro_guide – 初始化、错误处理与整体设计
 - @ref window_guide – 窗口创建、窗口属性与窗口事件
 - @ref monitor_guide – 显示器的枚举、视频模式与 gamma
 - @ref input_guide – 键盘、鼠标、手柄、时间、剪贴板与拖放
 - @ref compat_guide – 本库依赖的平台能力与协议

要编译这个库，看 [compile_guide](@ref compile_guide)；要把它链进自己的程序，看 [build_guide](@ref build_guide)。

[API 参考](modules.html) 是逐函数的详细文档。

因为库不再创建任何图形上下文，**怎么在窗口里画东西完全由应用决定**：D3D11/D3D12、
Vulkan、GDI、Direct2D，或者干脆把窗口当成一个纯事件来源。

本文档由 Doxygen 从 `docs/` 与公共头文件生成。
