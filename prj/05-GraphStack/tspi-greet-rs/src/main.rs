// =============================================================================
//  tspi-greet-rs —— Weston kiosk-shell 全屏欢迎页客户端 (Rust 版)
//
//  与 C 版 prj/05-GraphStack/tspi-greet/src/main.c 一一对照重写：
//  - wayland-client (纯 Rust 实现 wayland wire protocol，零 libwayland-client.so 链接)
//  - wayland-protocols::xdg::shell::client 提供 xdg_wm_base / xdg_surface / xdg_toplevel
//      （等价于 C 版本里 wayland-scanner 生成的 xdg-shell-client.h / xdg-shell-protocol.c）
//  - cairo-rs 渲染（同一份 cairo C 库，靠 pkg-config 找到 buildroot sysroot）
//  - 通过 set_app_id("com.tspi.greet") 让 kiosk-shell 按 weston.ini 全屏接管
//
//  Dispatch 模型（wayland-client 0.31 风格）：
//    每个 wayland 对象事件 → 实现 `Dispatch<I, UserData>` trait
//    EventQueue 是 reactor 主循环 —— blocking_dispatch() 把 socket 上来的事件
//    路由到对应 Dispatch::event 回调。
// =============================================================================

mod draw;

use memmap2::MmapMut;
use std::fs::File;
use std::os::fd::AsFd;

use wayland_client::{
    delegate_noop,
    globals::{registry_queue_init, GlobalListContents},
    protocol::{wl_buffer, wl_callback, wl_compositor, wl_registry, wl_shm, wl_shm_pool, wl_surface},
    Connection, Dispatch, EventQueue, QueueHandle, WEnum,
};
use wayland_protocols::xdg::shell::client::{xdg_surface, xdg_toplevel, xdg_wm_base};

/// 应用全局状态。等价于 C 版 `struct app`。
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

impl App {
    fn new() -> Self {
        Self {
            compositor: None,
            shm: None,
            wm_base: None,
            surf: None,
            xsurf: None,
            top: None,
            width: 480,
            height: 800,
            configured: false,
            running: true,
            needs_redraw: false,
        }
    }
}

// =============================================================================
//  registry: 列举 server 暴露的 global，按 interface 名 bind
//  对照 C 版 reg_global() in main.c:49
// =============================================================================
impl Dispatch<wl_registry::WlRegistry, GlobalListContents> for App {
    fn event(
        _state: &mut Self,
        _proxy: &wl_registry::WlRegistry,
        _event: wl_registry::Event,
        _data: &GlobalListContents,
        _conn: &Connection,
        _qh: &QueueHandle<Self>,
    ) {
        // 初始 global 列举由 registry_queue_init 一次性完成；
        // 这里只处理热插拔，本 demo 不需要。
    }
}

// =============================================================================
//  xdg_wm_base: ping/pong —— 必须回 pong，否则被 compositor 视为僵死客户端踢出
//  对照 C 版 wm_ping() in main.c:42
// =============================================================================
impl Dispatch<xdg_wm_base::XdgWmBase, ()> for App {
    fn event(
        _state: &mut Self,
        wm: &xdg_wm_base::XdgWmBase,
        event: xdg_wm_base::Event,
        _data: &(),
        _conn: &Connection,
        _qh: &QueueHandle<Self>,
    ) {
        if let xdg_wm_base::Event::Ping { serial } = event {
            wm.pong(serial);
        }
    }
}

// =============================================================================
//  xdg_surface.configure: ack + 标记可以画
//  对照 C 版 xsurf_configure() in main.c:68
// =============================================================================
impl Dispatch<xdg_surface::XdgSurface, ()> for App {
    fn event(
        state: &mut Self,
        xs: &xdg_surface::XdgSurface,
        event: xdg_surface::Event,
        _data: &(),
        _conn: &Connection,
        _qh: &QueueHandle<Self>,
    ) {
        if let xdg_surface::Event::Configure { serial } = event {
            xs.ack_configure(serial);
            state.configured = true;
            state.needs_redraw = true;
        }
    }
}

