---
title: gtk4-rs API 实战笔记
tags: [gtk4-rs, Rust, GUI, GTK4, API-reference]
desc: 基于 gtk4-rs 0.7 (GTK 4.6) 的实战 API 速查：Application/Window/Widget 树、GLArea 渲染、CSS 注入、事件系统(GObject Signal + EventController)、Rust 共享状态模式
update: 2026-05-31
---


# gtk4-rs API 实战笔记

> [!note]
> **Ref:**
> - demo: [`prj/05-GraphStack/gtk4-rs/`](/home/pi/imx/prj/05-GraphStack/gtk4-rs/)
> - [gtk4-rs 官方文档](https://gtk-rs.org/gtk4-rs/stable/latest/docs/gtk4/)
> - [GTK4 官方 API](https://docs.gtk.org/gtk4/)

基于 gtk4-rs 0.7.3（对应 GTK 4.6），记录本次 demo 用到的核心 API 和 Rust 侧的模式/陷阱。

## 1. Application 与 Window

### 1.1 最小启动模板

```rust
use gtk4::prelude::*;
use gtk4 as gtk;

fn main() {
    let app = gtk::Application::new(Some("com.example.app.id"), Default::default());
    app.connect_activate(|app| {
        let win = gtk::ApplicationWindow::new(app);
        win.set_title(Some("Title"));
        win.set_default_size(800, 600);
        win.present();
    });
    app.run();
}
```

**关键点**：
- `Application::new()` 第一个参数是 D-Bus 应用 ID。`None` 表示无 D-Bus 注册（适用于不需要单实例控制的简单工具）。
- `connect_activate` 在 app 启动时触发——这是创建窗口的标准时机。
- `present()` 请求显示窗口（内部触发 GdkSurface 创建和 display server 协商）。

### 1.2 Window 常用属性/方法

| API | 作用 |
|-----|------|
| `window.set_title(Some("..."))` | 标题栏文本 |
| `window.set_default_size(w, h)` | 初始尺寸（可被 compositor 覆盖） |
| `window.present()` | 显示窗口 |
| `window.set_child(Some(&widget))` | 设置窗口的唯一子 widget（GTK4 每个 Window 只有一个 child） |
| `window.is_mapped()` | widget 及其 surface 是否已映射到屏幕 |
| `window.width()` / `window.height()` | 当前分配的实际尺寸 |
| `window.scale_factor()` | HiDPI 缩放因子（1×, 2×） |
| `window.surface()` | 获取底层 `GdkSurface` 引用（mapped 后有效） |

## 2. Widget 树

### 2.1 容器与布局

GTK4 用 `append` / `prepend` / `insert_child_after` 替代了 GTK3 的 `pack_start` / `pack_end`：

```rust
let vbox = gtk::Box::new(gtk::Orientation::Vertical, 12);  // 垂直，12px 间距
vbox.append(&label1);
vbox.append(&button);
vbox.append(&label2);
```

| Widget | 用途 |
|--------|------|
| `gtk::Box` | 线性布局（垂直/水平），`spacing` 控制子元素间距 |
| `gtk::Grid` | 网格布局，`attach(&w, col, row, w_span, h_span)` |
| `gtk::Notebook` | Tab 分页容器，`append_page(&child, Some(&tab_label))` |
| `gtk::Label` | 文本标签，`new(Some("text"))` |
| `gtk::Button` | 按钮，`with_label("text")` |
| `gtk::Entry` | 单行文本输入，`set_placeholder_text()` |
| `gtk::Separator` | 分隔线 |
| `gtk::GLArea` | OpenGL 渲染区域（见 §4） |

### 2.2 Widget 通用方法

```rust
widget.set_halign(gtk::Align::Center);    // 水平对齐
widget.set_valign(gtk::Align::Center);    // 垂直对齐
widget.set_hexpand(true);                 // 水平方向自动扩展（填满可用空间）
widget.set_vexpand(true);                 // 垂直方向自动扩展
widget.set_margin_top(24);                // 外边距（四边独立设置）
widget.set_max_width_chars(30);           // Entry 最大字符宽度
widget.add_css_class("class-name");       // 关联 CSS class（不加 ".")
```

## 3. CSS 样式系统

### 3.1 注入 CSS

```rust
let css = gtk::CssProvider::new();
css.load_from_data(include_str!("style.css"));  // 编译期嵌入 .css 文件

gtk::style_context_add_provider_for_display(
    &gdk4::Display::default().expect("display"),
    &css,
    gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,  // 优先级 600
);
```

**优先级层级**（数值越大越高）：

| 优先级常量 | 数值 | 用途 |
|-----------|------|------|
| `STYLE_PROVIDER_PRIORITY_FALLBACK` | 1 | 系统回退 |
| `STYLE_PROVIDER_PRIORITY_THEME` | 200 | 主题 |
| `STYLE_PROVIDER_PRIORITY_SETTINGS` | 400 | 设置 |
| `STYLE_PROVIDER_PRIORITY_APPLICATION` | 600 | **应用层（推荐）** |
| `STYLE_PROVIDER_PRIORITY_USER` | 800 | 用户覆盖 |

### 3.2 CSS 写法（GTK4 子集）

```css
.greet-button {
    background: #3584e4;
    border-radius: 8px;           /* GTK4 支持 */
    padding: 12px 28px;
    transition: background 200ms; /* GTK4 支持 CSS transition */
}
.greet-button:hover {             /* GTK CSS 引擎自动切换伪类 */
    background: #1c71d8;
}
.greet-button:active {
    background: #15539e;
}
.greet-entry:focus {              /* 焦点状态 */
    border-color: #3584e4;
}
```

**关键事实**：`:hover` 和 `:active` 由 GTK CSS 引擎自动管理，不需要任何 Rust 代码。只在需要记录日志或触发额外逻辑时才用 EventController 监听 enter/leave 事件。

## 4. GLArea — OpenGL 渲染

### 4.1 生命周期与回调

```rust
let gl_area = gtk::GLArea::new();
gl_area.set_required_version(3, 3);          // 要求 OpenGL 3.3 Core Profile

// ① GL context 就绪 —— 创建 shader、VBO、VAO
gl_area.connect_realize(move |gl_area| {
    gl_area.make_current();
    unsafe { renderer.borrow_mut().realize(); }
});

// ② 每帧绘制 —— 计算 MVP、glDrawArrays
gl_area.connect_render(move |gl_area, _ctx| {
    unsafe { renderer.borrow().draw(gl_area.width(), gl_area.height()); }
    glib::Propagation::Stop    // 阻止默认 render（不做 double-draw）
});

// ③ context 销毁前 —— 释放 GPU 资源
gl_area.connect_unrealize(move |gl_area| {
    gl_area.make_current();
    unsafe { renderer.borrow_mut().free_gl(); }
});

// ④ 帧时钟同步 —— 驱动动画循环
gl_area.add_tick_callback(move |gl_area, _frame_clock| {
    renderer.borrow_mut().update();  // 更新状态
    gl_area.queue_draw();            // 请求重绘
    glib::ControlFlow::Continue      // 保持 callback 活跃
});
```

### 4.2 关键约束

| 约束 | 原因 |
|------|------|
| GL 资源创建必须在 `realize` 中 | `realize` 之后 context 才有效 |
| GL 资源销毁必须在 `unrealize` 中 | `unrealize` 之后 context 即将失效 |
| `render` 中不要创建/销毁资源 | 每帧调用，创建/销毁太重 |
| 每帧调用 `make_current()` 或依赖 GTK 自动管理 | context 可能被其他线程/窗口切换 |
| `queue_draw()` 不是同步绘制 | 标记 dirty，下一帧时钟才触发 render |

### 4.3 视口零尺寸保护

```rust
fn draw(&self, viewport_w: i32, viewport_h: i32) {
    if viewport_w <= 0 || viewport_h <= 0 {
        return;  // GLArea 首帧可能尚未分配尺寸
    }
    // ...
}
```

## 5. 事件系统

### 5.1 GObject Signal（动作型事件）

```rust
button.connect_clicked(move |_button| {
    // button 被点击
});

window.connect_map(move |_win| {
    // window 首次映射到屏幕
});
```

特点：Widget 内置信号，传统 API，GTK3→4 无变化。

### 5.2 EventController（输入型事件）

GTK4 废弃了 `GtkWidget` 的 `enter-notify-event` / `leave-notify-event` / `motion-notify-event` 等老信号，改为 EventController 模型：

```rust
let motion_ctrl = gtk::EventControllerMotion::new();

motion_ctrl.connect_enter(move |_ctrl, x: f64, y: f64| {
    // 指针进入 widget 区域，(x, y) 是 widget 内坐标
});

motion_ctrl.connect_leave(move |_ctrl| {
    // 指针离开 widget 区域
});

motion_ctrl.connect_motion(move |_ctrl, x: f64, y: f64| {
    // 指针在 widget 内移动
});

widget.add_controller(motion_ctrl);  // 绑定到 widget
```

**GTK3 → GTK4 信号迁移表**：

| GTK3 Widget Signal | GTK4 EventController |
|---------------------|---------------------|
| `enter-notify-event` | `EventControllerMotion::enter` |
| `leave-notify-event` | `EventControllerMotion::leave` |
| `motion-notify-event` | `EventControllerMotion::motion` |
| `button-press-event` | `GestureClick::pressed` |
| `button-release-event` | `GestureClick::released` |
| `key-press-event` | `EventControllerKey::key-pressed` |
| `scroll-event` | `EventControllerScroll::scroll` |

**EventController vs Signal 的架构差异**：

| 维度 | GObject Signal | EventController |
|------|---------------|-----------------|
| 来源 | Widget 内置 | 独立 controller 对象 |
| 绑定 | `widget.connect_xxx(cb)` | `widget.add_controller(ctrl)` |
| 生命周期 | 与 widget 绑定 | 可独立 attach/detach |
| 设计意图 | 高层语义（动作完成了） | 低层输入抽象（指针位置、按键状态） |

## 6. Rust 共享状态模式

### 6.1 `Rc<RefCell<T>>` — 单线程共享可变状态

```rust
use std::rc::Rc;
use std::cell::RefCell;

let renderer = Rc::new(RefCell::new(CubeRenderer::new()));

// 克隆 Rc 指针（只增加引用计数，不复制数据）
let r1 = renderer.clone();
let r2 = renderer.clone();

// 在不同闭包中分别读写
gl_area.connect_realize(move |ga| {
    r1.borrow_mut().realize();  // &mut（运行时检查，panic 如果已借出）
});

gl_area.connect_render(move |ga, _ctx| {
    let r = r2.borrow();        // &（共享借用）
    r.draw(ga.width(), ga.height());
});
```

**这是 Rust 中单线程共享可变状态的标准模式**：

- `Rc<T>` — 非原子的引用计数指针，单线程，零开销
- `RefCell<T>` — 将借用检查从编译期推迟到运行时：`borrow_mut()` 返回 `RefMut<T>`（排他），`borrow()` 返回 `Ref<T>`（共享）
- 违反借用规则（两个 `borrow_mut()` 同时存在）→ **运行期 panic**，不会编译错误
- 对应多线程场景下的 `Arc<Mutex<T>>` / `Arc<RwLock<T>>`

### 6.2 `Arc<AtomicU32>` — 线程安全的信号闭包

```rust
use std::sync::Arc;
use std::sync::atomic::{AtomicU32, Ordering};

let count = Arc::new(AtomicU32::new(0));

window.connect_map({
    let c = count.clone();
    move |_| {
        c.fetch_add(1, Ordering::Relaxed);
    }
});
```

**为什么不能用 `Rc<RefCell<u32>>`**：GTK4 信号闭包的约束是 `Fn + Send + Sync + 'static`。信号可能被 display server 线程触发，GTK 要求闭包线程安全：

- `Rc<T>` — **不实现** `Send`（非原子引用计数）
- `RefCell<T>` — **不实现** `Sync`（运行时借用检查非线程安全）
- `Arc<T>` — 实现 `Send + Sync`（当 `T: Send + Sync`）
- `AtomicU32` — 实现 `Send + Sync`（硬件原子操作，零额外开销）

### 6.3 `Arc<AtomicU32>` vs `Arc<Mutex<u32>>`

| 维度 | `Arc<AtomicU32>` | `Arc<Mutex<u32>>` |
|------|-----------------|-------------------|
| 开销 | 单条 CPU 原子指令 | 系统调用（futex）可能 |
| 适用场景 | 简单计数器、flag | 需要读-改-写多个字段时 |
| 阻塞 | 无锁 | 可能阻塞等待锁 |
| API | `fetch_add`, `load`, `store` | `lock().unwrap()` → `*v += 1` |

**选择原则**：简单计数器用 `AtomicU32`（零开销），复杂状态共享用 `Mutex`。本 demo 的 map/config 计数只需要自增，`AtomicU32` 是最佳选择。

### 6.4 Widget Clone 的开销

```rust
let label1 = gtk::Label::new(Some("text"));
let label2 = label1.clone();  // 不是深拷贝！
```

GTK widget 的 `Clone` 实现只增加底层 `GObject` 的引用计数，类似 `Arc::clone`——所有 "clone" 出来的变量指向同一个 GTK widget。这意味着在多个闭包中捕获同一个 widget 的成本极低。

## 7. GdkSurface 与 Display

### 7.1 三层模型

```
GtkApplicationWindow    ← 应用层：title, child, present()
    └─ GdkSurface       ← GDK 层：mapped 状态, 尺寸, scale
        └─ Display Server ← 系统层：Wayland compositor / X11 server
```

### 7.2 常用查询

```rust
use gdk4::prelude::DisplayExt;

// Display 信息
let display = gdk4::Display::default().expect("no display");
let name = display.name();  // "wayland-0" 或 ":0"

// Surface mapped 状态
let mapped = window.is_mapped();  // GtkWidget 级

// 环境检测
let is_wayland = std::env::var("WAYLAND_DISPLAY").is_ok();
let is_x11 = std::env::var("DISPLAY").is_ok();
```

### 7.3 定时轮询

```rust
glib::timeout_add_local(
    std::time::Duration::from_millis(500),
    move || {
        // 轮询窗口状态...
        glib::ControlFlow::Continue  // 保持定时器活跃
    }
);
```

`timeout_add_local` 确保回调在 GLib 主线程执行——可以安全操作 GTK widget。**不要**用 `std::thread::spawn` 操作 widget（GTK 不是线程安全的）。

## 8. `connect_notify` — 属性变化监听

```rust
window.connect_notify(Some("default-width"), move |_, _| {
    // default-width 属性变化时触发
});
```

`connect_notify` 是 GObject 属性变化的通用监听。参数 `Some("property-name")` 指定监听的属性，`None` 表示监听所有属性。

**本 demo 用 `default-width` / `default-height` 作为 resize 的代理信号**——注意这不完全精确（这两个是构造属性，不是每次 `size-allocate` 都变化），但在 gtk4-rs 0.7 中 `GdkSurface::size-changed` 信号未暴露，这是最接近的替代。

## 9. Crate 依赖

```toml
[dependencies]
gtk4 = "0.7"     # GTK4 widgets
gdk4 = "0.7"     # GDK surface/display（独立 crate，非 gtk4 重导出）
glib = "0.18"    # GLib 基础类型（Propagation, ControlFlow, timeout_add_local）
```

**注意**：gtk4-rs 0.7 中 `gdk4` 和 `glib` 是独立 crate，不是 `gtk4` 的重导出。如果代码中用到 `gdk4::Display`、`glib::Propagation`、`glib::timeout_add_local`，必须在 `Cargo.toml` 中显式声明依赖。
