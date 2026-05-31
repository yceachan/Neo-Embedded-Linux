---
title: gtk4-rs 窗口与表面管理实现 — SurfaceMonitor
tags: [gtk4-rs, GdkSurface, window-lifecycle, display-server]
desc: SurfaceMonitor 数据模型、信号+轮询双轨监控流程、Arc<AtomicU32> 的 Send+Sync 约束、GTK4 widget→surface→display 三层模型
update: 2026-05-31
---


# 窗口与表面管理实现 — SurfaceMonitor

> [!note]
> **Ref:** 源码 [`src/surface_monitor.rs`](./src/surface_monitor.rs), [`src/main.rs`](./src/main.rs)

> Aimed: 通过信号监听 + 500ms 定时轮询，实时追踪 `GtkApplicationWindow` 及 `GdkSurface` 的状态变化，展示 GTK4 的窗口/表面/display 三层模型。

## 1. 数据模型

### 1.1 SurfaceMonitor — 持有 Panel 的 root widget

```rust
pub struct SurfaceMonitor {
    pub root: gtk::Box,   // 顶层容器，被 main.rs append 到 Notebook Tab 2
}
```

结构极其简单——root 是 GTK widget，所有子 widget（Grid、Label）都在 `new()` 中创建并挂到 root 下。`SurfaceMonitor` 本身不持有任何业务数据，状态存在两个 `Arc<AtomicU32>` 中（见下）。

### 1.2 运行时状态（不在 struct 里，在闭包中）

```rust
let map_count = Arc::new(AtomicU32::new(0));      // 窗口 map 事件计数
let config_count = Arc::new(AtomicU32::new(0));   // 窗口 configure 事件计数(首帧+resize触发)
```

**为什么用 `Arc<AtomicU32>` 而非 `Rc<RefCell<u32>>`**：

```rust
// GTK4 信号闭包的 trait 约束：
// Fn(&Self, &ParamSpec) + Send + Sync + 'static
//                         ^^^^   ^^^^
// Rc 不实现 Send（非原子引用计数）
// RefCell 不实现 Sync（运行时借用检查非线程安全）
```

GTK4 可能在不同线程间分发信号（display server 通信线程），因此信号闭包必须线程安全。本 demo 的计数需求简单，`AtomicU32::fetch_add` 零开销。

### 1.3 监控的 7 项属性

| # | 属性 | 来源 | 取值方式 | 更新频率 |
|---|------|------|---------|---------|
| 0 | Surface mapped | `GtkWidget::is_mapped()` | 轮询 | 500ms |
| 1 | Window size | `GtkWidget::width()` / `height()` | 轮询 | 500ms |
| 2 | Display name | `GdkDisplay::name()` | 轮询 | 500ms |
| 3 | Scale factor | `GtkWidget::scale_factor()` | 轮询（HiDPI 用） | 500ms |
| 4 | Toplevel type | `env::var("WAYLAND_DISPLAY")` | 轮询 | 500ms |
| 5 | Map count | `connect_map` 信号回调 | 事件推送 | 实时 |
| 6 | Configure count | `connect_notify("default-width")` | 事件推送 | 实时 |

## 2. 源码执行流程

### 2.1 初始化序列

```
main() → build_ui(app)
  → window = ApplicationWindow::new(app)
  → window.set_title / set_default_size (960×680)

  → monitor = SurfaceMonitor::new(&window)
      ├─ root = Box::new(vertical)
      ├─ 构建 UI:
      │   ├─ title Label "窗口与表面管理"
      │   ├─ Grid (7 rows × 2 cols)
      │   │   每行: [属性名 Label] [值 Label]
      │   │   val_labels 存入 Vec<Label>
      │   └─ legend Label (说明文本)
      │
      ├─ 创建计数器:
      │   ├─ map_count = Arc::new(AtomicU32::new(0))
      │   └─ config_count = Arc::new(AtomicU32::new(0))
      │
      ├─ 连接信号 (Push):
      │   ├─ window.connect_map(move |_| { map_count.fetch_add(1) })
      │   │   窗口首次显示时触发。注意：这是 GtkWidget::map，不是 GdkSurface::map
      │   │
      │   ├─ window.connect_notify("default-width", …)
      │   └─ window.connect_notify("default-height", …)
      │       属性变化时 config_count++，用作 resize 的代理事件
      │
      └─ 启动轮询 (Poll):
          glib::timeout_add_local(500ms, move || {
              // 读取窗口状态 → 更新 7 个 Label
              // …
              glib::ControlFlow::Continue   // 返回 Continue = 保持定时器活跃
          })
```

