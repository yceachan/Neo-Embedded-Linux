---
title: gtk4-rs GUI 范式演示
tags: [gtk4-rs, Rust, OpenGL, GUI, demo]
desc: 单窗口三 Tab 演示：GLArea 3D 渲染管线、Button-Greet 声明式 UI、窗口表面管理
update: 2026-05-31
---


# gtk4-rs GUI 范式演示

单窗口三 Tab 的 gtk4-rs 综合演示项目，在一个 GTK4 应用中同时展示 GUI 框架的三大核心范式。

> [!note]
> **Ref:**
> - 架构设计: [`Arch.md`](./Arch.md)
> - 现象分析: [`Phenomenon.md`](./Phenomenon.md)
> - 实现详解: [`Impl-CubeRenderer.md`](./Impl-CubeRenderer.md), [`Impl-GreetPanel.md`](./Impl-GreetPanel.md), [`Impl-SurfaceMonitor.md`](./Impl-SurfaceMonitor.md)

## 三 Tab 演示内容

| Tab | 内容 | 演示的范式 |
|-----|------|-----------|
| 🧊 3D 渲染管线 | GLArea + 绕轴旋转六色立方体 | OpenGL context 集成、shader 编译链接、MVP 变换、帧同步动画 |
| 🎨 声明式 UI | Button-Greet 交互组件 | Widget 树程序化构建、CSS 样式（含 `:hover`）、clicked + EventController 事件 |
| 🪟 窗口表面 | GdkSurface 状态实时监控 | map/unmap 生命周期、display server 检测、HiDPI scale、信号 + 轮询双轨追踪 |

## 依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| Rust | ≥1.70 | 编译工具链 |
| libgtk-4-dev | 4.6+ | GTK4 C 库 + headers |
| libgl-dev | any | OpenGL 链接（通过 libglvnd） |
| libepoxy-dev | any | GTK4 的 GL dispatch 依赖 |

### 安装依赖（Ubuntu/Debian）

```bash
sudo apt install libgtk-4-dev libgl-dev libepoxy-dev
```

## 快速开始

```bash
cd prj/05-GraphStack/gtk4-rs

# 一键构建 + 冒烟测试
./test.sh

# 或手动运行 GUI
cargo run
```

## 期望输出

启动后弹出 GTK4 窗口（960×680），顶部有三个 Tab 页签：

- **Tab 0**：深色背景上一个旋转的六色立方体，受方向光照射，面与面之间有明暗区分
- **Tab 1**：输入框 + 按钮 + 问候标签 + 事件日志。输入名字点击 Greet，标签更新；鼠标移入/移出按钮，日志记录事件
- **Tab 2**：窗口属性实时显示（mapped 状态、尺寸、display 名称、scale factor）

终端 stderr 输出：
```
[cube_renderer] realize: program=<非零>, vao=1, vbo=1, u_mvp=0
[main] GL context ready, renderer initialized.
```

如果看到 `program=<非零>` 且窗口正常显示，说明所有三条范式链路均正常工作。

## 文件索引

| 文件 | 作用 |
|------|------|
| `src/main.rs` | 入口：Application → Window → Notebook 三 Tab 组装 |
| `src/cube_renderer.rs` | 3D 渲染核心：shader 编译、VBO/VAO、MVP 计算、每帧 draw |
| `src/greet_panel.rs` | Button-Greet 组件：Widget 树、CSS 注入、信号连接 |
| `src/surface_monitor.rs` | 窗口状态监控：信号监听 + 500ms 定时轮询 |
| `src/gl.rs` | 最小化 OpenGL 3.3 FFI（26 个 entry point） |
| `src/math.rs` | 手写 4×4 矩阵（perspective、lookAt、rotate） |
| `src/style.css` | Button-Greet 面板 CSS 样式 |
| `src/surface.css` | Surface Monitor 面板 CSS 样式 |
| `test.sh` | 一键：clean → build → cargo run (4s smoke test) |
