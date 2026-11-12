# 支持

本仓库是 GLFW 3.5.1 的裁剪分支（Win32 + Wayland，无图形上下文），改造说明与实测
矩阵见仓库根目录的 `README.md`。

- 使用问题、编译问题：先看 [quick_guide](@ref quick_guide)、[compile_guide](@ref compile_guide)、@ref build_guide
  与 [compat_guide](@ref compat_guide)。
- 改造引入的缺陷：在本仓库的 issue 里报，附上平台与正在运行的后端
  （`glfwGetPlatform()` 的返回值）、编译选项、最小复现代码。
- 与上游 GLFW 的行为差异：对照 `README.md` 的“语义变化”一节，里面列了本构建有意
  改变的行为（后端集合、忽略的 hint、不再产生的错误码等）。

上游 GLFW 的文档、论坛与 issue tracker 仍然适合查“这个 API 原本怎么用”，但请注意
上游文档包含大量本构建已删除的功能（上下文、X11、Cocoa）。