### 2.2 运行时：每 500ms 的轮询循环

```
glib 主循环
  → 500ms 定时器到期 → 轮询闭包执行:

  ① mapped = win_ref.is_mapped()
      // GtkWidget 级——widget 及其 GdkSurface 已映射到屏幕
      // 关闭虚拟桌面/最小化时 mapped 变 false（取决于 compositor）

  ② w = win_ref.width()         // 当前分配的实际宽度（含 CSD 阴影边框）
     h = win_ref.height()        // 如果 widget 尚未 size-allocate，返回 -1 或 0

  ③ display = gdk4::Display::default().name()
      // 返回 "wayland-0" 或 ":0" 等 display server 标识

  ④ scale = win_ref.scale_factor()
      // HiDPI: 1× (普通), 2× (Retina/4K 缩放)

  ⑤ toplevel = 环境变量推断 Wayland vs X11
      // 注意：这只反映环境变量，不反映 GTK4 实际使用的 GDK 后端
      // GDK_BACKEND 环境变量可以覆盖，更准确的方法需要 GdkDisplay API

  ⑥ 将以上 + map_count.load() + config_count.load()
     格式化为 7 个 String
     逐项 set_text() → Grid 中的值 Label 即时刷新
```

### 2.3 窗口生命周期事件序列

```
Application::run()
  → activate signal
      → Window::new(app)               // ① GObject 构造（GdkSurface 尚未创建）
      → window.present()               // ② 请求显示
          → GdkSurface 创建            //     GDK 后端分配 surface
          → GdkSurface::realize()      //     分配后端资源
          → GtkWidget::map()           //     → map_count += 1
      → [事件循环]
          → (用户拖拽边缘 resize)       //     → connect_notify → config_count += 1
          → (用户移动窗口、切换 Tab)    //     无计数变化
          → (用户关闭窗口)              //
              → close-request signal   //     GTK 层信号
              → GtkWidget::unmap()     //     surface 解除映射
              → GdkSurface::destroy()  //     surface 销毁
              → Widget::destroy()      //     window 销毁
  → Application 退出（无剩余窗口时）
```

## 3. 关键实现细节

### 3.1 双轨监控（Push + Poll）的取舍

| 方式 | 优势 | 劣势 | 本 demo 用法 |
|------|------|------|-------------|
| **信号 (Push)** | 精确捕获事件时刻，不丢事件 | 需要 Send+Sync 闭包；关闭后的最后一个事件可能丢失 | map 计数、configure 计数 |
| **轮询 (Poll)** | 总能拿到当前快照；闭包简单（不需要跨线程安全） | 500ms 延迟；快速变化可能漏采 | 尺寸、mapped 状态、display 名称 |

两者互补：信号记录"发生了多少次"，轮询显示"现在是什么状态"。

### 3.2 信号捕获 configure 的局限性

`connect_notify("default-width")` 捕获的是 `default-width` 属性变化，而非每次 `size-allocate`。`default-width` 是构造属性，在窗口初始尺寸设定和程序化 resize 时变化，但**不一定响应每次用户拖拽**。更精确的做法是用 `GdkSurface::layout` 信号，但该信号在 gtk4-rs 0.7 中未暴露。

### 3.3 GTK4 窗口三层模型

```
GtkApplicationWindow    ← 应用层：title、child、present()
    └─ GdkSurface       ← GDK 层：mapped 状态、尺寸、scale
        └─ Display Server ← 系统层：Wayland compositor / X11 server
```

`GtkApplicationWindow` 持有逻辑属性（标题、默认尺寸），`GdkSurface` 是 display server 资源的代理。两者生命周期不完全重叠：
- Window 创建时 Surface 可能还不存在（`present()` 时才创建）
- Surface 销毁时 Window 可能还存在（unmap 后但未 destroy）

### 3.4 为什么用 `glib::timeout_add_local` 而非 `std::thread::spawn`

```rust
// ✓ 正确：在主线程中添加定时器
glib::timeout_add_local(500ms, move || { … });

// ✗ 错误：GTK 不是线程安全的，不能从其他线程操作 widget
std::thread::spawn(move || { label.set_text("…"); });  // UB!
```

GTK4 的所有 widget 操作必须在主线程（GLib 主循环线程）执行。`timeout_add_local` 确保回调在主循环中运行。