// =============================================================================
//  xdg_toplevel: configure (尺寸协商) + close
//  对照 C 版 top_configure() / top_close() in main.c:79/85
// =============================================================================
impl Dispatch<xdg_toplevel::XdgToplevel, ()> for App {
    fn event(
        state: &mut Self,
        _t: &xdg_toplevel::XdgToplevel,
        event: xdg_toplevel::Event,
        _data: &(),
        _conn: &Connection,
        _qh: &QueueHandle<Self>,
    ) {
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

// =============================================================================
//  wl_buffer.release: compositor 用完 → 释放 mmap + 销毁 wl_buffer
//  对照 C 版 buf_release() in main.c:106
// =============================================================================
struct BufferState {
    _mmap: MmapMut, // RAII：drop 时自动 munmap
    _file: File,    // RAII：drop 时自动 close
}

impl Dispatch<wl_buffer::WlBuffer, BufferState> for App {
    fn event(
        _state: &mut Self,
        buf: &wl_buffer::WlBuffer,
        event: wl_buffer::Event,
        _data: &BufferState,
        _conn: &Connection,
        _qh: &QueueHandle<Self>,
    ) {
        if let wl_buffer::Event::Release = event {
            // destroy buffer; BufferState (UData) 随 proxy drop 自动 munmap/close
            buf.destroy();
        }
    }
}

// =============================================================================
//  wl_callback (frame done) → 触发下一帧重绘
//  对照 C 版 frame_done() in main.c:216
// =============================================================================
impl Dispatch<wl_callback::WlCallback, ()> for App {
    fn event(
        state: &mut Self,
        _cb: &wl_callback::WlCallback,
        event: wl_callback::Event,
        _data: &(),
        _conn: &Connection,
        qh: &QueueHandle<Self>,
    ) {
        if let wl_callback::Event::Done { .. } = event {
            redraw(state, qh);
        }
    }
}

// 一些没有需要处理事件的对象，用 delegate_noop! 一行带过：
delegate_noop!(App: ignore wl_compositor::WlCompositor);
delegate_noop!(App: ignore wl_shm::WlShm);
delegate_noop!(App: ignore wl_shm_pool::WlShmPool);
delegate_noop!(App: ignore wl_surface::WlSurface);

// =============================================================================
//  redraw: 创建 SHM buffer → cairo 渲染 → attach/damage/commit
//  对照 C 版 redraw() in main.c:224
// =============================================================================
fn redraw(app: &mut App, qh: &QueueHandle<App>) {
    let w = if app.width > 0 { app.width } else { 480 };
    let h = if app.height > 0 { app.height } else { 800 };
    let stride = w * 4;
    let size = (stride * h) as usize;

    // ---- 创建 anonymous tmp file 作为 SHM 后备 ----
    let file = match tempfile::tempfile() {
        Ok(f) => f,
        Err(e) => {
            eprintln!("tempfile failed: {e}");
            app.running = false;
            return;
        }
    };
    if let Err(e) = file.set_len(size as u64) {
        eprintln!("set_len failed: {e}");
        app.running = false;
        return;
    }
    let mut mmap = unsafe {
        match MmapMut::map_mut(&file) {
            Ok(m) => m,
            Err(e) => {
                eprintln!("mmap failed: {e}");
                app.running = false;
                return;
            }
        }
    };

    // ---- cairo 渲染 ----
    draw::render_inplace(&mut mmap, w, h);

    // ---- 创建 wl_shm_pool 与 wl_buffer ----
    let shm = app.shm.as_ref().expect("shm not bound");
    let pool = shm.create_pool(file.as_fd(), size as i32, qh, ());
    let bstate = BufferState {
        _mmap: mmap,
        _file: file,
    };
    let buf = pool.create_buffer(0, w, h, stride, wl_shm::Format::Argb8888, qh, bstate);
    pool.destroy();

    // ---- attach / damage / frame-callback / commit ----
    let surf = app.surf.as_ref().unwrap();
    surf.attach(Some(&buf), 0, 0);
    surf.damage_buffer(0, 0, w, h);
    surf.frame(qh, ());
    surf.commit();
}

// =============================================================================
//  main
// =============================================================================
fn main() {
    // Layer 0: 报一下编译期 ARCH / OS，便于 run.log 一眼看出是 aarch64
    println!("[tspi-greet-rs] ARCH={} OS={}", std::env::consts::ARCH, std::env::consts::OS);

    let conn = Connection::connect_to_env().expect("WAYLAND_DISPLAY not set or connect failed");
    let (globals, mut event_queue): (_, EventQueue<App>) =
        registry_queue_init(&conn).expect("registry_queue_init");
    let qh = event_queue.handle();

    let mut app = App::new();

    // 绑定三大必备 global —— 对照 C 版 reg_global() switch
    app.compositor = Some(
        globals
            .bind::<wl_compositor::WlCompositor, _, _>(&qh, 4..=4, ())
            .expect("wl_compositor missing"),
    );
    app.shm = Some(
        globals
            .bind::<wl_shm::WlShm, _, _>(&qh, 1..=1, ())
            .expect("wl_shm missing"),
    );
    app.wm_base = Some(
        globals
            .bind::<xdg_wm_base::XdgWmBase, _, _>(&qh, 1..=4, ())
            .expect("xdg_wm_base missing"),
    );

    println!("[tspi-greet-rs] globals bound: compositor + wl_shm + xdg_wm_base");

    // 建 wl_surface + xdg_surface + xdg_toplevel
    let compositor = app.compositor.as_ref().unwrap().clone();
    let wm_base = app.wm_base.as_ref().unwrap().clone();

    let surf = compositor.create_surface(&qh, ());
    let xsurf = wm_base.get_xdg_surface(&surf, &qh, ());
    let top = xsurf.get_toplevel(&qh, ());

    top.set_app_id("com.tspi.greet".into());
    top.set_title("TSPI Greet (Rust)".into());

    app.surf = Some(surf);
    app.xsurf = Some(xsurf);
    app.top = Some(top);

    // 第一次 commit -> 等首次 configure
    app.surf.as_ref().unwrap().commit();
    event_queue.roundtrip(&mut app).expect("roundtrip");

    if app.needs_redraw {
        redraw(&mut app, &qh);
        app.needs_redraw = false;
    }

    println!("[tspi-greet-rs] entering main loop");

    while app.running {
        if event_queue.blocking_dispatch(&mut app).is_err() {
            eprintln!("[tspi-greet-rs] dispatch error -> exit");
            break;
        }
    }

    println!("[tspi-greet-rs] exit");
}

// 抑制未使用 warning（WEnum 是 wayland-client 通用占位，本 demo 不用）
#[allow(dead_code)]
fn _silence(_: WEnum<wl_shm::Format>) {}
