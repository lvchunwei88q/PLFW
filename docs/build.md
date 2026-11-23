# 构建、链接与内部结构 <a name="compile_guide"></a> <a name="internals_guide"></a>

本页讲三件事：把库编出来（依赖、CMake 选项、产物、手动编译），把它链进自己的程序
（头文件、宏、链接方式），以及源码树怎么分层、内部接口在哪（维护者向）。

## 用 CMake 构建 <a name="compile_cmake"></a>

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows 上用 MSVC，Linux 上用 GCC；这两条是硬约束，别的编译器（MinGW、Clang、
clang-cl、MSYS2）在配置阶段就会被 `message(FATAL_ERROR)` 拒掉。Windows 下建议在
“x64 Native Tools Command Prompt”里执行（或先 `call vcvars64.bat`），让 `cl.exe`
与 `ninja.exe` 出现在 PATH 上；也可以用 Visual Studio 生成器，版本名以
`cmake --help` 里的列表为准，形如 `-G "Visual Studio 17 2022" -A x64`。构建产物：

- `build/src/plfw3.lib`（Windows/MSVC）或 `libplfw3.a`（Linux）
- `build/tests/smoke.exe`、`build/examples/window-input.exe`、
  `build/examples/multi-window.exe`、`build/examples/hello-window.exe`
  （`PLFW_BUILD_TESTS` / `PLFW_BUILD_EXAMPLES` 打开时）

想跑一遍自检：

```sh
ctest --test-dir build
```

`tests/smoke.c` 是不依赖图形上下文的冒烟测试：初始化、建窗、查询属性、收事件、退出，
并在 Wayland 上跳过需要合成器能力的断言。

### 只要库的最小构建 <a name="compile_compile"></a>

