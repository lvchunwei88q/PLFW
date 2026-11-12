# 链接指南 {#build_guide}

[TOC]

本页讲怎么在自己的程序里用上这个库：包含什么头、宏是什么、链接要加哪些库。

## 包含头文件 {#build_include}

```c
#include <GLFW/glfw3.h>
```

头文件里只有声明、常量与不透明句柄，**不引入任何 GL / EGL / Vulkan / OSMesa
头**，也不会定义 `GLFW_INCLUDE_NONE` 那类开关（它们已经删除）。所以可以和 D3D、
Vulkan、GDI 头自由共存，包含顺序无关。

需要原生窗口句柄时：

```c
#define GLFW_EXPOSE_NATIVE_WIN32      // Windows：HWND、HINSTANCE
#include <GLFW/glfw3native.h>

#define GLFW_EXPOSE_NATIVE_WAYLAND    // Linux：wl_display*、wl_surface*
#include <GLFW/glfw3native.h>
```

`glfw3native.h` 只暴露这两段，`glfwGetWin32Window`、`glfwGetWin32Instance`、
`glfwGetWaylandDisplay`、`glfwGetWaylandWindow`；对应后端没编译时这些符号不存在。

## 编译宏 {#build_macros}

宏 | 用途
--- | ----
`GLFW_DLL` | 链接 DLL 版本的 GLFW 时定义（CMake 的 `glfw` 目标会自动传播）
`GLFW_EXPOSE_NATIVE_WIN32` | 在 `glfw3native.h` 里暴露 Win32 句柄访问器
`GLFW_EXPOSE_NATIVE_WAYLAND` | 在 `glfw3native.h` 里暴露 Wayland 句柄访问器

库本身用 `_GLFW_WIN32` / `_GLFW_WAYLAND` 选择后端，这两个宏由构建系统定义，不要
在应用里手动定义。

## 链接 {#build_link}

静态库（本构建默认）在 Windows 上需要额外链 `gdi32`：

- Windows：`glfw3.lib`（MSVC）或 `libglfw3.a`（MinGW）
- Linux：`libglfw3.a`，另需 `wayland-client`、`wayland-cursor`、`xkbcommon`、
  `dl`、`rt`、`m`

Linux 上这些依赖大多数情况由 pkg-config 帮你补齐，见 [build_link_pkgconfig](@ref build_link_pkgconfig)。

### Windows（Visual Studio） {#build_link_win32}

把 `glfw3.lib` 加进“附加依赖项”，再补系统库：

```text
glfw3.lib
gdi32.lib
```

调试版与发布版 CRT 必须一致：用 CMake 构建时选 `USE_MSVC_RUNTIME_LIBRARY_DLL`
等于 `ON`（默认）就是动态 CRT。DLL 版还要链接 `glfw3dll.lib` 并在代码里定义
`GLFW_DLL`。

### Windows（MinGW） {#build_link_mingw}

```sh
gcc my_program.c -o my_program.exe -I/path/to/include -L/path/to/lib -lglfw3 -lgdi32
```

顺序不能反：`-lglfw3` 在前，`-lgdi32` 在后。C++ 程序用 `g++`，并在
`extern "C"` 之外调用即可（头文件已处理）。

### 用 CMake 的源码构建 {#build_link_cmake_source}

```cmake
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/glfw)

target_link_libraries(my_app PRIVATE glfw)
```

`add_subdirectory` 会把 `glfw` 目标加到同一构建里，包含目录与依赖库自动传播。
`GLFW_BUILD_DOCS` 默认是 `ON`，只是想用库的话关掉它，省一次 Doxygen 运行。

### 用 CMake 的已安装包 {#build_link_cmake_package}

```cmake
find_package(glfw3 3.5 REQUIRED)
target_link_libraries(my_app PRIVATE glfw)
```

需要先 `cmake --install` 到某个前缀，并让 CMake 找到它：`-DCMAKE_PREFIX_PATH=/path/to/prefix`。
包配置导出了 `glfw` 目标及其依赖。

### 用 pkg-config {#build_link_pkgconfig}

```sh
cc my_program.c -o my_program $(pkg-config --cflags --libs glfw3)
```

`glfw3.pc` 里 `Requires.private` 列出 `wayland-client wayland-cursor xkbcommon`，
`Libs.private` 列出 `-lrt -lm -ldl`（`-lgdi32` 在 Windows 上）。静态链接时要加
`--static`：

```sh
cc my_program.c -o my_program $(pkg-config --static --cflags --libs glfw3)
```

没有 pkg-config 就直接判 `pkg-config --exists glfw3` 决定是否加上那些库。
