# 链接指南 <a name="build_guide"></a>

本页讲怎么在自己的程序里用上这个库：包含什么头、宏是什么、链接要加哪些库。

## 包含头文件 <a name="build_include"></a>

```c
#include <PLFW/plfw3.h>
```

头文件里只有声明、常量与不透明句柄，**不引入任何 GL / EGL / Vulkan / OSMesa
头**，也不会定义 `GLFW_INCLUDE_NONE` 那类开关（它们已经删除）。所以可以和 D3D、
Vulkan、GDI 头自由共存，包含顺序无关。

需要原生窗口句柄时：

```c
#define PLFW_EXPOSE_NATIVE_WIN32      // Windows：HWND、HINSTANCE
#include <PLFW/plfw3native.h>

#define PLFW_EXPOSE_NATIVE_WAYLAND    // Linux：wl_display*、wl_surface*
#include <PLFW/plfw3native.h>
```

`plfw3native.h` 只暴露这两段、共 6 个访问器：Win32 侧 `plfwGetWin32Adapter`（适配器设备名）、
`plfwGetWin32Monitor`（显示器设备名）、`plfwGetWin32Window`（`HWND`）；Wayland 侧 `plfwGetWaylandDisplay`、
`plfwGetWaylandMonitor`（`wl_output*`）、`plfwGetWaylandWindow`（`wl_surface*`）。对应后端没编译时这些符号不存在。

## 编译宏 <a name="build_macros"></a>

宏 | 用途
--- | ----
`PLFW_DLL` | 链接 DLL 版本的 PLFW 时定义（CMake 的 `plfw` 目标会自动传播）
`PLFW_EXPOSE_NATIVE_WIN32` | 在 `plfw3native.h` 里暴露 Win32 句柄访问器
`PLFW_EXPOSE_NATIVE_WAYLAND` | 在 `plfw3native.h` 里暴露 Wayland 句柄访问器

库本身用 `_PLFW_WIN32` / `_PLFW_WAYLAND` 选择后端，这两个宏由构建系统定义，不要
在应用里手动定义。

## 链接 <a name="build_link"></a>

静态库（本构建默认）在 Windows 上需要额外链 `gdi32`：

- Windows：`plfw3.lib`（MSVC）或 `libplfw3.a`（MinGW）
- Linux：`libplfw3.a`，另需 `wayland-client`、`wayland-cursor`、`xkbcommon`、
  `dl`、`rt`、`m`

Linux 上这些依赖大多数情况由 pkg-config 帮你补齐，见 [build_link_pkgconfig](#build_link_pkgconfig)。

### Windows（Visual Studio） <a name="build_link_win32"></a>

把 `plfw3.lib` 加进“附加依赖项”，再补系统库：

```text
plfw3.lib
gdi32.lib
```

调试版与发布版 CRT 必须一致：用 CMake 构建时选 `USE_MSVC_RUNTIME_LIBRARY_DLL`
等于 `ON`（默认）就是动态 CRT。DLL 版还要链接 `plfw3dll.lib` 并在代码里定义
`PLFW_DLL`。

### Windows（MinGW） <a name="build_link_mingw"></a>

```sh
gcc my_program.c -o my_program.exe -I/path/to/include -L/path/to/lib -lplfw3 -lgdi32
```

顺序不能反：`-lplfw3` 在前，`-lgdi32` 在后。C++ 程序用 `g++`，并在
`extern "C"` 之外调用即可（头文件已处理）。

### 用 CMake 的源码构建 <a name="build_link_cmake_source"></a>

```cmake
set(PLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/plfw)

target_link_libraries(my_app PRIVATE plfw)
```

`add_subdirectory` 会把 `plfw` 目标加到同一构建里，包含目录与依赖库自动传播。
`PLFW_BUILD_TESTS` / `PLFW_BUILD_EXAMPLES` 只在顶层构建时默认 `ON`，作为子目录
引入时本来就关着；想更彻底可以把它们显式设为 `OFF`。

### 用 CMake 的已安装包 <a name="build_link_cmake_package"></a>

```cmake
find_package(plfw3 3.5 REQUIRED)
target_link_libraries(my_app PRIVATE plfw)
```

需要先 `cmake --install` 到某个前缀，并让 CMake 找到它：`-DCMAKE_PREFIX_PATH=/path/to/prefix`。
包配置导出了 `plfw` 目标及其依赖。

### 用 pkg-config <a name="build_link_pkgconfig"></a>

```sh
cc my_program.c -o my_program $(pkg-config --cflags --libs plfw3)
```

`plfw3.pc` 里 `Requires.private` 列出 `wayland-client wayland-cursor xkbcommon`，
`Libs.private` 列出 `-lrt -lm -ldl`（`-lgdi32` 在 Windows 上）。静态链接时要加
`--static`：

```sh
cc my_program.c -o my_program $(pkg-config --static --cflags --libs plfw3)
```

没有 pkg-config 就直接判 `pkg-config --exists plfw3` 决定是否加上那些库。
