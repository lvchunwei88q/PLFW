# PLFW 代码审查报告

- 审查基线：`b592583`（工作区干净）
- 审查日期：2026-11-13
- 审查范围：一是把 GLFW 3.5.1 裁剪成纯窗口/输入/事件库时（删图形上下文、删 X11 与 Cocoa 后端）遗留的问题；二是改名、Wayland 空白 buffer、smoke 剪贴板适配、示例链接修复、`internal.h` 清理这几轮改动引入的问题。
- 审查方式：全库静态扫描（孤儿声明、死字段、GL 残留、CMake 选项残留）+ 公开 API 与库导出符号对账 + 构建与运行验证矩阵（Windows 静态/动态，Linux Wayland / Null / 共享库）。
- 本文只记录问题与建议，不含代码改动。

## 结论摘要

| 级别 | 数量 | 说明 |
| --- | --- | --- |
| P1（会导致错误结果或崩溃） | 0 | 未发现 |
| P2（使用者可感知的行为偏差 / 掩盖真实缺陷） | 2 | 透明窗口语义失真；smoke 覆盖不到上屏链路 |
| P3（健壮性、整洁性、文档一致性） | 9 | 见下文 |

构建与符号层面已交叉验证通过的部分见第四节，可以视为已排除的方向。

---

## 一、P2 级

### P2-1 Wayland 上「透明窗口」不再透明

> 处理状态：已按决议**整体移除透明帧缓冲能力**（提示、属性、三后端字段与分支、Win32 的 DWM 模糊路径全部删除，窗口恒不透明），本条保留作为历史记录。

- 位置：`src/wl_window.c:236-247`（把 buffer 填成 `0x20,0x20,0x20,0xff`，alpha 固定 255）、`src/wl_window.c:255-285`（attach）、`src/wl_window.c:573-574`（`transparent` 时跳过 `setContentAreaOpaque`）。
- 现象：调用方 `plfwWindowHint(PLFW_TRANSPARENT_FRAMEBUFFER, PLFW_TRUE)` 后，`plfwGetWindowAttrib(window, PLFW_TRANSPARENT_FRAMEBUFFER)` 返回 `PLFW_TRUE`，但窗口实际是一整块不透明深灰，看不到桌面透出。
- 原因：这块空白 buffer 的 alpha 恒为 `0xff`，与 `transparent` 分支"不设置 opaque region"的意图矛盾——opaque region 是给合成器的提示，真正决定透明与否的是像素 alpha。
- 影响：Win32（`win32_window.c:1475` 走 DWM）与 Null（`null_window.c:103` 只记状态）都不受影响；Wayland 上透明语义失效。这一条由空白 buffer 改动引入，之前窗口根本不上屏，所以没被暴露。
- 建议：按 `window->wl.transparent` 选填充值——不透明时 `0xff`，透明时填 `0x00`（保持映射与可聚焦），或者至少让文档明确"Wayland 下透明窗口只是可映射、内容不透明"。

### P2-2 smoke 只测隐藏窗口，覆盖不到「上屏—聚焦—剪贴板」这条链路

- 位置：`tests/smoke.c:124-125`（`plfwWindowHint(PLFW_VISIBLE, PLFW_FALSE)` 后创建窗口）、`tests/smoke.c:217-240`（剪贴板读回为空即跳过）。
- 现象：Wayland 下 smoke 永远输出 `skip clipboard check (Wayland: No clipboard data available)`，窗口可见性只断言"不可见"。
- 影响：这正是上一轮真实缺陷（窗口从不 attach buffer → 不上屏 → 拿不到焦点 → 剪贴板无 data offer）所在的链路。若同类缺陷再次出现，smoke 仍然全绿——测试无法区分"平台限制"和"我们的 bug"。目前这条只靠人工在 WSLg 里跑 `multi-window` 才被发现。
- 建议：拆成两段测试：隐藏窗口跑现有能力检查；再开一个可见窗口，`plfwShowWindow` + 泵事件若干轮，断言 `PLFW_VISIBLE == PLFW_TRUE` 且拿到焦点后再做剪贴板往返。Wayland 上"隐藏窗口读不回剪贴板"确实是协议语义，但"可见且聚焦"的窗口读不回就是缺陷。

---

## 二、P3 级

### P3-1 空白 buffer 的尺寸与 `fbWidth/fbHeight` 不一致（分数缩放）

- 位置：`src/wl_window.c:260-262`（buffer 尺寸 = `wl.width * bufferScale`）对比 `src/wl_window.c:559-571`（`fbWidth = (wl.width * scalingNumerator + 60) / 120`）。
- 现象：整数缩放（`bufferScale = 2`）两者一致；但 `fractionalScale` 生效时，报告的 framebuffer 尺寸按 120 分之一格算，实际 buffer 仍按 `bufferScale` 算，两者可能不相等（例如 1.75 倍：`fbWidth` 1.75×，buffer 1×）。
- 影响：视觉上仍是覆盖整个窗口的灰块（`wp_viewport` 会把 buffer 放大到目标尺寸），但 `plfwGetFramebufferSize` 报的尺寸与实际 buffer 不符；将来接真实渲染内容时会变成画面与尺寸对不上的隐性 bug。
- 建议：buffer 直接用 `window->wl.fbWidth/fbHeight`，与 framebuffer 尺寸共用同一套换算。

