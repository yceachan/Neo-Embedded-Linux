---
title: gtk4-rs 声明式 UI 与事件系统实现 — GreetPanel
tags: [gtk4-rs, UI-declarative, CSS, event-system, EventController]
desc: GreetPanel 数据模型、Widget 树构建流程、CSS 注入路径、clicked/enter/leave 三种事件的数据流
update: 2026-05-31
---


# 声明式 UI 与事件系统实现 — GreetPanel

> [!note]
> **Ref:** 源码 [`src/greet_panel.rs`](./src/greet_panel.rs), [`src/style.css`](./src/style.css)

> Aimed: 通过"输入名字 → 点击按钮 → 显示问候"的交互链路，演示 GTK4 的 Widget 树构建、CSS 样式注入、GObject 信号 + EventController 事件处理。

## 1. 数据模型

### 1.1 GreetPanel — 组件容器

```rust
pub struct GreetPanel {
    pub root: gtk::Box,           // 顶层容器，被 main.rs append 到 Notebook Tab
    greet_label: gtk::Label,      // "Hello, {name}!" 的输出标签
    event_log: gtk::Label,        // 事件日志（最近 5 条事件，最新的在顶部）
}
```

三个字段中 `root` 是 `pub`——外部（`main.rs`）通过 `greet.root` 将它加入 Tab。另外两个 label 是私有的，只在组件内部的信号回调中更新。

### 1.2 Widget 树（在 `new()` 中一次性构建）

```
gtk::Box (vertical, spacing=12)           ← root
├── gtk::Label       "Button Greet 演示"   .greet-title
├── gtk::Entry        placeholder text     .greet-entry
├── gtk::Button      "Greet !"             .greet-button
├── gtk::Label       "等待点击..."          .greet-result  ← greet_label
├── gtk::Separator   (horizontal)
├── gtk::Label       "事件日志:"           .greet-log-header
└── gtk::Label       "（等待交互...）"      .greet-log     ← event_log
```

每个 widget 通过 `root.append(&child)` 加入父容器。GTK4 用 `append` / `prepend` / `insert_child_after` 取代了 GTK3 的 `pack_start` / `pack_end`。

### 1.3 CSS 类名到 Widget 的映射

| CSS class | 应用到哪个 Widget | 样式作用 |
|-----------|------------------|---------|
| `.greet-title` | 标题 Label | 字号、颜色 |
| `.greet-entry` | 输入框 Entry | 背景色、圆角、`:focus` 边框高亮 |
| `.greet-button` | 按钮 Button | 背景色、圆角、`:hover` 变色、`:active` 深色 |
| `.greet-result` | 问候结果 Label | 绿色、加粗 |
| `.greet-log-header` / `.greet-log` | 日志区 | 等宽字体、暗色背景 |

## 2. 源码执行流程

### 2.1 初始化序列（`build_ui` → `GreetPanel::new()`）

```
main() → build_ui(app)
  → greet = GreetPanel::new()
      ├─ root = gtk::Box::new(vertical, spacing=12)     // ① 创建垂直盒子
      ├─ title = Label::new("Button Greet 演示")         // ② 逐层 append 子 widget
      │   root.append(&title)
      ├─ entry = Entry::new()                             // ③ 输入框
      │   entry.set_placeholder_text("输入你的名字...")
      │   root.append(&entry)
      ├─ button = Button::with_label("Greet !")           // ④ 按钮
      │   root.append(&button)
      ├─ greet_label = Label::new("等待点击...")           // ⑤ 结果标签
      │   root.append(&greet_label)
      ├─ sep = Separator::new()                           // ⑥ 分隔线
      │   root.append(&sep)
      ├─ log_header = Label::new("事件日志:")             // ⑦ 日志标题
      │   root.append(&log_header)
      ├─ event_log = Label::new("（等待交互...）")         // ⑧ 日志内容
      │   root.append(&event_log)
      │
      ├─ CSS 注入:                                        // ⑨ 全局样式
      │   css = CssProvider::new()
      │   css.load_from_data(include_str!("style.css"))   // 编译期嵌入
      │   style_context_add_provider_for_display(
      │       &display, &css, PRIORITY_APPLICATION)       // 优先级 600
      │
      ├─ 信号连接:                                        // ⑩ 三个事件回调
      │   button.connect_clicked(move |_| { … })          // GObject action signal
      │   motion_ctrl = EventControllerMotion::new()      // GTK4 input controller
      │   motion_ctrl.connect_enter(move |_, x, y| { … })
      │   motion_ctrl.connect_leave(move |_| { … })
      │   button.add_controller(motion_ctrl)
      │
      └─ return GreetPanel { root, greet_label, event_log }

  → notebook.append_page(&greet.root, tab_label)          // ⑪ 挂到 Notebook Tab 1
```

