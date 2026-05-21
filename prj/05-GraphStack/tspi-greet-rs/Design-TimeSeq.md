---
title: tspi-greet-rs 运行时时序 —— 连接 / 协商 / 首帧 / 重绘 / 退出
tags: [wayland, sequence-diagram, xdg-shell, kiosk-shell, frame-callback]
desc: 用三张 mermaid sequenceDiagram 画出 wayland-rs 版本从 connect 到 main loop 的完整调用链
update: 2026-05-21
---


# tspi-greet-rs 运行时时序

> [!note]
> **Ref:** [`Design-Wayland-Rs.md`](Design-Wayland-Rs.md) ; [`src/main.rs`](src/main.rs:1) ;
> [`note/Subsystem/Graph-Stack/03-wayland-weston.md`](../../../note/Subsystem/Graph-Stack/03-wayland-weston.md)


## 1. 启动 + 握手 + 首帧

```mermaid
sequenceDiagram
    autonumber
    participant Sh as systemd/weston<br/>autolaunch
    participant App as tspi-greet-rs<br/>(Rust)
    participant Wc as wayland-client<br/>EventQueue
    participant Co as compositor<br/>(weston)
    participant Ks as kiosk-shell.so
    participant Cr as cairo

    rect rgb(220,235,250)
    note over Sh,App: 启动阶段 — weston.ini [autolaunch] 拉起
    Sh->>App: exec /usr/bin/tspi-greet-rs
    App->>App: env: XDG_RUNTIME_DIR / WAYLAND_DISPLAY=wayland-0
    App->>Wc: Connection::connect_to_env()
    Wc->>Co: connect(UDS /run/wayland-0)
    Co-->>Wc: ok
    end

    rect rgb(225,245,225)
    note over Wc,Co: registry 列举 — 一次 roundtrip 完成
    App->>Wc: registry_queue_init(&conn)
    Wc->>Co: wl_display.get_registry
    Co-->>Wc: global wl_compositor, wl_shm, xdg_wm_base, ...
    App->>Wc: globals.bind::<WlCompositor>(4..=4)
    App->>Wc: globals.bind::<WlShm>(1..=1)
    App->>Wc: globals.bind::<XdgWmBase>(1..=4)
    end

    rect rgb(250,235,225)
    note over App,Ks: window/surface 创建 — set_app_id 触发 kiosk-shell 接管
    App->>Co: compositor.create_surface -> wl_surface
    App->>Co: xdg_wm_base.get_xdg_surface(surf) -> xdg_surface
    App->>Co: xdg_surface.get_toplevel -> xdg_toplevel
    App->>Co: xdg_toplevel.set_app_id("com.tspi.greet")
    Co->>Ks: 新 toplevel app_id=com.tspi.greet
    Ks->>Ks: 查 weston.ini [output]<br/>app-ids=com.tspi.greet 命中 DSI-1
    Ks-->>Co: 把该 toplevel 全屏挂到 DSI-1
    App->>Co: wl_surface.commit (空 commit，触发首次 configure)
    Co-->>Wc: xdg_surface.configure(serial=1)
    Co-->>Wc: xdg_toplevel.configure(w=480, h=800)
    Wc->>App: Dispatch<XdgToplevel>::event -> 保存 w/h
    Wc->>App: Dispatch<XdgSurface>::event -> 'ack_configure(1) needs_redraw=true'
    end

    rect rgb(245,225,250)
    note over App,Cr: 首帧渲染 — cairo + SHM + commit
    App->>App: redraw(): tempfile + ftruncate(480*800*4)
    App->>App: memmap2::MmapMut::map_mut
    App->>Cr: ImageSurface::create_for_data_unsafe(ptr, ARgb32, 480, 800)
    Cr->>Cr: paint LinearGradient bg
    Cr->>Cr: show_text "Welcome to TSPI"
    Cr->>Cr: show_text HH:MM:SS / YYYY-mm-dd
    App->>Co: wl_shm.create_pool(fd, size)
    App->>Co: pool.create_buffer(0, w, h, stride, ARGB8888)
    App->>Co: wl_surface.attach(buf) + damage_buffer + frame() + commit
    Co->>Ks: 合成帧 -> 推到 DRM/KMS
    Ks-->>App: wl_callback.done(time_ms)
    Wc->>App: Dispatch<WlCallback>::event -> 再次 redraw
    end
```


## 2. 主循环 + ping/pong + 持续重绘

```mermaid
sequenceDiagram
    autonumber
    participant App as tspi-greet-rs
    participant Wc as wayland-client<br/>EventQueue
    participant Co as compositor

    loop while running
        Wc->>Wc: poll(socket) — 等下一帧 done / ping / 事件
        Co-->>Wc: xdg_wm_base.ping(serial)
        Wc->>App: Dispatch<XdgWmBase>::event
        App->>Co: xdg_wm_base.pong(serial)

        Co-->>Wc: wl_callback.done(t)
        Wc->>App: Dispatch<WlCallback>::event
        App->>App: redraw() — 新一帧 cairo + 新 buffer

        Co-->>Wc: wl_buffer.release(prev_buf)
        Wc->>App: Dispatch<WlBuffer>::event<br/>buf.destroy() + BufferState drop -> munmap
    end
```


## 3. 退出路径

```mermaid
sequenceDiagram
    autonumber
    participant Op as 操作员
    participant Co as compositor
    participant App as tspi-greet-rs
    participant Os as kernel

    alt 用户触发 (e.g. weston 终止)
        Op->>Co: SIGTERM
        Co-->>App: xdg_toplevel.close
        App->>App: Dispatch<XdgToplevel>::event<br/>running=false
        App->>App: blocking_dispatch 返回 -> while 退出
    else 外部 kill
        Op->>Os: kill <pid>
        Os->>App: SIGTERM
        App->>App: 默认处理 -> 正常 unwind
    end

    App->>App: main() return<br/>(App drop -> 所有 wayland 对象 RAII 收尾)
    App->>Co: wl_surface.destroy / xdg_*.destroy / wl_display.disconnect
    Co-->>Os: UDS close
```


## 4. 与 C 版时序的差异

| 节点 | C 版 | Rust 版 |
|------|------|---------|
| 列举 globals | `add_listener + roundtrip` 两步 | `registry_queue_init` 一步 |
| 事件分发 | C struct of fn pointers (`*_listener`) | trait `Dispatch::event(&mut self, …)` |
| 资源回收 | 每个回调里手写 `wl_*_destroy + munmap + close + free` | `BufferState` UserData RAII，drop 链路自动 |
| 主循环 | `while wl_display_dispatch(dpy) != -1` | `while event_queue.blocking_dispatch(&mut app).is_ok()` |
| 退出清理 | main 末尾手写 4 行 destroy / disconnect | App drop 自动 |
