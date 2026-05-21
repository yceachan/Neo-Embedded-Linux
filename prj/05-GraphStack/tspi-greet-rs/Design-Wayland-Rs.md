---
title: wayland-rs 改写 tspi-greet 的应用层架构 —— Dispatch / EventQueue / xdg-shell 全景
tags: [rust, wayland, wayland-rs, xdg-shell, cairo, shm, kiosk-shell, tspi]
desc: 从 C 版 main.c 到 Rust 版 main.rs 的逐函数走读 —— wayland-client::Dispatch trait 如何对应 C 的 listener 表，EventQueue 如何取代 wl_display_dispatch
update: 2026-05-21
---


# wayland-rs 改写 tspi-greet 的应用层架构

> [!note]
> **Ref:**
> [`src/main.rs`](src/main.rs:1) ; [`src/draw.rs`](src/draw.rs:1) ;
> 对照原版 [`prj/05-GraphStack/tspi-greet/src/main.c`](../tspi-greet/src/main.c:1) ;
> [`note/Subsystem/Graph-Stack/03-wayland-weston.md`](../../../note/Subsystem/Graph-Stack/03-wayland-weston.md) ;
> [`note/Subsystem/Graph-Stack/05-tspi-buildroot-weston-de.md`](../../../note/Subsystem/Graph-Stack/05-tspi-buildroot-weston-de.md) ;
> [`Design-CrossCompile.md`](Design-CrossCompile.md) ; [`Design-TimeSeq.md`](Design-TimeSeq.md) ;
> [wayland-client 0.31 docs](https://docs.rs/wayland-client/0.31/) ; [wayland-protocols 0.31 docs](https://docs.rs/wayland-protocols/0.31/)


## 1. 整体架构

![ChatGPT Image 2026年5月21日 19_11_46](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/ChatGPT%20Image%202026%E5%B9%B45%E6%9C%8821%E6%97%A5%2019_11_46.png)

每个组件存在的理由：

- **`Connection`**：纯 Rust 写的 wayland-rs 直接打开 `$XDG_RUNTIME_DIR/$WAYLAND_DISPLAY`，按 wayland wire 协议收发 — 因此**最终 ELF 不需要链 `libwayland-client.so`**（见 [Design-CrossCompile.md §7](Design-CrossCompile.md)）。
- **`EventQueue<App>`**：reactor 主循环。等价于 C 版的 `while wl_display_dispatch(dpy)`。
- **每个 `impl Dispatch<I, U>`**：把 C 版本里"对每个对象都写一个 listener struct + 回调函数"的模式，**静态分派**到 trait 方法。编译期就能保证"事件 → 处理"完整无遗漏。
- **`wayland-protocols::xdg::shell::client`**：等价于 `wayland-scanner client-header` 生成的 `xdg-shell-client.h` —— 但**在编译期由 build.rs 完成**，无需任何外部 `.xml` 输入或运行 scanner。
- **SHM 缓冲两段路径**：`tempfile` 拿一个匿名 fd → `set_len(size)` 撑大 → `memmap2::MmapMut` 映射 → cairo 直接在这片 mmap 上绘 → 同一个 fd 喂给 `wl_shm.create_pool` —— compositor 在自己进程里 mmap 同一个 fd，看到我们刚画的像素，无需拷贝。


## 2. 关键 API 走读（带源码片段 + file:line）

### 2.1 `App` 状态机 —— C 的 struct app 直译

**位置**：[`src/main.rs:30-46`](src/main.rs:30)
**作用一句话**：把 C 版本里那一堆松散的全局指针包到一个 struct，由 `Dispatch::event(state: &mut Self, …)` 安全可变借出。

```rust
// src/main.rs:30
struct App {
    compositor: Option<wl_compositor::WlCompositor>,
    shm: Option<wl_shm::WlShm>,
    wm_base: Option<xdg_wm_base::XdgWmBase>,
    surf: Option<wl_surface::WlSurface>,
    xsurf: Option<xdg_surface::XdgSurface>,
    top: Option<xdg_toplevel::XdgToplevel>,
    width: i32,
    height: i32,
    configured: bool,
    running: bool,
    needs_redraw: bool,
}
```

**为什么这么写**：
- 全部 `Option<_>` —— wayland 对象延迟绑定（先 listen 再 bind），用 None 表"还没拿到"；
- 把状态全收口到 App，**Rust 借用检查器**自动防止两个回调同时改 `running` —— C 版本里这种竞争只能靠"单线程信任"。
- 等价 C 版 `struct app` 见 [`prj/05-GraphStack/tspi-greet/src/main.c:24`](../tspi-greet/src/main.c:24)。

### 2.2 `Dispatch<XdgWmBase>::event` —— ping/pong 心跳

**位置**：[`src/main.rs:75-91`](src/main.rs:75)
**作用一句话**：compositor 每隔一段时间 ping，本端必须立即 pong 回去；否则会被认作"僵死客户端"踢出。

```rust
// src/main.rs:75
impl Dispatch<xdg_wm_base::XdgWmBase, ()> for App {
    fn event(_state: &mut Self, wm: &xdg_wm_base::XdgWmBase,
             event: xdg_wm_base::Event, _data: &(),
             _conn: &Connection, _qh: &QueueHandle<Self>) {
        if let xdg_wm_base::Event::Ping { serial } = event {
            wm.pong(serial);
        }
    }
}
```

**为什么这么写**：
- 对照 C 版 [`tspi-greet/src/main.c:42`](../tspi-greet/src/main.c:42) `wm_ping()` —— 同一份职责，Rust 把回调写在 trait 方法里而不是 listener struct；
- 第二个 trait 类型参数 `UserData = ()` —— 此对象不需要携带用户态状态；如果需要（如下面的 WlBuffer），就把 RAII 资源挂上去。
- 模式 match Enum 而不是查 union → 编译期穷尽性检查，新增协议事件不会被默默吃掉。

### 2.3 `Dispatch<XdgSurface>::event` —— configure ack

**位置**：[`src/main.rs:97-114`](src/main.rs:97)
**作用一句话**：compositor 发 configure 表示"窗口已准备好可以画了"，必须 `ack_configure(serial)` 后才允许 attach buffer。

```rust
// src/main.rs:97
impl Dispatch<xdg_surface::XdgSurface, ()> for App {
    fn event(state: &mut Self, xs: &xdg_surface::XdgSurface,
             event: xdg_surface::Event, _data: &(),
             _conn: &Connection, _qh: &QueueHandle<Self>) {
        if let xdg_surface::Event::Configure { serial } = event {
            xs.ack_configure(serial);
            state.configured = true;
            state.needs_redraw = true;
        }
    }
}
```

**为什么这么写**：
- `ack_configure` **必须在第一次 commit buffer 之前**调用 —— 否则 compositor 把这次 commit 当作协议错误，断开连接（C 版本同语义见 [`main.c:68`](../tspi-greet/src/main.c:68)）；
- `needs_redraw = true` 而非这里直接 `redraw()` —— Dispatch 上下文里没法拿 `qh` 充当不可变借用同时去 mut 借 `state`；把"想画"标记下来，等 `main()` 跑到那一行再画。这是 wayland-rs 0.31 dispatch 模型最常见的"延后动作"模式。

### 2.4 `Dispatch<XdgToplevel>::event` —— 尺寸协商 / 关闭

**位置**：[`src/main.rs:120-141`](src/main.rs:120)
**作用一句话**：尺寸由 compositor 决定（kiosk-shell 会推 DSI 输出的全屏尺寸 480×800）；close 来了就退出主循环。

```rust
// src/main.rs:120
impl Dispatch<xdg_toplevel::XdgToplevel, ()> for App {
    fn event(state: &mut Self, _t: &xdg_toplevel::XdgToplevel,
             event: xdg_toplevel::Event, _data: &(),
             _conn: &Connection, _qh: &QueueHandle<Self>) {
        match event {
            xdg_toplevel::Event::Configure { width, height, .. } => {
                if width > 0 && height > 0 {
                    state.width = width;
                    state.height = height;
                }
            }
            xdg_toplevel::Event::Close => state.running = false,
            _ => {}
        }
    }
}
```

**为什么这么写**：
- 对照 C 版 [`tspi-greet/src/main.c:79`](../tspi-greet/src/main.c:79) 的 `top_configure()` + `top_close()` —— C 必须显式给四个回调（configure / close / configure_bounds / wm_capabilities）都写空函数，否则 listener 表元素数对不上；Rust 这边 `_ => {}` 一行兜走所有未来新增 / 不关心的事件，类型系统帮你做 forward-compat。
- 与 C 同样的偏执：`if width > 0 && height > 0` —— compositor 偶尔会 configure(0,0) 表"你自己拿主意"，此时保留默认 480×800。

### 2.5 SHM Buffer 的 RAII 包装

**位置**：[`src/main.rs:144-165`](src/main.rs:144)
**作用一句话**：把 mmap + file 同 wl_buffer 绑定，让 wayland-rs 的 UserData 机制在 buffer release 时自动 munmap/close 资源。

```rust
// src/main.rs:148
struct BufferState {
    _mmap: MmapMut, // RAII：drop 时自动 munmap
    _file: File,    // RAII：drop 时自动 close
}

impl Dispatch<wl_buffer::WlBuffer, BufferState> for App {
    fn event(_state: &mut Self, buf: &wl_buffer::WlBuffer,
             event: wl_buffer::Event, _data: &BufferState, ...) {
        if let wl_buffer::Event::Release = event {
            buf.destroy(); // BufferState 随 proxy drop 自动 munmap/close
        }
    }
}
```

**为什么这么写**：
- C 版 [`tspi-greet/src/main.c:106`](../tspi-greet/src/main.c:106) `buf_release()` 里要**手动** `munmap` + `free` + `wl_buffer_destroy`；这里把 mmap/file 塞进 wayland 对象的 UserData，Rust 析构链路一并清理。**漏一行的概率从 3 个动作 → 0 个动作**。
- 下划线前缀字段名（`_mmap` / `_file`）—— 告诉编译器"我只是占位 RAII，不读不写"。

### 2.6 `redraw()` —— 一帧的完整生命周期

**位置**：[`src/main.rs:193-237`](src/main.rs:193)
**作用一句话**：分配 SHM 缓冲 → cairo 画一帧 → 喂给 compositor → 挂帧回调以便下一帧重复。

```rust
// src/main.rs:193
fn redraw(app: &mut App, qh: &QueueHandle<App>) {
    let w = if app.width > 0 { app.width } else { 480 };
    let h = if app.height > 0 { app.height } else { 800 };
    let stride = w * 4;
    let size = (stride * h) as usize;

    let file = tempfile::tempfile()?;
    file.set_len(size as u64)?;
    let mut mmap = unsafe { MmapMut::map_mut(&file) }?;

    draw::render_inplace(&mut mmap, w, h);   // ← cairo 现场画

    let shm = app.shm.as_ref().unwrap();
    let pool = shm.create_pool(file.as_fd(), size as i32, qh, ());
    let bstate = BufferState { _mmap: mmap, _file: file };
    let buf = pool.create_buffer(0, w, h, stride, wl_shm::Format::Argb8888, qh, bstate);
    pool.destroy();

    let surf = app.surf.as_ref().unwrap();
    surf.attach(Some(&buf), 0, 0);
    surf.damage_buffer(0, 0, w, h);
    surf.frame(qh, ());
    surf.commit();
}
```

**为什么这么写**：
- **pool 用完即 destroy**：只是个轻量 SHM region descriptor，buffer 持有真实引用 —— 早 destroy 减少 compositor 端可见的 pool 数量；
- **每帧重建 buffer 而不是双缓冲**：C 版同款写法（[`main.c:224`](../tspi-greet/src/main.c:224)）。坏处是每帧一次 mmap/munmap；好处是不需要管"哪一帧 compositor 还在读"。对 1Hz 时钟刷新而言开销可忽略；如果要做 60fps 视频帧，必须切换到 2-buffer 轮转；
- **frame() callback 挂在 commit 之前**：必须如此 —— commit 之后再挂会丢首次 done 事件。
- **damage_buffer 用 buffer 坐标**：避免 HiDPI 缩放下 surface 坐标产生 sub-pixel 损失。

### 2.7 `main()` —— 引导段

**位置**：[`src/main.rs:259-309`](src/main.rs:259)
**作用一句话**：连 compositor → 列举 globals → 绑定三大件 → 建 toplevel → set_app_id 让 kiosk-shell 接管 → 进 reactor。

```rust
// src/main.rs:259
let conn = Connection::connect_to_env().expect("WAYLAND_DISPLAY not set or connect failed");
let (globals, mut event_queue) = registry_queue_init(&conn).expect("registry_queue_init");
let qh = event_queue.handle();

let mut app = App::new();
app.compositor = Some(globals.bind::<wl_compositor::WlCompositor,_,_>(&qh, 4..=4, ())?);
app.shm        = Some(globals.bind::<wl_shm::WlShm,_,_>(&qh, 1..=1, ())?);
app.wm_base    = Some(globals.bind::<xdg_wm_base::XdgWmBase,_,_>(&qh, 1..=4, ())?);

let surf  = compositor.create_surface(&qh, ());
let xsurf = wm_base.get_xdg_surface(&surf, &qh, ());
let top   = xsurf.get_toplevel(&qh, ());
top.set_app_id("com.tspi.greet".into());      // ← kiosk-shell 据此全屏接管
top.set_title("TSPI Greet (Rust)".into());

app.surf.as_ref().unwrap().commit();
event_queue.roundtrip(&mut app).unwrap();     // 等首次 configure

if app.needs_redraw { redraw(&mut app, &qh); app.needs_redraw = false; }

while app.running {
    if event_queue.blocking_dispatch(&mut app).is_err() { break; }
}
```

**为什么这么写**：
- `registry_queue_init` 一次性把 global 列表收集完成 —— 比 C 版的"先 add_listener 再 roundtrip 等列表"少一轮；
- 三个 `bind::<I, _, _>(qh, ver_range, udata)` —— `ver_range` 是版本约束，本端最高接受到第几版。`xdg_wm_base 1..=4` 表示 1~4 任意都行；
- `set_app_id("com.tspi.greet")` 与 [`etc/00-kiosk.ini`](etc/00-kiosk.ini:13) 里 `[output] app-ids=com.tspi.greet` **精确匹配** —— kiosk-shell 看见这个 app_id 就把窗口绑定到 DSI-1 输出全屏；
- `blocking_dispatch` 在主循环里阻塞等下一次 socket 数据，与 C 版 `wl_display_dispatch` 等价。

### 2.8 `draw::render_inplace` —— cairo 把字画到 mmap 上

**位置**：[`src/draw.rs:22-78`](src/draw.rs:22)
**作用一句话**：构造一个"借走 mmap 内存"的 ImageSurface，cairo 在上面画渐变背景 + 标语 + 时钟。

```rust
// src/draw.rs:22
pub fn render_inplace(pixels: &mut [u8], w: i32, h: i32) {
    let stride = w * 4;
    let surface = unsafe {
        ImageSurface::create_for_data_unsafe(pixels.as_mut_ptr(),
                                             Format::ARgb32, w, h, stride)
    }.expect("create_for_data_unsafe failed");
    let cr = Context::new(&surface).expect("Context::new failed");

    // 渐变背景
    let bg = LinearGradient::new(0.0, 0.0, 0.0, h as f64);
    bg.add_color_stop_rgb(0.0, 0.04, 0.10, 0.22);
    bg.add_color_stop_rgb(1.0, 0.01, 0.03, 0.08);
    cr.set_source(&bg).unwrap(); cr.paint().unwrap();

    // ...居中文字 / 时间字符串绘制...
    surface.flush();  // ← 必须 flush 才能确保位图落到 mmap 字节
}
```

**为什么这么写**：
- 选 `create_for_data_unsafe` 而非 `create_for_data`：后者要求把整段 `Vec<u8>` 所有权交给 cairo-rs，那意味着每帧从 mmap 拷贝一遍。这里直接借走 mmap 指针，零拷贝；
- **必须 `surface.flush()`**：cairo 内部 ImageSurface 维持一个脏页缓存，不 flush 时位图可能仍在缓存里，compositor 拿到的就是上一帧（C 版 [`main.c:209`](../tspi-greet/src/main.c:209) 同样依赖 `cairo_surface_destroy` 隐式 flush）。
- 时间用 `libc::localtime_r` 而不是 `chrono`：避免引入 ~200KB 的额外依赖，对仪表盘类应用没必要。


## 3. 源码巡读 (Source-walk)

按"启动 → 等握手 → 画一帧 → 主循环 → 退出"的故事线读 [`src/main.rs`](src/main.rs:1)：

1. **启动**：`main` 第一行 `Connection::connect_to_env`（[L261](src/main.rs:261)）—— 这是 Rust 版与 C 版的对应点（C: `wl_display_connect(NULL)` at [main.c:248](../tspi-greet/src/main.c:248)）。
2. **列举 globals**：`registry_queue_init` 顶替 C 版的"add_listener + roundtrip"两步（[L262](src/main.rs:262)）。
3. **bind 三大件**：compositor / wl_shm / xdg_wm_base（[L268-280](src/main.rs:268)）。C 版同位置 [main.c:53-60](../tspi-greet/src/main.c:53)。
4. **建 toplevel + set_app_id**：[L285-295](src/main.rs:285) ← 与 kiosk-shell 的契约（C 版 [main.c:267](../tspi-greet/src/main.c:267)）。
5. **首次 roundtrip**：等 compositor 推首个 xdg_surface.configure（[L298](src/main.rs:298)）。其间 `Dispatch<XdgSurface>::event` 触发，置 `needs_redraw=true`。
6. **首帧 redraw**：[L300](src/main.rs:300)。具体进 `draw::render_inplace`，再回到 attach/damage/commit。
7. **主循环**：`while app.running { event_queue.blocking_dispatch(&mut app) }`（[L304-307](src/main.rs:304)）。
    - 每收到 frame.done → `Dispatch<WlCallback>::event` → 调 redraw（[L171](src/main.rs:171)）→ 下一帧；
    - 每收到 xdg_wm_base.ping → 自动 pong（[L84](src/main.rs:84)）；
    - 收到 xdg_toplevel.close → `running=false`，循环退出。
8. **退出**：函数 return → `App` drop → 所有 wayland 对象按依赖序自动 destroy（**RAII 收尾的 Rust 优势**：C 版需要手写 `wl_surface_destroy / wl_display_disconnect` at [main.c:278-281](../tspi-greet/src/main.c:278)）。


## 4. 与 C 版本的逐函数映射表

| C (`tspi-greet/src/main.c`) | Rust (`tspi-greet-rs/src/main.rs`) | 备注 |
|----------------------------|------------------------------------|------|
| `struct app` (L24) | `struct App` (L30) | Option<_> 模型 |
| `wm_ping` listener (L42) | `Dispatch<XdgWmBase>::event` (L75) | match Enum |
| `reg_global` listener (L49) | `globals.bind` (L268-280) | 一次性绑定 |
| `xsurf_configure` (L68) | `Dispatch<XdgSurface>::event` (L97) | 同语义 |
| `top_configure`/`top_close` (L79) | `Dispatch<XdgToplevel>::event` (L120) | `_ => {}` 兜底 |
| `buf_release` (L106) | `Dispatch<WlBuffer>::event` (L153) + `BufferState` RAII | 自动 munmap |
| `create_buffer` (L115) | inline in `redraw` (L208-220) | 同语义 |
| `draw` (L141) | `draw::render_inplace` (L22) | unsafe 零拷贝 |
| `redraw` (L224) | `redraw` (L193) | 同语义 |
| `frame_done` (L216) | `Dispatch<WlCallback>::event` (L171) | 触发下一帧 |
| `main` (L244) | `main` (L259) | reactor 主体 |


## 5. 与 [05-tspi-buildroot-weston-de.md](../../../note/Subsystem/Graph-Stack/05-tspi-buildroot-weston-de.md) 的衔接

这份 demo 是该笔记 "DE = Weston/kiosk-shell + autolaunch 客户端" 路线的 Rust 实例。
与笔记里的 weston.ini 一一对照：

| `etc/00-kiosk.ini` | 对应笔记小节 |
|--------------------|-------------|
| `shell=kiosk-shell.so` | §"为什么用 kiosk-shell" |
| `[output] app-ids=com.tspi.greet` | §"app-id 绑定 output" |
| `[autolaunch] path=/usr/bin/tspi-greet-rs` | §"weston 启动后自动拉应用" |
| `# watch=true` 注释说明 | §"watch=true 的语义陷阱" |

参考 [memory: Weston autolaunch.watch 语义](../../../../home/pi/.claude/projects/-home-pi-imx/memory/feedback_weston_autolaunch_watch.md) —— `watch=true` 在 weston 14 里是"子进程死则 weston 死"，**不是**自动重拉。本 demo 故意不写。
