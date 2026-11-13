# 编译指南 <a name="compile_guide"></a>

本仓库用 CMake 构建，源码树里带 `CMakeLists.txt`、`deps/wayland/` 的协议 XML 与
`docs/`。用 CMake 是最省事的方式，手动编译也在最后一节给出。

## 用 CMake 构建 <a name="compile_cmake"></a>

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Windows + MinGW 时把 `-G Ninja` 换成 `-G "MinGW Makefiles"` 也可以；Visual Studio
直接 `-G "Visual Studio 17 2022" -A x64`。构建产物：

- `build/src/libplfw3.a`（Windows/MinGW）或 `libplfw3.a`（Linux）
- `build/tests/smoke.exe`、`build/examples/window-input.exe`、
  `build/examples/multi-window.exe`（`PLFW_BUILD_TESTS` / `PLFW_BUILD_EXAMPLES`
  打开时）

想跑一遍自检：

```sh
ctest --test-dir build
```

`tests/smoke.c` 是不依赖图形上下文的冒烟测试：初始化、建窗、查询属性、收事件、退出，
并在 Wayland 上跳过需要的合成器能力。

## 依赖 <a name="compile_deps"></a>

### Windows

只要一个 C99 编译器（MSVC 2015+ / MinGW-w64）与 Windows SDK，没有第三方依赖。
Win32 后端用到 `user32`、`gdi32`、`dwmapi`、`shell32`、`winmm` 等系统库，CMake 会
自己接。

### Linux（Wayland）

必须有：

- `wayland-client`、`wayland-cursor`（≥ 0.2.7）与 `xkbcommon`（≥ 0.5.0），
  pkg-config 名字就是这三个；
- `wayland-scanner`（`wayland` 包里的命令行工具），用来把 `deps/wayland/` 下的
  XML 生成客户端协议头；
- `wlroots`/`libdecor` 可选：`libdecor` 用于窗口装饰，缺失时退化为 PLFW 自带的
  简单装饰。

Ubuntu / Debian 一行装齐：

```sh
sudo apt install build-essential cmake ninja-build                  libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev
```

协议头文件由 `wayland-scanner` 生成，见下文生成 Wayland 协议代码。

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

## 编译 <a name="compile_compile"></a>

最小构建（只要库）：

```sh
cmake -S . -B build -DPLFW_BUILD_TESTS=OFF -DPLFW_BUILD_EXAMPLES=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

只用 Wayland、不带 Win32 的交叉转发场景（在 Linux 上）：

```sh
cmake -S . -B build -DPLFW_BUILD_WAYLAND=ON -DCMAKE_BUILD_TYPE=Release
```

构建过程中你应当看到且只看到 `-- Including Wayland support`（Windows 上是
`-- Including Win32 support`），不会再有 X11 相关提示。

## 构建选项 <a name="compile_options"></a>

选项 | 默认值 | 说明
---- | ------ | ----
`BUILD_SHARED_LIBS` | `OFF` | 建动态库；见 [动态库](#compile_options_shared)
`PLFW_BUILD_WIN32` | Windows 上 `ON` | 编译 Win32 后端
`PLFW_BUILD_WAYLAND` | 非 Apple 的 Unix 上 `ON` | 编译 Wayland 后端
`PLFW_BUILD_EXAMPLES` | 源码树为顶层时 `ON` | 构建 `examples/` 里的两个示例
`PLFW_BUILD_TESTS` | 源码树为顶层时 `ON` | 构建 `tests/smoke.c`
`PLFW_INSTALL` | `ON` | 生成 `install` 目标与 CMake 包配置
`PLFW_USE_HYBRID_HPG` | `OFF` | 只在 Win32 下有意义：强制高性能 GPU；本构建无上下文，
| | | 该选项保留但基本无效果
`USE_MSVC_RUNTIME_LIBRARY_DLL` | `ON` | MSVC 下使用动态 CRT

`GLFW_BUILD_X11`、`GLFW_BUILD_COCOA` **已删除**，传了也不会被识别（CMake 会提示
“未被工程使用”）。

### 动态库 <a name="compile_options_shared"></a>

```sh
cmake -S . -B build-shared -DBUILD_SHARED_LIBS=ON
```

产物是 `plfw3.dll` + `plfw3dll.lib`（Windows）或 `libplfw.so.3`（Linux）。Windows
DLL 会自动生成 `.rc` 版本资源。用 DLL 的应用必须定义 `PLFW_DLL`，CMake 的 `plfw`
目标会以 `INTERFACE` 方式自动加上它。

### Windows 专用选项 <a name="compile_options_win32"></a>

- `USE_MSVC_RUNTIME_LIBRARY_DLL=OFF`：静态 CRT，通常只在你确定整条依赖链都静态
  链接时用。
- 用 MinGW 建 DLL 时 CMake 会去掉 `lib` 前缀、把导入库改名为 `.dll.a`，还会自动
  探测并启用 `--nxcompat`、`--dynamicbase`、`--high-entropy-va` 三项加固。

### Unix 专用选项 <a name="compile_options_unix"></a>

- `PLFW_BUILD_WAYLAND=OFF`：得到一个只有 Null 后端的库，用于纯后台或无显示
  的 CI 环境。
- `CMAKE_DISABLE_FIND_PACKAGE_X11=TRUE`：本构建**完全不查 X11**，这个变量传了
  也只是被 CMake 标注为“未被工程使用”，不会报错。
- `WAYLAND_SCANNER_EXECUTABLE`：手动指定 `wayland-scanner` 路径。

### MinGW 交叉编译 <a name="compile_mingw_cross"></a>

从 Linux 上给 Windows 编（需要 `mingw-w64`）：

```sh
cmake -S . -B build-win -DCMAKE_SYSTEM_NAME=Windows \
      -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
      -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