### 2.2 事件链路：用户点击 Greet 按钮

```
用户鼠标移入按钮
  → GTK CSS engine 检测 :hover 伪类
  → 自动应用 .greet-button:hover 样式（背景色 #1c71d8）
  → EventControllerMotion 发射 enter 信号
      → connect_enter 闭包:
          读取 event_log 当前文本
          在最前面拼接 "[enter] 鼠标进入按钮区域"
          截取前 5 行 → set_text() 写回

用户鼠标移出按钮
  → GTK CSS engine 移除 :hover 伪类
  → 恢复 .greet-button 默认样式（背景色 #3584e4）
  → connect_leave 闭包执行（同上流程）

用户点击按钮
  → GtkButton 内部状态机: pressed → released → clicked
  → button 发射 clicked 信号
      → connect_clicked 闭包:
          name = entry.text()                         // ① 读 Entry 文本
          if name.is_empty() { name = "世界" }         // ② 默认值
          greet_label.set_text("Hello, {name}!")       // ③ 更新问候标签
          读取 event_log 旧文本                         // ④ 更新事件日志
          拼接 "[clicked] 问候: \"{name}\""
          截取前 5 行 → set_text()
```

### 2.3 事件模型对比：信号 vs EventController

```
clicked  ──→ GObject Signal ──→ connect_clicked() ──→ 闭包
              (Button 内置)       (传统 API)

enter    ──→ EventController ──→ connect_enter()  ──→ 闭包
leave        (独立 controller)    (GTK4 新 API)
```

| 维度 | GObject Signal (`clicked`) | EventController (`enter`/`leave`) |
|------|---------------------------|-----------------------------------|
| 来源 | Widget 自身 | 独立 controller 对象 |
| 绑定 | `widget.connect_xxx(cb)` | `widget.add_controller(ctrl)` |
| 生命周期 | 与 widget 绑定 | 可独立 attach / detach |
| GTK3 → GTK4 | 无变化 | GTK3 的 `enter-notify-event` 已废弃，须改用 EventControllerMotion |

## 3. 关键实现细节

### 3.1 CSS 注入时机与范围

`style_context_add_provider_for_display` 对 **整个 Display** 的所有 widget 生效——不是只对某个 widget。这意味着 `.greet-button` 规则会影响 display 上任何带这个 class 的按钮。在本 demo 中只有一个 GreetPanel，所以无冲突。

**优先级**：`STYLE_PROVIDER_PRIORITY_APPLICATION` = 600，介于 `USER`（500）和 `THEME`（800）之间。用户主题可以覆盖应用样式。

### 3.2 CSS :hover 无代码驱动

GTK4 CSS 引擎在 widget 收到 pointer enter/leave 事件时**自动**切换 `:hover` 伪类。不需要任何 Rust 代码——EventControllerMotion 的 enter/leave 回调只用于**事件日志记录**，不参与样式切换。关闭 EventControllerMotion 不影响按钮 hover 变色。

### 3.3 事件日志的"重写"策略

```
读取当前全文 → 前面插入新行 → 截取前5行 → 写回
```

Label 不支持增量追加（只有一个 `set_text`），所以每次事件都重写整个文本。5 行限制防止日志无限增长。

### 3.4 Widget 引用克隆的开销

`button`, `entry`, `greet_label`, `event_log` 被多个闭包捕获时使用 `.clone()`——这些是 GTK widget，clone 只是增加 `GObject` 的内部引用计数（类似 `Arc::clone`），不需要深拷贝 widget 树。