```sh
cmake -S . -B build -DPLFW_BUILD_TESTS=OFF -DPLFW_BUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

构建过程中你应当看到且只看到 `-- Including Wayland support`（Windows 上是
`-- Including Win32 support`），不会再有 X11 相关提示。

## 依赖 <a name="compile_deps"></a> <a name="compat_guide"></a>

### Windows <a name="compat_win32"></a>

只要 **MSVC（2015 及以上）** 与 Windows SDK，没有第三方依赖。Win32 后端用到下面这些
系统能力，CMake 会自己接对应的导入库：

依赖 | 用途 | 缺失后果
---- | ---- | ----
Windows 7 及以上 | `SetProcessDpiAwareness` 等 API | 更老的系统不支持
`user32.dll` | 窗口、消息循环、输入 | 无法运行
`gdi32.dll` | 光标、显示模式、DIB 光标 | 链接失败
`XInput` | 手柄 | 手柄不可用
Raw Input | `PLFW_RAW_MOUSE_MOTION` | 该模式退化为普通鼠标
高 DPI API | 内容缩放（`plfwGetWindowContentScale`） | 缩放恒为 `1.0`
显示模式切换 | 全屏窗口 | 退化为窗口模式

Windows 上只有 Win32 后端，`PLFW_BUILD_WIN32=OFF` 会得到一个只剩 Null 的库。

### Linux（Wayland） <a name="compat_wayland"></a>

必须有：

- `wayland-client`、`wayland-cursor`（≥ 0.2.7）与 `xkbcommon`（≥ 0.5.0），
  pkg-config 名字就是这三个；
- `wayland-scanner`（`wayland` 包里的命令行工具），用来把 `deps/wayland/` 下的
  XML 生成客户端协议头；
- `libdecor` 可选：用于窗口装饰，缺失时退化为 PLFW 自带的简单装饰。

Ubuntu / Debian 一行装齐：

```sh
sudo apt install build-essential cmake ninja-build                  libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev
```

运行期还要看合成器提供了哪些协议。**必需**的六个：

协议 | 用途
---- | ----
`wl_compositor` / `wl_surface` | 创建表面
`wl_seat` | 输入设备入口；**没有 seat 时剪贴板与输入都不可用**
`wl_pointer` / `wl_keyboard` | 鼠标 / 键盘
`xdg_wm_base` | 顶层窗口、最大化、全屏、装饰触发
`wl_shm` | 光标图像与共享内存缓冲
`wl_output` | 显示器

**可选**的缺失时降级而不是报错：

协议 | 缺失后果
---- | ----
`xdg_decoration` | 无服务端装饰；用 libdecor 或 PLFW 自带装饰替代
`libdecor` | 装饰退化为 PLFW 自带的简单标题栏
`relative_pointer` | `PLFW_CURSOR_DISABLED` 退化为隐藏光标
`pointer_constraints` | 光标无法锁定在窗口内
`fractional_scale` | 分数缩放退化为整数缩放，`plfwGetWindowContentScale` 精度下降
`viewporter` | 全屏缩放路径退化
`idle_inhibit` | 无法阻止熄屏
`xdg_activation` | `plfwRequestWindowAttention` 无效
`wl_data_device` | 剪贴板与文件拖放都不可用（报 `PLFW_FEATURE_UNAVAILABLE`）

**明确不支持**的能力（调用会报 `PLFW_FEATURE_UNAVAILABLE` 或被忽略）：由客户端摆放窗口
（`plfwSetWindowPos`）、摆放光标（`plfwSetCursorPos`）、窗口图标、gamma、强制抢焦点；
显示器位置只能报合成器给的值。各条的具体表现写在窗口章与输入章的对应小节里。

本构建不依赖 XWayland，也没有 X11 后端：跑在 XWayland 下的程序会被当成 X11 客户端，
而本库没有 X11 支持，所以别把它当成回退路径。需要无显示环境时用 `PLFW_PLATFORM_NULL`。

### 可选工具

- Weston 或其它 Wayland 合成器：运行验证需要；CI 里可以用 `weston --headless`。

## 生成 Wayland 协议代码 <a name="compile_generate"></a>

`src/CMakeLists.txt` 对 `deps/wayland/` 下这 9 个 XML 逐个调用

```sh
wayland-scanner client-header <file>.xml <file>-client-protocol.h
wayland-scanner private-code  <file>.xml <file>-client-protocol-code.h
```

| 协议文件 | 用途 |
| -------- | ---- |
| `wayland.xml` | 核心协议（`wl_seat`、`wl_pointer`、`wl_data_device` 等） |
| `viewporter.xml` | 视口缩放 |
| `xdg-shell.xml` | 顶层窗口、最大化、全屏、装饰触发 |
| `idle-inhibit-unstable-v1.xml` | 阻止熄屏 |
| `pointer-constraints-unstable-v1.xml` | 光标锁定 |
| `relative-pointer-unstable-v1.xml` | 原始鼠标位移 |
| `fractional-scale-v1.xml` | 分数缩放 |
| `xdg-activation-v1.xml` | 请求注意 / 激活 |
| `xdg-decoration-unstable-v1.xml` | 客户端 / 服务端装饰协商 |

生成物落在构建目录，不污染源码树。**打包源码时必须带上这 9 个 XML**，否则配置
阶段就会报缺文件。

## 构建选项 <a name="compile_options"></a>

选项 | 默认值 | 说明
---- | ------ | ----
`BUILD_SHARED_LIBS` | `OFF` | 建动态库；见 [动态库](#compile_options_shared)
`PLFW_BUILD_WIN32` | Windows 上 `ON` | 编译 Win32 后端
`PLFW_BUILD_WAYLAND` | Unix 上 `ON` | 编译 Wayland 后端
`PLFW_BUILD_EXAMPLES` | 源码树为顶层时 `ON` | 构建 `examples/` 里的三个示例
`PLFW_BUILD_TESTS` | 源码树为顶层时 `ON` | 构建 `tests/smoke.c`
`PLFW_USE_HYBRID_HPG` | `OFF` | 只在 Win32 下有意义：强制高性能 GPU；本构建无上下文，
| | | 该选项保留但基本无效果
`USE_MSVC_RUNTIME_LIBRARY_DLL` | `ON` | MSVC 下使用动态 CRT

X11 / Cocoa 相关的选项（`PLFW_BUILD_X11`、`PLFW_BUILD_COCOA`，以及更早的
`GLFW_USE_OSMESA`、`GLFW_USE_WAYLAND`）**都不存在**：传了不会被识别，CMake 只会
把它们标注为“未被工程使用”。

### 动态库 <a name="compile_options_shared"></a>

```sh
cmake -S . -B build-shared -DBUILD_SHARED_LIBS=ON
```

产物是 `plfw3.dll` + `plfw3dll.lib`（Windows）或 `libplfw.so.3.5`（Linux，SONAME
`libplfw.so.3`）。Windows DLL 会自动生成 `.rc` 版本资源。用 DLL 的应用必须定义
`PLFW_DLL`，CMake 的 `plfw` 目标会以 `INTERFACE` 方式自动加上它。

### Windows 专用选项 <a name="compile_options_win32"></a>

- `USE_MSVC_RUNTIME_LIBRARY_DLL=OFF`：静态 CRT，通常只在你确定整条依赖链都静态
  链接时用。
- 加固（DEP、ASLR、高熵 ASLR）由 MSVC 链接器默认开启，仓库不提供任何开关。

### Unix 专用选项 <a name="compile_options_unix"></a>

- `PLFW_BUILD_WAYLAND=OFF`：得到一个只有 Null 后端的库，用于纯后台或无显示
  的 CI 环境。
- `WAYLAND_SCANNER_EXECUTABLE`：手动指定 `wayland-scanner` 路径。

### 交叉编译 <a name="compile_cross"></a>

**不支持**从 Linux 往 Windows 交叉编译：Windows 目标必须由 MSVC 产出，仓库里也没有
mingw-w64 工具链文件。要在 Linux 上验证 Windows 构建质量，请在 Windows 机器或
Windows CI 上跑 MSVC。

## 编辑器集成（VS Code / clangd） <a name="compile_editor"></a>

CMake 只在**单配置生成器**上写 `compile_commands.json`：Ninja 与 Makefile 都会自动
生成（本仓库 `CMakeLists.txt` 已为独立构建打开 `CMAKE_EXPORT_COMPILE_COMMANDS`），
**Visual Studio 生成器不会**——它生成 `.sln`/`.vcxproj`，编辑器读不到编译参数。

所以做代码补全时额外开一个 Ninja 构建目录，和 `.sln` 构建目录并存：

```bat
:: Windows，在 "x64 Native Tools Command Prompt" 里（或先 call vcvars64.bat）
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
```

VS 自带的 `ninja.exe` 在 `Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\` 下，
不在 PATH 时把它加进去，或者用 VS Code 的 CMake Tools 扩展指定的 generator。

```sh
# Linux
cmake -S . -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
```

配置完成后 `build-ninja/compile_commands.json` 就是完整的编译数据库（含
`-D_PLFW_WIN32` / `-D_PLFW_WAYLAND`、include 路径、警告选项）。改了 `CMakeLists.txt`
或增删源文件后重新跑一次 `cmake -S . -B build-ninja` 即可刷新，不用重新构建。

VS Code 两种客户端任选其一（`.vscode/settings.json` 已被 `.gitignore` 忽略，属本地配置）：

```json
{
    "C_Cpp.default.compileCommands": "${workspaceFolder}/build-ninja/compile_commands.json",
    "clangd.arguments": ["--compile-commands-dir=${workspaceFolder}/build-ninja"]
}
```

`clangd` 还需要知道这是 C 项目、按 C99 解析；本仓库所有源文件都是 `.c`，默认即可。
只想看单平台的话，在 Ninja 目录上再传一次 `-DPLFW_BUILD_WAYLAND=OFF` 之类即可，
数据库会跟着变。

## 手动编译 <a name="compile_manual"></a>

不想用 CMake 时，自己按源码树分层挑文件。`src/` 的布局就是手动编译的分组依据：

```text
src/
├── internal.h  platform.h            内部结构与平台层声明
├── mappings.h                        内置手柄映射（直接随源码编译）
├── plfw.rc.in                        Windows DLL 版本资源模板
├── init.c  input.c  monitor.c  window.c   公共 API 实现（后端无关）
├── platform.c                        平台表：后端选择
├── posix/            posix_time.* posix_thread.* posix_module.c posix_poll.*
│                     非 Windows 的 OS 原语层（Wayland 与 Null 共用）
├── null/             null_init.c null_monitor.c null_window.c
│                     null_joystick.c/h  null_platform.h
├── win32/            win32_init.c win32_monitor.c win32_window.c
│                     win32_joystick.c/h  win32_platform.h
│                     win32_time.* win32_thread.* win32_module.c
└── wayland/          wl_init.c wl_monitor.c wl_window.c wl_platform.h
                      linux_joystick.c/h