### P3-2 先销毁旧 buffer 再创建新 buffer，失败即丢内容；pool 未判空

- 位置：`src/wl_window.c:267-277`（`wl_buffer_destroy` 之后才 `createBlankBuffer`）、`src/wl_window.c:232-247`（`wl_shm_create_pool` 的返回值直接传给 `wl_shm_pool_create_buffer`，没有判空）。
- 影响：`createBlankBuffer` 失败（匿名文件创建/mmap 失败）时，旧 buffer 已经被销毁，surface 变成无内容；`wl_shm_create_pool` 返回 NULL 时是空指针解引用而不是干净报错。
- 建议：新 buffer 创建成功后再销毁旧的；`wl_shm_create_pool` 判空并走已有的 `_plfwInputError(PLFW_PLATFORM_ERROR, ...)` 路径。

### P3-3 每次尺寸变化都重建 shm 文件；内存常驻且隐藏不释放

- 位置：`src/wl_window.c:267-277`、`src/wl_window.c:2471`（仅析构时释放）。
- 影响：拖动改变窗口大小时每一步 configure 都可能重建一次匿名文件与映射（每像素 4 字节，1920×1080 约 8 MB），隐藏窗口也保留这份内存。功能正确，但属于可感知的资源抖动；多窗口 + 大尺寸时内存占用是可以算出来的。
- 建议：可以只在可见且尺寸变化时重建（现状），也可以给 buffer 加容量上限或按需延迟重建；若要保守，至少在文档里写明"每个窗口常驻一块窗口大小的 shm 内存"。

### P3-4 libdecor 路径未考虑内容区 insets

- 位置：`src/wl_window.c:1012-1020`（`libdecorFrameHandleConfigure` 尾部同样调 `attachBlankBuffer`）。
- 影响：`attachBlankBuffer` 一律用整个 surface 尺寸铺满。走 libdecor 时内容区应扣掉装饰边距，buffer 会盖住标题栏区域。本项目默认未编译 libdecor（需要单独依赖），属条件性问题，但一旦启用就会出问题。

### P3-5 文档关于「显示器位置恒为 0,0」与实现不符

- 位置：`docs/compat.md:54`、`docs/monitor.md:22`、`docs/monitor.md:80` 都写"Wayland 恒为 `0,0`"；实现是 `src/wl_monitor.c:54-55` 从 `wl_output.geometry` 的 `x/y` 取值，`src/wl_monitor.c:199-205` 原样返回。
- 现象：协议里这两个字段由合成器给，多数合成器发 0，但不保证（多输出布局的合成器可能发非零）。
- 影响：文档把"通常"写成了"恒为"，与代码语义不一致；`plfwGetWindowPos` 那条（`plfwGetWindowPos` 先清零再调后端）反而是准确的。
- 建议：改成"Wayland 协议不向客户端暴露全局布局，实际多为 `0,0`，以合成器 `wl_output.geometry` 上报为准"。

### P3-6 smoke 里的恒真断言、弱断言与条件格式串

- 恒真：`tests/smoke.c:259`（`check("joystick enumeration ran without error", PLFW_TRUE)`）、`:271`、`:280`、`:283` —— 断言参数写死 `PLFW_TRUE`，永远通过，等于没测。
- 恒真范围：`tests/smoke.c:151`（`left >= 0 && top >= 0`，两个变量初始化为 0，接口不会返回负数）。
- 弱断言：`tests/smoke.c:168`（`cursorX > -1.0 || cursorY > -1.0`，只要一个坐标不为初值就通过；改成两个都校验更严格）。
- 条件格式串：`tests/smoke.c:285`（`printf(failures ? "...%d..." : "...", failures)`），分支之一没有 `%d` 却仍传参，属 `-Wformat` 类工具容易报出来的写法，建议拆成 `if/else` 两个 `printf`。
- 影响：测试可信度打折，属于"看起来有覆盖、实际没有"的典型。

### P3-7 `examples` / `tests` 使用目录级 `link_libraries()` 与 `add_definitions()`

- 位置：`examples/CMakeLists.txt:7,10,15`、`tests/CMakeLists.txt:6,9,14`。
- 影响：目录级作用域会把链接依赖和宏泄漏到该目录之后新增的所有目标，后续加示例/测试时会莫名其妙多出依赖；`add_definitions(-D_CRT_SECURE_NO_WARNINGS)` 同理。当前只有 2+1 个目标，尚未出问题。
- 建议：改成每个 `add_executable` 之后配 `target_link_libraries(... PRIVATE plfw)` 与 `target_compile_definitions(...)`。

