---
title: Wayland 客户端协议 —— 从 unix socket 通信到 struct app 字段精解
tags: [wayland, protocol, xdg-shell, wl_surface, wl_registry, unix-socket, scm-rights, native-client]
desc: 自底向上讲透 Wayland 客户端的三件事：(1) 与 compositor 的 unix socket 通信总体架构 (2) 核心协议对象的层次依赖 (3) 把这些对象映射到一个真实 C 客户端 (tspi-greet) 的 struct app 字段并按依赖序逐字段精解
update: 2026-05-22

---


# Wayland 客户端协议：从 unix socket 通信到 struct app 字段精解

> [!note]
> **Ref:**
> - 实践靶标：[`prj/05-GraphStack/tspi-greet/src/main.c`](../../../prj/05-GraphStack/tspi-greet/src/main.c) (本笔记的 struct app 走读基准)
> - 对照实现：[`prj/05-GraphStack/tspi-greet-rs/`](../../../prj/05-GraphStack/tspi-greet-rs/) (Rust 等价) ; [`prj/05-GraphStack/tspi-greet-egl/`](../../../prj/05-GraphStack/tspi-greet-egl/) (EGL 扩展形态)
> - 合成器侧 (Weston) 内部分层与配置：[[03-1-compositor-weston]]
> - 相关知识链：[[01-ui-stack-overview]] ; [[04-kernel-fb-drm-kms]] ; [[06-EGL]]
> - 上游：[Wayland 协议文档](https://wayland.freedesktop.org/docs/html/) ; [Wayland Book (Drew DeVault)](https://wayland-book.com/) ; [`wayland.xml`](https://gitlab.freedesktop.org/wayland/wayland/-/blob/main/protocol/wayland.xml)


## 1. 客户端 ↔ 合成器：Unix Socket 通信架构

### 1.1 物理拓扑

Wayland **没有网络透明**（区分于传统x11支持远程渲染指令）。客户端与 compositor 是**同一台机器上的两个进程**，靠一个 unix-domain socket 通信。

```text
┌─────────────────────┐                       ┌──────────────────────┐
│   Wayland Client    │     unix socket       │  Wayland Compositor  │
│   (tspi-greet)      │ ◄──────────────────►  │  (Weston)            │
│                     │   wire 帧 + SCM_RIGHTS│                      │
│   libwayland-client │                       │   libwayland-server  │
└─────────────────────┘                       └──────────────────────┘
        ▲                                              │
        │                                              │ drmModeAtomicCommit
        │                                              ▼
        │                                       ┌───────────────┐
        │ SCM_RIGHTS 传递的 fd                  │ /dev/dri/card0│
        │ (SHM fd / dma-buf fd / keymap fd)     │ (DRM/KMS)     │
        │                                       └───────────────┘
        ▼
┌─────────────────────┐
│ /tmp/tspi-greet-XX  │  (匿名共享内存)
│ 或 /dev/mali0 (GPU) │
└─────────────────────┘
```

**两条数据通道，物理上分开**：

1. **wire 通道**：unix socket 上的 request / event 字节流 —— 控制平面
2. **fd 通道**：通过 SCM_RIGHTS 跨进程递交的文件描述符 —— 数据平面 (SHM 内存、dma-buf、keymap 文件...)

这是 Wayland 零拷贝的物理基础：客户端把"指向像素的 fd"通过控制平面递交，**像素数据本身永远不进控制平面**。

### 1.2 Socket 位置：`$XDG_RUNTIME_DIR/wayland-N`

```text
$XDG_RUNTIME_DIR/
  ├── wayland-0           # 默认 socket 文件 (unix-domain, type=SOCK_STREAM)
  └── wayland-0.lock      # advisory lock, 防同名 compositor 重复绑定
```

**客户端用 `$WAYLAND_DISPLAY` 环境变量定位 socket：**

```sh
export XDG_RUNTIME_DIR=/run
export WAYLAND_DISPLAY=wayland-0
# 客户端进程内 libwayland-client 会做 connect("$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY")
```

在 tspi 板上实测：

```sh
$ ssh tspi 'ls -la /run/wayland-*'
srwxrwxr-x 1 root weston 0 May 21 09:11 /run/wayland-0
-rw-rw---- 1 root weston 0 May 21 09:11 /run/wayland-0.lock
```

`s` 前缀表示 socket 文件，符合 unix-domain socket 约定。

### 1.3 Wire 帧格式

所有 request 和 event 在 socket 上都是同一种二进制帧：

```text
偏移   字段                       说明
0x00   sender_object_id  (u32)    谁发的；客户端发请求时 = 调用对象的 ID，服务端发事件时 = 触发对象的 ID
0x04   opcode (u16)               XML 中本 interface 内 request/event 的序号 (0-based)
       size   (u16)               整帧字节数 (含 8 字节头部)
0x08   args[0..n]                 按 XML 声明顺序紧接, 全部 4 字节对齐
```

`args` 中各类型的编码：

| XML type        | 字节布局                                                | 备注                                          |
| --------------- | ------------------------------------------------------- | --------------------------------------------- |
| `int` / `uint`  | 4 字节 (小端)                                           | -                                             |
| `fixed`         | 4 字节，s23.8 定点                                      | 子像素精度坐标                                |
| `object`        | 4 字节，目标对象 ID                                     | -                                             |
| `new_id`        | 4 字节，**客户端自分配**的新对象 ID                     | 见 §1.5                                       |
| `string`        | `len(u32)` + UTF-8 字节 + `\0` + padding 到 4B          | `len` 含末尾 NUL                              |
| `array`         | `len(u32)` + 原始字节 + padding 到 4B                   | 用于 modifier 列表、states 列表等             |
| `fd`            | **不在 wire 上！**                                      | 走 SCM_RIGHTS 控制消息单独递交（见 §1.4）     |

**协议是异步的、双向的、严格按帧顺序**：每条 wire 字节流就是一串这样的帧拼接，连一个字节都不浪费。客户端发请求**不阻塞等回包**——服务端按 wire 顺序处理，回的事件随时插入流中。

### 1.4 关键能力：SCM_RIGHTS 传 fd

`fd` 类型参数**不出现在 wire 帧里**，而是通过 socket 的 `sendmsg(2)` 控制消息 `SCM_RIGHTS` 跨进程递交：

```text
客户端调:  sendmsg(socket_fd,
                   {iov = wire 帧, cmsg = {SCM_RIGHTS, [fd1, fd2, ...]}},
                   0);
内核:     在服务端进程里复制 fd 表项,服务端进程拿到一个新 fd 值
          但指向同一个 file struct (即同一段共享内存 / 同一个 dma-buf)
服务端调:  recvmsg(socket_fd, &msg, 0);   // msg.msg_control 里拿到 fd
```

**典型用例**：

| Wayland 请求                          | 通过 SCM_RIGHTS 传的 fd                | 用途                          |
| ------------------------------------- | -------------------------------------- | ----------------------------- |
| `wl_shm.create_pool(fd, size)`        | 客户端 `mkstemp` + `mmap` 的临时文件 fd | CPU 共享内存                  |
| `zwp_linux_dmabuf_v1.create_params`   | GPU 渲染目标的 dma-buf fd               | 零拷贝 GPU buffer             |
| `wl_keyboard.keymap` (event)          | 合成器编好的 xkb keymap 文件 fd         | 客户端 mmap 读 keymap         |

**为什么这是零拷贝的根**：客户端在自己进程里 `mmap` 共享内存写像素，然后把 fd 通过 SCM_RIGHTS 递给合成器，合成器在自己进程里 `mmap` 同一个 fd —— **两边看到的是同一段物理内存**。像素从未拷贝过，只是 fd 这个引用跨进程交付了一次。

### 1.5 同步原语：roundtrip 与 sync

Wire 是异步的，但有时客户端必须等"服务端把队列里的事都处理完再回应一下"——比如等 `wl_registry.global` 事件列举完所有 global 才好挑哪些 bind。

`wl_display.sync` 就是这个原语：

```c
// libwayland-client.h 概念伪代码
struct wl_callback *cb = wl_display_sync(dpy);
// → 在 wire 上发 wl_display.sync(new_id=K), 创建一个 wl_callback 对象@K
// → 服务端处理完之前所有请求后, 立刻回 wl_callback.done(serial=...)
// → 客户端 dispatch 到 wl_callback 的 done listener
```

`wl_display_roundtrip(dpy)` 是这个的封装：发 sync 然后阻塞 dispatch 到 done 回调返回。

在 [`tspi-greet/src/main.c:253`](../../../prj/05-GraphStack/tspi-greet/src/main.c:253) 与 [`main.c:271`](../../../prj/05-GraphStack/tspi-greet/src/main.c:271) 分别用了两次：一次等 registry 列举完，一次等首次 configure 到达。**这是初始化阶段必须的同步点**，错过会导致后续代码用空指针 / 拿不到尺寸。


## 2. 协议对象层次关系

### 2.1 对象模型

Wayland 是**面向对象的二进制 RPC 协议**：

- **接口 (interface)**：一类对象的能力描述 (如 `wl_surface`、`xdg_toplevel`)；
- **对象 (object)**：接口的实例，每个对象有一个 32-bit ID；
- **请求 (request)**：客户端 → 合成器的方法调用；
- **事件 (event)**：合成器 → 客户端的回调通知；
- **ID 分配**：客户端 ID ∈ `[1, 0xFEFFFFFF]`，服务端 ID ∈ `[0xFF000000, 0xFFFFFFFF]`，**永远不冲突**。
- **`new_id` 自分配**：客户端在请求里填一个尚未使用的 ID，立刻可用，无需等服务端确认——这是 wayland 协议**多步调用零 round-trip**的关键。

### 2.2 核心对象的依赖树

```text
wl_display (ID=1, 协议根, libwayland-client 启动时硬编码)
   │
   ├── wl_registry              (← wl_display.get_registry)
   │     │
   │     │ 列举服务端所有 global, 客户端 bind 自己需要的:
   │     ▼
   │   bind 出具体的 global 对象:
   │
   ├── wl_compositor            (surface 工厂)
   │     └── wl_surface          (← wl_compositor.create_surface)
   │           │ 裸内容容器,需绑 role 才能 commit 显示
   │           │
   │           ├─ attach buffer (wl_shm / dmabuf / wl_egl_window 来源)
   │           └─ 绑 role:
   │               ↑
   ├── xdg_wm_base              (xdg-shell 扩展协议入口)
   │     └── xdg_surface         (← xdg_wm_base.get_xdg_surface(wl_surface))
   │           │ 把裸 surface 升格为 "桌面 surface 适配器"
   │           │
   │           ├── xdg_toplevel  (← xdg_surface.get_toplevel)  ← role: 桌面窗口
   │           └── xdg_popup     (← xdg_surface.get_popup)     ← role: 菜单/弹层
   │
   ├── wl_shm                   (CPU 共享内存 buffer 工厂)
   │     └── wl_shm_pool         (← wl_shm.create_pool(fd, size))
   │           └── wl_buffer     (← pool.create_buffer)        ← attach 到 wl_surface
   │
   ├── zwp_linux_dmabuf_v1      (GPU dma-buf 工厂,扩展协议)
   │     └── zwp_linux_buffer_params_v1
   │           └── wl_buffer     (dma-buf 包装)
   │
   ├── wl_seat                  (输入设备组)
   │     ├── wl_keyboard         (← seat.get_keyboard)
   │     ├── wl_pointer          (← seat.get_pointer)
   │     └── wl_touch            (← seat.get_touch)
   │
   └── wl_output                (一台物理显示器)
         (event: geometry / mode / scale / done)
```

注意三件事：

1. **wl_display 是唯一的协议入口**：所有对象最终都从它派生出来；
2. **wl_registry 是发现机制**：服务端有哪些 global 由 registry 列举决定，**不在协议层硬编码**；
3. **wl_surface 是核心抽象**：它本身是裸内容，**通过 role 绑定获得语义**（toplevel / popup / cursor / subsurface / layer-shell / ivi-surface）。

### 2.3 wl_surface 的 role 模型

`wl_surface` 是一块裸像素容器。它**没有 role 时不能 commit 显示**。客户端必须显式给它绑一个 role：

| Role            | 绑定方式                                  | 含义                                |
| --------------- | ----------------------------------------- | ----------------------------------- |
| `xdg_toplevel`  | `xdg_surface.get_toplevel(surf)`          | 桌面窗口 (本笔记示例)                |
| `xdg_popup`     | `xdg_surface.get_popup(parent, ...)`      | 菜单 / 工具提示                     |
| `cursor`        | `wl_pointer.set_cursor(surf, ...)`        | 鼠标贴图                            |
| `subsurface`    | `wl_subcompositor.get_subsurface(...)`    | 嵌套子 surface (画中画)              |
| `wlr_layer`     | `zwlr_layer_shell_v1.get_layer_surface`   | 状态栏 / 壁纸 / 锁屏 (wlroots 扩展) |
| `ivi_surface`   | `ivi_application.surface_create`          | 汽车 IVI / HMI                      |

**一个 surface 只能绑定一个 role，且不可更改** —— 协议级硬约束。

### 2.4 协议扩展：XML + wayland-scanner

核心 wayland 协议保持极简（`wayland.xml` 全部加起来才 1500 行）；其他能力通过**协议扩展**注入：

| 类别       | 路径前缀                              | 稳定性策略                                |
| ---------- | ------------------------------------- | ----------------------------------------- |
| stable     | `wayland-protocols/stable/`           | 协议冻结,前向兼容 (xdg-shell, linux-dmabuf-v1)|
| staging    | `wayland-protocols/staging/`          | 接口冻结待迁 stable                        |
| unstable   | `wayland-protocols/unstable/`         | 名字带 `_v1`/`_v2`,可能不兼容              |
| wlr        | wlroots 自带                          | wlroots 生态扩展 (layer-shell 等)         |
| 厂商私有   | 自带 XML                              | IVI / 嵌入式定制                          |

每份 XML 描述一个 interface 集合。客户端构建时用 `wayland-scanner` 把 XML 编成两份代码：

```sh
# 本仓库实例: prj/05-GraphStack/tspi-greet/Makefile:11-12
wayland-scanner client-header  xdg-shell.xml  →  xdg-shell-client.h
wayland-scanner private-code   xdg-shell.xml  →  xdg-shell-protocol.c
```

- `*-client.h`：暴露 `xdg_toplevel_set_app_id()` 等内联包装函数 + listener 结构体类型
- `*-protocol.c`：marshaling/unmarshaling 实现，编进客户端二进制

**协议 = XML + scanner 生成的代码**。`wl_compositor_create_surface()` 这种函数不是 libwayland-client 里的，而是由 scanner 从 `wayland.xml` 现编出来的小段内联代码。


## 3. Wayland 核心接口对象

Wayland 是**面向对象的二进制 RPC 协议**：

- **接口 (interface)**：一类对象的能力描述 (如 `wl_surface`、`xdg_toplevel`)；

这些接口体现为wayland-client代码中的具体数据结构。

以 [`prj/05-GraphStack/tspi-greet/src/main.c:24-39`](../../../prj/05-GraphStack/tspi-greet/src/main.c:24) 的 `struct app` 为骨架，逐字段讲透每个指针的协议含义、获取方式、用途、依赖。

```c
// src/main.c:24
struct app {
    struct wl_display     *dpy;        // §3.2
    struct wl_registry    *reg;        // §3.3
    struct wl_compositor  *compositor; // §3.4
    struct wl_shm         *shm;        // §3.5
    struct xdg_wm_base    *wm_base;    // §3.6

    struct wl_surface     *surf;       // §3.7
    struct xdg_surface    *xsurf;      // §3.8
    struct xdg_toplevel   *top;        // §3.9

    int  width, height;                // §3.10 (state)
    int  configured;
    int  running;
    int  needs_redraw;
};
```

### 3.1 OverView

以下表格基于笔记中 `struct app` 涉及的 Wayland 核心接口(`struct wl_xxx`)，总结了每个接口的核心能力（请求、事件、用途）及依赖关系。

| 接口            | 协议作用                                         | 主要能力（请求 / 事件）                                      | 依赖 / 备注                                                  |
| --------------- | ------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| `wl_display`    | 客户端与合成器的连接根对象（ID=1）               | **请求**：`get_registry`（创建 `wl_registry`）、`sync`（同步回调）<br>**函数**：`wl_display_connect`（建立socket）、`wl_display_dispatch`（读fd派发事件）、`wl_display_roundtrip`（阻塞等待同步） | 无依赖（协议入口）。通过 `$WAYLAND_DISPLAY` socket 连接。    |
| `wl_registry`   | 全局对象目录，枚举合成器支持的所有全局接口       | **事件**：`global`（通告接口名、版本、name）、`global_remove`（接口被移除）<br>**请求**：`bind`（根据 name 绑定具体全局对象） | 由 `wl_display.get_registry` 创建。必须 `roundtrip` 等待 `global` 事件后再 `bind`。 |
| `wl_compositor` | 合成器全局对象，唯一作用是创建 `wl_surface`      | **请求**：`create_surface`（返回新 `wl_surface`）、`create_region`（返回 `wl_region`，用于输入区域） | 通过 `wl_registry.bind` 获得，接口名 `"wl_compositor"`，通常绑定版本 4。 |
| `wl_shm`        | 共享内存全局工厂，提供 CPU 可写的像素 buffer     | **事件**：`format`（通告支持的像素格式，如 ARGB8888）<br>**请求**：`create_pool`（创建 `wl_shm_pool`，传递文件描述符 fd） | 通过 `wl_registry.bind` 获得，接口名 `"wl_shm"`。EGL 路径下不需要。 |
| `xdg_wm_base`   | xdg-shell 扩展协议的全局入口，实现桌面窗口语义   | **事件**：`ping`（探测客户端存活，需立即 `pong` 回复）<br>**请求**：`pong`（回复 ping）、`get_xdg_surface`（为 `wl_surface` 创建 `xdg_surface`） | 通过 `wl_registry.bind` 获得，接口名 `"xdg_wm_base"`。必须添加 `ping` 监听，否则断开连接。 |
| `wl_surface`    | 可显示内容的容器（无角色时不可见）               | **请求**：`attach`（绑定 `wl_buffer`）、`damage`/`damage_buffer`（标记脏区域）、`commit`（原子提交 pending 状态）、`frame`（请求帧回调）、`set_buffer_scale` 等<br>**事件**：`enter`/`leave`（进入/离开 `wl_output`） | 由 `wl_compositor.create_surface` 创建。**双缓冲状态**：所有请求先进入 pending，`commit` 后变为 current。 |
| `xdg_surface`   | 为 `wl_surface` 赋予 xdg-shell 角色（适配器层）  | **事件**：`configure`（合成器建议配置，带 serial）<br>**请求**：`ack_configure`（确认配置，必须回复）、`get_toplevel`（创建 `xdg_toplevel`）、`get_popup`（创建 `xdg_popup`）、`set_window_geometry`（设置窗口几何） | 由 `xdg_wm_base.get_xdg_surface` 创建。必须先 `ack_configure` 才能第一次 `commit` buffer。 |
| `xdg_toplevel`  | 桌面窗口角色（常规应用主窗口）                   | **事件**：`configure`（传递尺寸、状态如最大化/全屏）、`close`（用户或合成器要求关闭）<br>**请求**：`set_title`、`set_app_id`（kiosk‑shell 匹配关键）、`set_maximized`/`set_fullscreen`、`move`/`resize`（响应鼠标拖拽）、`set_min_size`/`set_max_size` | 由 `xdg_surface.get_toplevel` 创建。`set_app_id` 用于 compositor 识别应用（如 weston kiosk 自动全屏）。 |
| `wl_buffer`     | 像素数据容器（由 `wl_shm_pool` 或 EGL 内部创建） | **请求**：`destroy`<br>**事件**：`release`（合成器不再使用该 buffer，可重用或销毁） | 通过 `wl_shm_pool.create_buffer` 创建（CPU 路径），或由 `wl_egl_window` / dmabuf 隐式管理（GPU 路径）。 |
| `wl_callback`   | 帧同步回调                                       | **事件**：`done`（回调时间点到达）<br>**请求**：无（由 `wl_surface.frame` 创建） | 用于节流渲染，等待上一帧被合成器处理后绘制下一帧。           |

> **补充说明**：上表中未列出 `wl_shm_pool`，它作为 `wl_shm` 的辅助接口，负责从 fd 切出多个 `wl_buffer`。另外，EGL 路径下 `wl_surface` 的 `attach`/`damage`/`commit` 由 `eglSwapBuffers` 内部完成，应用层不再显式调用。

### 3.2 `struct wl_display *dpy` —— 协议入口

**含义**：客户端与 compositor 的连接根对象。**对应协议对象 ID = 1，是硬编码的**——libwayland-client 库初始化时直接把客户端侧 wl_display proxy 的 object_id 写成 1。所有后续对象都直接或间接从它派生。

**怎么来**（[`main.c:248`](../../../prj/05-GraphStack/tspi-greet/src/main.c:248)）：

```c
a.dpy = wl_display_connect(NULL);
if (!a.dpy) { fprintf(stderr, "wl_display_connect failed\n"); return 1; }
```

`wl_display_connect(NULL)` 做了三件事：

1. 读 `$WAYLAND_DISPLAY` 环境变量（NULL 时），拼出 `$XDG_RUNTIME_DIR/wayland-N` 路径
2. `socket(AF_UNIX, SOCK_STREAM, 0)` + `connect(socket, ...)`
3. 在客户端进程内构造一个 `struct wl_display` —— **既是协议对象 proxy（ID=1），又是事件队列与 fd 的封装**

**干什么用**：

- 派生 wl_registry：`wl_display_get_registry(a.dpy)`
- 同步：`wl_display_roundtrip(a.dpy)` / `wl_display_sync(a.dpy)`
- 主循环驱动：`wl_display_dispatch(a.dpy)` 在内部读 fd、解析帧、调 listener
- 暴露 fd（高级用法）：`wl_display_get_fd(a.dpy)` 让你能把它塞进 `epoll` / `poll`
- 退出：`wl_display_disconnect(a.dpy)`

**依赖**：无（起点）。

**易错点**：

- 失败常见原因：`$XDG_RUNTIME_DIR` 没设、socket 文件不存在、用户不在 socket 文件的属组里。
- 板上调试用 `ls -l $XDG_RUNTIME_DIR/wayland-*` 三秒确认。
- 一个进程通常只调一次 `wl_display_connect`；可以连多次（多块 socket），但不常见。

### 3.3 `struct wl_registry *reg` —— 全局对象目录

**含义**：**服务端通过它告诉客户端"我支持哪些 global 对象**"。每个 global 是服务端预先注册好的全局可用接口（compositor、shm、seat、output ...）。

**怎么来**（[`main.c:251-253`](../../../prj/05-GraphStack/tspi-greet/src/main.c:251)）：

```c
a.reg = wl_display_get_registry(a.dpy);
wl_registry_add_listener(a.reg, &reg_listener, &a);
wl_display_roundtrip(a.dpy);   // 等所有 global 事件到达
```

三句话各做一件事：

1. **`wl_display_get_registry`** 在 wire 上发 `wl_display.get_registry(new_id=X)` —— 客户端立刻分配一个新 ID `X` 给 wl_registry，无需等回包；
2. **`wl_registry_add_listener`** 在客户端本地把 `(global, global_remove)` 两个composistor的回调函数挂到这个 wl_registry proxy；
3. **`wl_display_roundtrip`** 等服务端把所有 `wl_registry.global(name, interface, version)` 事件发完（一个 sync + done 屏障）。

**listener 是什么**：

```c
/* ---- registry -------------------------------------------------------- */
/*============================================================**
*@READNME:
- 当客户端调用 wl_display_get_registry() 并添加此监听器后，
  合成器会为每一个它能提供的全局服务（如 wl_compositor、wl_shm、xdg_wm_base 等）
  触发一次 global 事件。
  global回调中，client的流程代码就需要把这些全局对象资源的指针binding到自己的数据结构里。
*=============================================================*/
static void reg_global(void *data, struct wl_registry *reg, uint32_t name,
                       const char *iface, uint32_t version)
{
	struct app *a = data;
	if (!strcmp(iface, wl_compositor_interface.name))
		a->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, wl_shm_interface.name))
		a->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
	else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		a->wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(a->wm_base, &wm_listener, a);
	}
}
static void reg_global_remove(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener reg_listener = {
	.global = reg_global, .global_remove = reg_global_remove,
};
```

每个接口的"事件回调表"都由 `wayland-scanner` 从 XML 生成。当 wire 上来一帧 `wl_registry.global(...)`，libwayland 找到 reg 对象的 listener 表，调对应的 `.global` 函数指针。

**干什么用**：

- 在 `.global` 回调里，按 `interface` 名字匹配 → 调 `wl_registry_bind(reg, name, &wl_X_interface, version)` 拿到具体 global 的 proxy。这就是 `a.compositor` / `a.shm` / `a.wm_base` 是怎么填上的。

**依赖**：`a.dpy`。

**易错点**：

- 必须 roundtrip！没等 global 事件到，就调 bind 会用到 0 值的 name，立刻报协议错误。
- 一次性事件 vs 持续事件：`global_remove` 在 global 被服务端撤销时调（极少见，通常是 USB 输入设备热拔）；本 demo 没处理。

### 3.4 `struct wl_compositor *compositor` —— surface 工厂

**含义**：合成器最基本的 global，**唯一能做的事就是产出 `wl_surface`** 和 `wl_region`。

**怎么来**（[`main.c:53-54`](../../../prj/05-GraphStack/tspi-greet/src/main.c:53)）：

```c
if (!strcmp(iface, wl_compositor_interface.name))
    a->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
```

`wl_registry_bind(reg, name, &wl_X_interface, version)` 的 4 个参数：

| 参数 | 含义 |
|------|------|
| `reg` | 父对象 wl_registry |
| `name` | 服务端在 global 事件里给的 numeric name（不是字符串）|
| `&wl_compositor_interface` | scanner 生成的接口描述符（含 method 表 + opcode 表） |
| `4` | 客户端要求的版本号；服务端版本 ≥4 时给 v4 proxy，否则返回错误 |

**`wl_compositor_interface.name`** 是字符串 `"wl_compositor"`，由 scanner 生成；用于在 `.global` 回调里 strcmp 匹配。

**干什么用**：

- `wl_compositor_create_surface(a.compositor)` → 见 §3.7

**依赖**：`a.reg`。

**版本号选取**：v4 是当前最常用的稳定版本；v4 才支持 `wl_surface.damage_buffer`、`wl_surface.set_buffer_scale` 等。在 [`main.c:54`](../../../prj/05-GraphStack/tspi-greet/src/main.c:54) 选 4 是合理基线。

### 3.5 `struct wl_shm *shm` —— CPU 共享内存 buffer 工厂

**含义**：让客户端用"CPU 共享内存"作为像素来源的 global。`wl_shm` 自己只产 `wl_shm_pool`，pool 再切出多个 `wl_buffer`。

**怎么来**（[`main.c:55-56`](../../../prj/05-GraphStack/tspi-greet/src/main.c:55)）：

```c
else if (!strcmp(iface, wl_shm_interface.name))
    a->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
```

**干什么用**（[`main.c:115-138`](../../../prj/05-GraphStack/tspi-greet/src/main.c:115)）：

```c
char tmpl[] = "/tmp/tspi-greet-XXXXXX";
int fd = mkstemp(tmpl);
unlink(tmpl);                                  // 立刻 unlink → 进程退出自动回收
ftruncate(fd, size);                           // 撑开成 stride*h 字节
void *map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

struct wl_shm_pool *pool = wl_shm_create_pool(a->shm, fd, size);
//                          ↑ 这一句在 wire 上发 wl_shm.create_pool(new_id, fd, size)
//                            fd 通过 SCM_RIGHTS 跨进程传给合成器
//                            合成器在自己进程里 mmap 同一个 fd → 共享同一段物理内存

close(fd);                                     // pool 已持有 fd 引用,可关
struct wl_buffer *wl = wl_shm_pool_create_buffer(pool, 0, w, h, stride,
                                                 WL_SHM_FORMAT_ARGB8888);
wl_shm_pool_destroy(pool);                     // 销毁 pool 不销毁 buffer
```

**完整调用语义**：

1. 客户端 `mkstemp + ftruncate + mmap` 拿到一段本地可读可写的内存；
2. `wl_shm_create_pool` 把这段内存（通过 fd）注册给服务端，得到 `wl_shm_pool` 句柄；
3. `pool.create_buffer(offset, w, h, stride, format)` 在 pool 上"切片"出 `wl_buffer`——客户端可以多次切，做双缓冲；
4. 客户端往 mmap 区域里画像素；
5. `wl_surface.attach(buf) + damage + commit` 提交给合成器。

**依赖**：`a.reg`。

**易错点**：

- format 必须是 `WL_SHM_FORMAT_ARGB8888` / `XRGB8888` 等服务端 `wl_shm.format` 事件**事先声明过**的；
- `stride * h <= pool_size`，否则协议错误踢出连接；
- pool 销毁后已切出去的 wl_buffer 仍然有效（pool 与 buffer 引用计数分离）。

**EGL 路径下不需要 wl_shm**：[`tspi-greet-egl/src/main.c`](../../../prj/05-GraphStack/tspi-greet-egl/src/main.c:69) 完全不 bind `wl_shm`，因为 EGL 实现内部走 `zwp_linux_dmabuf_v1` 协议直接送 GPU dma-buf。

### 3.6 `struct xdg_wm_base *wm_base` —— xdg-shell 入口

**含义**：xdg-shell 扩展协议的全局入口对象。xdg-shell 协议规定了"桌面窗口"的语义（toplevel / popup），核心 wayland 协议**故意不规定**这一层，让窗口管理策略与 wayland 协议解耦。

**怎么来**（[`main.c:57-60`](../../../prj/05-GraphStack/tspi-greet/src/main.c:57)）：

```c
else if (!strcmp(iface, xdg_wm_base_interface.name)) {
    a->wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(a->wm_base, &wm_listener, a);
}
```

注意它**同时挂了 listener**。原因：`xdg_wm_base` 会发 `ping(serial)` 事件，客户端必须立即 `pong(serial)` 回应：

```c
// src/main.c:42-46
static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial)
{
    xdg_wm_base_pong(b, serial);
}
static const struct xdg_wm_base_listener wm_listener = { .ping = wm_ping };
```

**漏写这个回调** → 服务端 ~10 秒后认定客户端"僵死"，单方面断开连接。这是初次写 wayland 客户端最常见的 silent bug。

**干什么用**：

- `xdg_wm_base_get_xdg_surface(a.wm_base, a.surf)` —— 把一个裸 wl_surface 升格为 xdg_surface（见 §3.8）
- 处理 ping/pong 探活

**依赖**：`a.reg`。

**为什么这是"扩展"而不是核心**：核心 wayland 协议刻意只管 surface + buffer + 输入；窗口管理（最大化、标题栏、popup 位置）属于"桌面策略"层。xdg-shell 作为 stable 协议，事实上已成为所有桌面 compositor 的标准窗口入口。

### 3.7 `struct wl_surface *surf` —— 裸内容容器

**含义**：一块**可显示的内容**，没有 role 时不能 commit 显示。`wl_surface` 是 wayland 最核心的抽象——所有"用户能看到的东西"最终都是某个 wl_surface 的 buffer。

**怎么来**（[`main.c:261`](../../../prj/05-GraphStack/tspi-greet/src/main.c:261)）：

```c
a.surf = wl_compositor_create_surface(a.compositor);
```

一句话。**无需 listener**——wl_surface 自己几乎没有事件（只有 `enter`/`leave` 通知进入了哪个 wl_output，本 demo 不关心）。

**干什么用**：

- 给它绑一个 role（本 demo 是 xdg_toplevel，见 §3.8）；
- `wl_surface_attach(surf, buf, x, y)` —— 关联一个 wl_buffer 作为内容；
- `wl_surface_damage_buffer(surf, x, y, w, h)` —— 标记哪部分像素改了（合成器优化用）；
- `wl_surface_frame(surf)` —— 拿一个 wl_callback，下一帧可显示时回调（节流机制）；
- `wl_surface_commit(surf)` —— **原子事务提交**：把 attach / damage / frame / scale 等 pending state 一次性变成 current state。

**关键概念：双缓冲 state**：

```text
应用调 wl_surface.attach(buf)        ─┐
应用调 wl_surface.damage(rect)        │   pending state (客户端这边累积)
应用调 wl_surface.set_buffer_scale 2  │
                                    ─┘
应用调 wl_surface.commit()             ─→  原子拷贝 pending → current
                                           合成器永远只看见 current
```

这就是 Wayland "every frame is perfect" 的协议保证——合成器看不到"半完成"状态。

**依赖**：`a.compositor`。

### 3.8 `struct xdg_surface *xsurf` —— role 适配器

**含义**：**把 wl_surface 升格为"xdg-shell 语义层 surface"**。它是 wl_surface 与具体 role（toplevel / popup）之间的中间层，负责处理 configure / ack_configure 协商序列。

**怎么来**（[`main.c:262-263`](../../../prj/05-GraphStack/tspi-greet/src/main.c:262)）：

```c
a.xsurf = xdg_wm_base_get_xdg_surface(a.wm_base, a.surf);
xdg_surface_add_listener(a.xsurf, &xsurf_listener, &a);
```

注意：

- **`get_xdg_surface` 是个适配器函数**——它不创建新内容，只是把 `a.surf` 包了一层 role 适配；
- 调用之后，`a.surf` 仍然存在，但**"被预定了 role"**——再调一次 `get_xdg_surface(.., a.surf)` 会协议错误。

**listener**（[`main.c:68-77`](../../../prj/05-GraphStack/tspi-greet/src/main.c:68)）：

```c
static void xsurf_configure(void *data, struct xdg_surface *xs, uint32_t serial)
{
    struct app *a = data;
    xdg_surface_ack_configure(xs, serial);
    a->configured = 1;
    a->needs_redraw = 1;
}
static const struct xdg_surface_listener xsurf_listener = {
    .configure = xsurf_configure,
};
```

**configure / ack_configure 协议**：

- 客户端 commit → 合成器决定"这窗口该多大、什么状态" → 发 `xdg_surface.configure(serial)`；
- 客户端必须 `ack_configure(serial)` —— **不 ack 则不允许 attach 第一个 buffer**；
- 这是首帧上屏前必走的协商。

**干什么用**：

- 接收 configure，调 ack_configure；
- `xdg_surface_get_toplevel(a.xsurf)` —— 真正绑 role（见 §3.9）；
- 也可能调 `xdg_surface_get_popup(...)` 绑 popup role；
- 设置窗口几何区域：`xdg_surface_set_window_geometry(xs, x, y, w, h)`。

**依赖**：`a.surf` + `a.wm_base`。

### 3.9 `struct xdg_toplevel *top` —— 桌面窗口角色

**含义**：把 xdg_surface 进一步绑定为"桌面窗口"——具有 title / app_id / 最大化 / 关闭 / 尺寸协商等桌面窗口语义。**这一步完成后，a.surf 的 role = xdg_toplevel，可以 commit 显示了**。

**怎么来**（[`main.c:265-268`](../../../prj/05-GraphStack/tspi-greet/src/main.c:265)）：

```c
a.top = xdg_surface_get_toplevel(a.xsurf);
xdg_toplevel_add_listener(a.top, &top_listener, &a);
xdg_toplevel_set_app_id(a.top, "com.tspi.greet");
xdg_toplevel_set_title(a.top, "TSPI Greet");
```

**listener**（[`main.c:79-97`](../../../prj/05-GraphStack/tspi-greet/src/main.c:79)）：

```c
static void top_configure(void *data, struct xdg_toplevel *t,
                          int32_t w, int32_t h, struct wl_array *states)
{
    struct app *a = data;
    if (w > 0 && h > 0) { a->width = w; a->height = h; }
}
static void top_close(void *data, struct xdg_toplevel *t)
{
    struct app *a = data;
    a->running = 0;
}
static const struct xdg_toplevel_listener top_listener = {
    .configure        = top_configure,    // 尺寸协商
    .close            = top_close,        // 用户/合成器要求关闭
    .configure_bounds = top_configure_bounds,
    .wm_capabilities  = top_wm_capabilities,
};
```

**关键事件解读**：

- `configure(w, h, states)`：合成器告诉客户端 "我希望你画 w×h，当前状态有 fullscreen/maximized/...". 注意这只是建议，客户端可以选择不同尺寸（kiosk 全屏场景就强制全屏）；
- `close`：用户点了 X 或合成器决定关掉窗口——客户端应主动退出（本 demo 设 `a.running = 0`）。

**关键 request `set_app_id`**：

- 这是 **kiosk-shell 与本窗口绑定的关键**！
- weston.ini 的 `[output] app-ids=com.tspi.greet` 会匹配这个字符串，**这个 toplevel 自动全屏占据 DSI-1 输出**。
- 详见 [[03-1-compositor-weston]] 配置章节。

**`set_title`** 是 desktop-shell 显示标题用的；kiosk-shell 全屏场景看不到，但写上无害。

**干什么用（其他常用 request）**：

- `xdg_toplevel_set_maximized` / `unset_maximized`
- `xdg_toplevel_set_fullscreen(output)` / `unset_fullscreen`
- `xdg_toplevel_set_min_size` / `set_max_size`
- `xdg_toplevel_destroy` —— 退出时务必显式 destroy（[`main.c:278`](../../../prj/05-GraphStack/tspi-greet/src/main.c:278)）。

**依赖**：`a.xsurf`。

**至此**，wayland 协议对象建立完毕：`dpy → reg → (compositor + shm + wm_base) → surf → xsurf → top`。下面进入"画第一帧"。



### 3.11 完整调用序列	

```c
// src/main.c:244
int main(void)
{
    struct app a = { .width = 480, .height = 800, .running = 1 };

    /* ── 阶段一: 连接 + 列举 ────────────────────────────── */
    a.dpy = wl_display_connect(NULL);                          // §3.2
    a.reg = wl_display_get_registry(a.dpy);                    // §3.3
    wl_registry_add_listener(a.reg, &reg_listener, &a);
    wl_display_roundtrip(a.dpy);    // ← 这里 .global 回调把 §3.4/3.5/3.6 三个字段填上

    if (!a.compositor || !a.shm || !a.wm_base) return 1;       // 三个 global 任一缺则失败

    /* ── 阶段二: 建 surface + 绑 role ──────────────────── */
    a.surf  = wl_compositor_create_surface(a.compositor);      // §3.7
    a.xsurf = xdg_wm_base_get_xdg_surface(a.wm_base, a.surf);  // §3.8
    xdg_surface_add_listener(a.xsurf, &xsurf_listener, &a);

    a.top = xdg_surface_get_toplevel(a.xsurf);                 // §3.9
    xdg_toplevel_add_listener(a.top, &top_listener, &a);
    xdg_toplevel_set_app_id(a.top, "com.tspi.greet");
    xdg_toplevel_set_title(a.top, "TSPI Greet");

    /* ── 阶段三: 触发首次 configure 并等 ───────────────── */
    wl_surface_commit(a.surf);                                 // 触发首轮 configure
    wl_display_roundtrip(a.dpy);                               // 等 configure 到达

    /* ── 阶段四: 首帧 + 进入主循环 ─────────────────────── */
    if (a.needs_redraw) { redraw(&a); a.needs_redraw = 0; }    // 画第一帧 → §4
    while (a.running && wl_display_dispatch(a.dpy) != -1)
        ;

    /* ── 阶段五: 反序清理 ──────────────────────────────── */
    xdg_toplevel_destroy(a.top);
    xdg_surface_destroy(a.xsurf);
    wl_surface_destroy(a.surf);
    wl_display_disconnect(a.dpy);
    return 0;
}
```

四个阶段对应 §3.1 依赖图的四层；每个 `roundtrip` 是同步屏障，把异步 wire 切成可推理的阶段。

### 3.12 EGL 路径下 struct app 多出的 5 个字段

如果客户端走 GPU 渲染路径（`wayland-egl` + GLES），struct app 上**不需要 wl_shm 字段**，但**多出 5 个 EGL 字段**：

```c
// prj/05-GraphStack/tspi-greet-egl/src/main.c:32-46 (节选)
struct app {
    /* wayland 部分 (与 §3.2-3.9 完全相同, 但少 wl_shm) */
    struct wl_display     *dpy;
    struct wl_registry    *reg;
    struct wl_compositor  *compositor;
    struct xdg_wm_base    *wm_base;
    struct wl_surface     *surf;
    struct xdg_surface    *xsurf;
    struct xdg_toplevel   *top;

    /* EGL 桥接 (新增) */
    EGLDisplay  egl_dpy;       // 把 a.dpy 包成 EGL 视角的 display
    EGLContext  egl_ctx;       // GLES 渲染上下文
    EGLConfig   egl_cfg;       // framebuffer 属性预设
    EGLSurface  egl_surf;      // EGL 渲染目标
    struct wl_egl_window *egl_win;  // 把 a.surf 与尺寸打包成 EGL native window

    /* state */
    int  width, height;
    int  pending_w, pending_h;  // configure 来的新尺寸,主循环里 wl_egl_window_resize
    /* ... */
};
```

EGL 5 个字段的依赖关系与协议对应物在 [`tspi-greet-egl/Design-EGL-Path.md`](../../../prj/05-GraphStack/tspi-greet-egl/Design-EGL-Path.md) §3 中详细走读，EGL 这一层抽象本身的概念在 [[06-EGL]] §4 / §6 中展开。**核心一句话**：

> `struct wl_egl_window` 是 wayland 与 EGL 实现之间的薄桥接（libwayland-egl 上游提供，板上 8.5 KB），它绑住 `a.surf` 与尺寸；`eglCreateWindowSurface(egl_dpy, cfg, egl_win, NULL)` 让 EGL 实现接管 `a.surf` 的 buffer 生命周期 —— 应用层从此**不再调** `wl_surface.attach / damage / commit`，由 `eglSwapBuffers` 在内部完成。


## 4. 主循环与事件分派

### 4.1 `wl_display_dispatch(dpy)`

[`main.c:275-276`](../../../prj/05-GraphStack/tspi-greet/src/main.c:275)：

```c
while (a.running && wl_display_dispatch(a.dpy) != -1)
    ;
```

`wl_display_dispatch` 内部做四件事：

1. `wl_display_flush` —— 把客户端这边累积的 request 写到 socket；
2. `poll/read` —— 阻塞等 socket 上有事件帧到达；
3. 解析帧 —— 按 object_id 找 proxy、按 opcode 找 listener；
4. 调用对应 listener 回调（在当前线程同步执行）。

**返回值**：成功返回派发的事件数（>= 1），错误返回 -1。

**阻塞语义**：这个调用是**阻塞的**，没事件时一直睡在 poll 里。所以主循环非常简洁——"每醒来一次就处理一批事件"。

### 4.2 回调里发生了什么

主循环阻塞在 dispatch；事件到达时驱动以下三类回调：

| 回调来源              | 文件位置                                         | 做什么                                            |
| --------------------- | ------------------------------------------------ | ------------------------------------------------- |
| `xdg_wm_base.ping`    | [`main.c:42-44`](../../../prj/05-GraphStack/tspi-greet/src/main.c:42)     | 立即 `xdg_wm_base_pong(b, serial)` 回应,防被踢   |
| `xdg_surface.configure` | [`main.c:68-74`](../../../prj/05-GraphStack/tspi-greet/src/main.c:68)   | `ack_configure(serial)` + 置 `needs_redraw=1`     |
| `xdg_toplevel.configure` | [`main.c:79-84`](../../../prj/05-GraphStack/tspi-greet/src/main.c:79)  | 把新尺寸记到 `a.width/height`                     |
| `xdg_toplevel.close`  | [`main.c:85-89`](../../../prj/05-GraphStack/tspi-greet/src/main.c:85)    | 置 `a.running=0`,下次循环判定退出                  |
| `wl_buffer.release`   | [`main.c:106-112`](../../../prj/05-GraphStack/tspi-greet/src/main.c:106) | 合成器用完 buffer,客户端 munmap + destroy buffer  |
| `wl_callback.done` (frame) | [`main.c:216-221`](../../../prj/05-GraphStack/tspi-greet/src/main.c:216) | 上一帧出完了,触发 `redraw()` 出下一帧             |

**回调链 → 渲染**：当 `xsurf_configure` 置 `needs_redraw=1` 后，主程序在阶段四首次调一次 `redraw()`；之后每次 `frame_done` 回调触发下一次 `redraw()`，形成**自驱动的渲染循环**。

`redraw()`（[`main.c:224-241`](../../../prj/05-GraphStack/tspi-greet/src/main.c:224)）做的事：

```c
static void redraw(struct app *a)
{
    int w = a->width, h = a->height;
    struct buffer *b = create_buffer(a, w, h);   // 见 §3.5 mmap+shm_pool 链
    draw(a, b->data, w, h);                       // cairo 在 mmap 上画
    wl_surface_attach(a->surf, b->wl, 0, 0);     // attach 到 pending
    wl_surface_damage_buffer(a->surf, 0, 0, w, h);

    struct wl_callback *cb = wl_surface_frame(a->surf);  // 拿下一帧通知
    wl_callback_add_listener(cb, &frame_listener, a);

    wl_surface_commit(a->surf);                   // 原子提交 pending → current
}
```

**这一整段串通了 §3 所有字段**：
- 用 `a.shm` 建 pool 切 buffer；
- 把 buffer attach 到 `a.surf`；
- 用 `a.surf` 的 frame 机制约束下一帧节奏；
- commit 触发合成器拿走这帧。


## 5. 关键易错点小结

按出现频率排：

| # | 易错点 | 后果 | 在哪修 |
|---|--------|------|--------|
| 1 | 没回 `xdg_wm_base.ping` 的 pong | ~10 秒后被合成器单方面断开,日志无显式错误 | §3.6 必须挂 `wm_listener` |
| 2 | bind 前没 roundtrip | global 名字还没拿到就 bind,协议错 | §3.3 必须 `wl_display_roundtrip` |
| 3 | 没 ack_configure 就 attach buffer | 首帧不显示 / 协议错 | §3.8 `xsurf_configure` 必须先 `ack_configure(serial)` |
| 4 | wl_surface 没绑 role 就 commit | 协议错 (no role) | §3.7 → §3.8 → §3.9 顺序不能错 |
| 5 | wl_shm format 不在服务端声明列表里 | wl_buffer 创建失败 | §3.5 用 `WL_SHM_FORMAT_ARGB8888/XRGB8888` 这种标准 fmt |
| 6 | 退出时没 `wl_display_disconnect` | socket 残留 / lock 没释放 | `main()` 阶段五 |
| 7 | 多线程下共享一个 dpy 不加 queue | 事件竞态/丢失 | 多线程渲染要用 per-proxy event queue (本 demo 单线程不涉及) |


## 6. 下一步阅读

- **合成器侧**：[[03-1-compositor-weston]] —— 同一份 wire 协议，从合成器角度怎么看（libweston 分层、前端 shell、renderer、backend、weston.ini 配置）
- **内核侧**：[[04-kernel-fb-drm-kms]] —— wl_buffer 最终如何变成 DRM framebuffer 上屏
- **EGL 扩展**：[[06-EGL]] + [`prj/05-GraphStack/tspi-greet-egl/`](../../../prj/05-GraphStack/tspi-greet-egl/) —— struct app 多 5 个字段后会怎样
- **Rust 等价**：[`prj/05-GraphStack/tspi-greet-rs/Design-Wayland-Rs.md`](../../../prj/05-GraphStack/tspi-greet-rs/Design-Wayland-Rs.md) —— `Dispatch` trait 怎么取代 C 的 listener 表