cmake --build build-win -j
```

产物 `libplfw3.a` 可以直接给 Windows 上的 MinGW 或 MSVC（用 `.a` 需转换，推荐在
Windows 上重新构建）。

## 手动编译 <a name="compile_manual"></a>

不想用 CMake 时，自己指定源文件与宏。**公共核心**（两个后端都要）：

```text
src/context?      —— 不存在，已删除
src/init.c  src/input.c  src/monitor.c  src/platform.c  src/window.c
src/null_init.c  src/null_monitor.c  src/null_window.c  src/null_joystick.c
src/platform.h  src/internal.h  src/mappings.h   （头文件）
```

再加一套 OS 层：

```text
Windows： src/win32_time.c src/win32_thread.c src/win32_module.c
Linux：   src/posix_time.c src/posix_thread.c src/posix_module.c
```

Win32 后端（宏 `_PLFW_WIN32`、`UNICODE`、`_UNICODE`、`_CRT_SECURE_NO_WARNINGS`）：

```text
src/win32_init.c src/win32_joystick.c src/win32_monitor.c src/win32_window.c
```

链接：`-lgdi32`（其余系统库由默认链接补上）。

Wayland 后端（宏 `_PLFW_WAYLAND`、`_DEFAULT_SOURCE`，Linux 上另有
`_PLFW_LINUX` 语义的 `linux_joystick.c`）：

```text
src/wl_init.c src/wl_monitor.c src/wl_window.c
src/linux_joystick.c src/posix_poll.c
deps/wayland/*.xml  → wayland-scanner 生成的 18 个头/源文件
```

链接：`-lwayland-client -lwayland-cursor -lxkbcommon -lpthread -ldl -lrt -lm`，
并按 `HAVE_MEMFD_CREATE` 决定是否定义该宏（不定义也能编，只是少一条优化路径）。

手动编译示例（Linux）：

```sh
gcc -c -std=c99 -D_DEFAULT_SOURCE -D_PLFW_WAYLAND \
    -Iinclude -Isrc src/*.c build/wl-proto/*.c
gcc my_program.o libplfw_objects.a -lwayland-client -lwayland-cursor \
    -lxkbcommon -lpthread -ldl -lrt -lm -o my_program
```

**注意**：手动编译必须把 `src/platform.c` 一起带上，它负责后端选择（平台表），漏掉
它会在链接期报 `plfwInit` 未定义。