### P3-8 示例在 Wayland 上必然刷出 6 条 `PLFW_FEATURE_UNAVAILABLE`

- 位置：`examples/multi-window.c:116`（建窗后 `plfwGetWindowPos`）、`examples/multi-window.c:256`（`plfwSetWindowPos` 排版）。
- 现象：Wayland 上每个窗口两条错误（取值不支持 + 设置不支持），三个窗口共 6 条，混在正常输出里。
- 影响：功能没错（Wayland 本来就不让客户端定位），但示例的输出噪音会让人误以为出错。
- 建议：示例里按 `plfwGetPlatform() == PLFW_PLATFORM_WAYLAND` 跳过定位相关的调用，或先把错误回调静音再打印一行说明。

### P3-9 `_PLFWfbconfig` 的名字与现在的语义已经不匹配

- 位置：`src/internal.h:158-167`（如今只剩 `redBits`/`greenBits`/`blueBits`/`transparent` 四个字段，用途是"标称视频模式位深 + 透明标志"，不再描述任何 framebuffer）。
- 影响：结构体名与注释在本次清理后已经解释了用途，但名字仍叫 fbconfig，读代码的人会以为还有真实 framebuffer 概念。
- 建议：若后续还要动这块，可顺手改名（如 `_PLFWnominalVideoConfig`）；纯命名问题，不改也不影响行为。

---

## 三、我这几轮改动的风险评估（按改动归类）

| 改动 | 结论 | 关联问题 |
| --- | --- | --- |
| 整体改名（`fb9de7c`） | 无遗留：头文件 110 个公开函数全部有实现，库导出 113 个（含 3 个 Wayland native 访问器），`src` 内 GL/EGL/OSMesa/pixelformat 残留 0 处 | — |
| Wayland 空白 buffer（`bff384a`） | 逻辑方向正确（无 buffer 就不会被 map，这是投递协议事实），但有 4 个遗留边角 | P2-1、P3-1、P3-2、P3-3、P3-4 |
| smoke 剪贴板跳过（`fb4a61a`） | 单次运行更"绿"，但把"平台限制"和"缺陷"混为一谈 | P2-2 |
| 示例链接修复（`50aa6e5`） | 修得对（`__has_include` 只能看到头文件，看不到库是否真编了该后端） | — |
| `internal.h` 清理（`b592583`） | 删的是真死代码；`redBits/greenBits/blueBits` 与 `transparent` 都被读，所以保留了 `createWindow` 的 `fbconfig` 参数是对的 | P3-9（命名） |

---

## 四、已验证、可以排除的方向

| 检查项 | 方法 | 结果 |
| --- | --- | --- |
| 公开 API 声明与实现是否一一对应 | 头文件提取函数名 vs 库导出符号对账 | 110 个声明全部有实现，0 缺失；多出的 3 个是 `plfw3native.h` 的 Wayland 访问器 |
| 删除图形层是否留下 GL 代码 | 全库扫 `_PLFWcontext`/`GLRC`/`EGLContext`/`pixelFormat`/`SetPixelFormat` | 0 命中 |
| 是否还有"只有声明没有定义"的内部函数 | 头文件 `_plfw*` 声明 vs `.c` 出现次数 | 0（上一轮清掉的两条是最后的中招者） |
| Win32 窗口状态结构是否残留图形字段 | 读 `win32_platform.h:217-240` | 干净 |
| `PLFW_TRANSPARENT_FRAMEBUFFER` 是否有真实消费者 | 三后端 `transparent` 读取点 + `plfwGetWindowAttrib` | 有：Win32 DWM 分支、Wayland opaque region 分支、Null 属性 |
| CMake 是否残留已删后端的选项 | 扫 `X11`/`COCOA`/`OSMESA`/`VULKAN` | 只保留了"旧选项就报 FATAL_ERROR 并提示新选项"的守卫（`CMakeLists.txt:17-24`），属有意设计 |
| 构建矩阵 | Windows 静态 + `BUILD_SHARED_LIBS=ON`；Linux Wayland / `PLFW_BUILD_WAYLAND=OFF` / 共享库 | 全部 0 警告 0 错误，smoke 全过 |

---

## 五、建议的修复顺序

1. **P2-2**（测试覆盖）：它决定了后面所有 Wayland 类问题能否被自动发现，优先级实际高于其余几条。
2. **P2-1**（透明语义）：一行填充值的事，但直接影响 API 承诺。
3. **P3-1 / P3-2**（buffer 尺寸换算、创建顺序与判空）：都在同一个函数里，一起改最省事。
4. **P3-5 / P3-6 / P3-7 / P3-8**（文档措辞、测试断言、CMake 作用域、示例降噪）：纯整洁性，可以合并成一次提交。
5. **P3-3 / P3-4 / P3-9**（内存策略、libdecor、命名）：按需，暂不影响正确性。