```

**公共核心**（两个后端都要）：

```text
src/init.c  src/input.c  src/monitor.c  src/platform.c  src/window.c
src/null/null_init.c  src/null/null_monitor.c
src/null/null_window.c  src/null/null_joystick.c
src/internal.h  src/platform.h  src/mappings.h   （头文件）
```

再加一套 OS 层（`src/posix/` 与 `src/win32/` 是 OS 原语，不是后端）：

```text
Windows： src/win32/win32_time.c src/win32/win32_thread.c src/win32/win32_module.c
Linux：   src/posix/posix_time.c src/posix/posix_thread.c src/posix/posix_module.c
```

Win32 后端（宏 `_PLFW_WIN32`、`UNICODE`、`_UNICODE`、`_CRT_SECURE_NO_WARNINGS`）：

```text
src/win32/win32_init.c src/win32/win32_joystick.c
src/win32/win32_monitor.c src/win32/win32_window.c
```

链接：`-lgdi32`（其余系统库由默认链接补上）。

Wayland 后端（宏 `_PLFW_WAYLAND`、`_DEFAULT_SOURCE`，Linux 上另有
`_PLFW_LINUX` 语义的 `linux_joystick.c`）：

```text
src/wayland/wl_init.c src/wayland/wl_monitor.c src/wayland/wl_window.c
src/wayland/linux_joystick.c src/posix/posix_poll.c
deps/wayland/*.xml  → wayland-scanner 生成的 18 个头/源文件
```

链接：`-lwayland-client -lwayland-cursor -lxkbcommon -lpthread -ldl -lrt -lm`，
并按 `HAVE_MEMFD_CREATE` 决定是否定义该宏（不定义也能编，只是少一条优化路径）。

手动编译示例（Linux）：

```sh
gcc -c -std=c99 -D_DEFAULT_SOURCE -D_PLFW_WAYLAND -Iinclude -Isrc \
    src/*.c src/null/*.c src/posix/*.c src/wayland/*.c build/wl-proto/*.c
gcc my_program.o libplfw_objects.a -lwayland-client -lwayland-cursor \
    -lxkbcommon -lpthread -ldl -lrt -lm -o my_program
```

**注意**：手动编译必须把 `src/platform.c` 一起带上，它负责后端选择（平台表），漏掉
它会在链接期报 `plfwInit` 未定义。

## 在自己的程序里使用 <a name="build_guide"></a>

这一半讲怎么在自己的程序里用上这个库：包含什么头、宏是什么、链接要加哪些库。

### 包含头文件 <a name="build_include"></a>

```c
#include <PLFW/plfw3.h>
```

头文件里只有声明、常量与不透明句柄，**不引入任何 GL / EGL / Vulkan / OSMesa
头**，也不会定义“要不要包含 GL 头”那类开关（这类宏在这个构建里不存在）。所以可以和 D3D、
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

### 编译宏 <a name="build_macros"></a>

宏 | 用途
--- | ----
`PLFW_DLL` | 链接 DLL 版本的 PLFW 时定义（CMake 的 `plfw` 目标会自动传播）
`PLFW_EXPOSE_NATIVE_WIN32` | 在 `plfw3native.h` 里暴露 Win32 句柄访问器
`PLFW_EXPOSE_NATIVE_WAYLAND` | 在 `plfw3native.h` 里暴露 Wayland 句柄访问器

库本身用 `_PLFW_WIN32` / `_PLFW_WAYLAND` 选择后端，这两个宏由构建系统定义，不要
在应用里手动定义。

### 链接 <a name="build_link"></a>

静态库（本构建默认）在 Windows 上需要额外链 `gdi32`：

- Windows：`plfw3.lib`（MSVC，唯一支持的工具链）
- Linux：`libplfw3.a`，另需 `wayland-client`、`wayland-cursor`、`xkbcommon`、
  `dl`、`rt`、`m`

用 CMake 的源码构建时，`plfw` 目标会处理平台依赖；手动链接时则按上面的平台依赖列表添加。

#### Windows（Visual Studio） <a name="build_link_win32"></a>

把 `plfw3.lib` 加进“附加依赖项”，再补系统库：

```text
plfw3.lib
gdi32.lib
```

调试版与发布版 CRT 必须一致：用 CMake 构建时选 `USE_MSVC_RUNTIME_LIBRARY_DLL`
等于 `ON`（默认）就是动态 CRT。DLL 版还要链接 `plfw3dll.lib` 并在代码里定义
`PLFW_DLL`。

#### 用 CMake 的源码构建 <a name="build_link_cmake_source"></a>

```cmake
set(PLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/plfw)

target_link_libraries(my_app PRIVATE plfw)
```

`add_subdirectory` 会把 `plfw` 目标加到同一构建里，包含目录与依赖库自动传播。
`PLFW_BUILD_TESTS` / `PLFW_BUILD_EXAMPLES` 只在顶层构建时默认 `ON`，作为子目录
引入时本来就关着；想更彻底可以把它们显式设为 `OFF`。

## 源码树与内部接口 <a name="internals_layout"></a>

这一节的读者是要改这个库的人。源码树的分层见上一节里的目录树，三条规则：根目录的文件**不得包含**任何后端头文件；
后端之间互不包含；`platform.h` 是唯一同时认识所有后端的地方。新增一个后端 = 新建
`src/<后端>/` 目录，在 `platform.h` 里加状态宏与 include 分支，在 `src/CMakeLists.txt`
里加一段 `target_sources`。

### 公共接口 <a name="internals_public"></a>

`include/PLFW/plfw3.h` 是唯一的公共头，`include/PLFW/plfw3native.h` 暴露原生句柄。
所有导出符号都以 `PLFWAPI` 修饰（`__declspec(dllexport)` 或可见性属性），并在
`src/*.c` 里定义。公共头只依赖 `<stddef.h>`、`<stdint.h>`、`<stdarg.h>` 这类 C 标准
头与平台基础类型，不引入任何图形 API 头。

### 原生接口 <a name="internals_native"></a>

`plfw3native.h` 的实现散在 `src/win32/win32_window.c`（`plfwGetWin32Window`）与
`src/wayland/wl_window.c`（`plfwGetWaylandDisplay` / `plfwGetWaylandWindow`）里。每个访问器
都只在对应后端编译时才存在，所以缺后端时是链接错误而不是运行期 `NULL`——这是有意的。

### 内部接口 <a name="internals_internal"></a>

`src/internal.h` 定义所有内部结构：

- `_PLFWwindow`：原生句柄、输入状态、回调表、尺寸与缩放缓存、用户指针；
- `_PLFWmonitor`、`_PLFWvidmode`、`_PLFWcursor`；
- `_PLFWjoystick`：轴 / 按键 / 帽的缓冲区与映射；
- `_PLFWlibrary`：全局单例，含平台表、错误状态、计时器、手柄数组；
- `_PLFWallocator` 与 `_PLFW_REQUIRE_INIT` 之类的辅助宏。

**没有** `_GLFWcontext`、`_GLFWctxconfig`，也没有 `window->context`：这是它和上游
最大的结构差异，任何“给窗口挂上下文”的代码都不该存在。

### 平台层 <a name="internals_platform"></a>

`src/platform.h` 用 `_PLFW_WIN32` / `_PLFW_WAYLAND` /（Null 永远可用）定义
`_PLFW_PLATFORM_*` 状态宏，并把对应后端的头文件 include 进来。

`src/platform.c` 里有一张平台表（`_PLFWplatform` 结构数组），每项提供
`init` / `terminate` / `createWindow` / `destroyWindow` / `pollEvents` 等函数指针，
`plfwInit` 按 `PLFW_PLATFORM` 提示与 `plfwPlatformSupported` 的结果选一个。

- 平台表只剩 Win32、Wayland、Null 三项；
- 平台 ID 只有 `PLFW_ANY_PLATFORM`、`PLFW_PLATFORM_WIN32`、`PLFW_PLATFORM_WAYLAND`、
  `PLFW_PLATFORM_NULL` 四个；X11 / Cocoa 的常量已删除，它们的旧数值现在按未知 ID
  处理（`plfwInitHint` → `PLFW_INVALID_ENUM`，`plfwPlatformSupported` →
  `PLFW_FALSE`）。

### 事件接口 <a name="internals_event"></a>

后端在自己的消息循环里调用 `_plfwInput*` 系列函数（`_plfwInputKey`、`_plfwInputMouseClick`、
`_plfwInputCursorPos`、`_plfwInputWindowSize`、`_plfwInputWindowCloseRequest` 等），
由 `src/input.c` 与 `src/window.c` 统一维护状态并派发回调。加一个新后端只需实现
平台表里的函数，事件上报一律走这些 `_plfwInput*`，不要直接调用户回调。

`plfwPollEvents` → 平台表 `pollEvents` → 后端读原生事件 → `_plfwInput*`。阻塞版本
`plfwWaitEvents` 在 Linux 上靠 `posix/posix_poll.c` 的 `poll()`，Windows 上靠
`MsgWaitForMultipleObjects`；`plfwPostEmptyEvent` 通过一对内部事件 / pipe 唤醒。

### 配置宏 <a name="internals_config"></a>

宏 | 作用
--- | ----
`_PLFW_WIN32` | 编译 Win32 后端
`_PLFW_WAYLAND` | 编译 Wayland 后端
`_PLFW_BUILD_DLL` | 目标定义为 DLL（由 CMake 设置）
`HAVE_MEMFD_CREATE` | Linux 上用 `memfd_create` 分配匿名缓冲（自动探测）
`_PLFW_USE_HYBRID_HPG` | Win32 强制高性能 GPU（对应 `PLFW_USE_HYBRID_HPG` 选项）
`_CRT_SECURE_NO_WARNINGS` | MSVC 下屏蔽 CRT 弃用警告
`_DEFAULT_SOURCE` | Linux 上用 glibc 的 POSIX 扩展（`poll` 等）

Null 后端没有自己的宏：它总是被编译进去，只在显式请求 `PLFW_PLATFORM_NULL` 时启用。

### 测试与代码约定 <a name="internals_static"></a>

- `src/mappings.h` 是仓库内置的 SDL_GameControllerDB 映射，直接参与编译；正常构建
  不会下载或生成映射文件。
- `tests/smoke.c` 是唯一的自动化测试，随 `ctest` 跑；它断言 X11 / Cocoa 后端不在
  构建里，防止误把后端加回来。
- 编译时 `-Wall` / `/W3` 默认打开；本仓库要求零警告。
