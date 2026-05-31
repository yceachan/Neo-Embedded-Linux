---
title: gtk4-rs 3D 渲染管线实现 — CubeRenderer
tags: [gtk4-rs, OpenGL, GLArea, rendering, shader]
desc: CubeRenderer 数据模型、源码执行流程、shader 编译细节、MVP 计算、GL FFI 策略
update: 2026-05-31
---


# 3D 渲染管线实现 — CubeRenderer

> [!note]
> **Ref:** 源码 [`src/cube_renderer.rs`](./src/cube_renderer.rs), [`src/gl.rs`](./src/gl.rs), [`src/math.rs`](./src/math.rs)

> Aimed: 通过 `gtk::GLArea` 在 GTK4 窗口中嵌入 OpenGL 3.3 渲染上下文，实现绕 Y 轴旋转的六色立方体。

## 1. 数据模型

### 1.1 CubeRenderer — 持有全部 GL 资源

```rust
pub struct CubeRenderer {
    program: GLuint,       // 链接后的 shader program ID（glCreateProgram 返回值）
    vao: GLuint,           // Vertex Array Object — 封装 VBO + 顶点属性布局
    vbo: GLuint,           // Vertex Buffer Object — 36 个立方体顶点的 GPU 存储
    u_mvp_loc: GLint,      // shader 中 uniform mat4 uMVP 的 location（用于每帧更新矩阵）
    rotation: f32,         // 当前累计旋转角（弧度），每帧递增 ~0.025 rad
    last_time: Option<Instant>,  // 上一帧时刻，用于计算 dt
}
```

**字段生命周期**：

```
realize()                     render()                    unrealize()
   │                              │                            │
   ├─ program ← glCreateProgram   ├─ glUseProgram(program)     ├─ glDeleteProgram(program)
   ├─ vao     ← glGenVertexArrays ├─ glBindVertexArray(vao)    ├─ glDeleteVertexArrays(vao)
   ├─ vbo     ← glGenBuffers      ├─ glBindBuffer(vbo)         ├─ glDeleteBuffers(vbo)
   └─ u_mvp_loc ← glGetUniform…   └─ glUniformMatrix4fv(…)     └─ (zeroed)
```

### 1.2 顶点数据布局 (stride = 36 bytes)

```
offset 0   → vec3 position  (12 bytes, float×3)
offset 12  → vec3 normal    (12 bytes, float×3)
offset 24  → vec3 color     (12 bytes, float×3)
─────────────────────────────────
total = 36 bytes per vertex
```

36 个顶点（6 面 × 2 三角形 × 3 顶点），无索引缓冲，直接 `glDrawArrays(GL_TRIANGLES, 0, 36)`。

**为什么 36 个独立顶点**：每个面的法线方向不同，无法在不同面之间共享顶点（共享顶点的前提是 position + normal + color 完全相同）。

### 1.3 gl.rs — OpenGL 3.3 FFI 符号表

```rust
#[link(name = "GL")]   // 链接 libGL.so (libglvnd)
extern "C" {
    pub fn glCreateShader(shader_type: GLenum) -> GLuint;
    pub fn glShaderSource(...);
    pub fn glCompileShader(...);
    // … 共 26 个 entry point
}
```

**为什么不用 `gleam` / `gl` crate**：保持零 OpenGL 依赖，FFI 声明即文档。26 个函数覆盖了本 demo 全部需求（shader、VBO/VAO、uniform、draw），比 `gl` crate 的 ~500 个声明更易审核。

## 2. 源码执行流程

### 2.1 入口：`main.rs` → GLArea 创建 + 回调注册

```
main()
  → Application::new("com.neo.demo.gtk4rs")
  → connect_activate(build_ui)
  → app.run()
      → build_ui(app)
          → GLArea::new()
          → gl_area.set_required_version(3, 3)     // ① 要求 OpenGL 3.3
          → gl_area.connect_realize(realize_cb)      // ② context 就绪时调用
          → gl_area.connect_unrealize(unrealize_cb)  // ③ context 销毁前调用
          → gl_area.connect_render(render_cb)        // ④ 每帧绘制
          → gl_area.add_tick_callback(tick_cb)       // ⑤ 帧时钟驱动动画
```

### 2.2 阶段 1：GL Context 初始化 (`realize_cb`)

```
realize_cb(gl_area)
  → gl_area.make_current()          // 确保 GL context 是当前线程的 active context
  → CubeRenderer::realize()
      → compile_shader(VERT, GL_VERTEX_SHADER)    // 编译 vertex shader
          glCreateShader → glShaderSource → glCompileShader → glGetShaderiv(COMPILE_STATUS)
      → compile_shader(FRAG, GL_FRAGMENT_SHADER)  // 编译 fragment shader
      → link_program(vs, fs)                      // 链接为 GPU program
          glCreateProgram → glAttachShader×2 → glLinkProgram → glGetProgramiv(LINK_STATUS)
          → glDeleteShader(vs)  // 链接后可删除 shader 中间对象
          → glDeleteShader(fs)
      → glGetUniformLocation(program, "uMVP")     // 查找 uniform 位置
      → cube_vertices() → [f32; 36×9]             // 生成顶点数组 (栈上)
      → glGenVertexArrays(1, &vao)                // 创建 VAO
      → glGenBuffers(1, &vbo)                     // 创建 VBO
      → glBindVertexArray(vao)
          glBindBuffer(GL_ARRAY_BUFFER, vbo)
          glBufferData(...)                        // 上传 36×9×4=1.3KB 到 GPU
          glVertexAttribPointer(0, 3, FLOAT, stride=36, off=0)   // attr 0: position
          glEnableVertexAttribArray(0)
          glVertexAttribPointer(1, 3, FLOAT, stride=36, off=12)  // attr 1: normal
          glEnableVertexAttribArray(1)
          glVertexAttribPointer(2, 3, FLOAT, stride=36, off=24)  // attr 2: color
          glEnableVertexAttribArray(2)
      → glBindVertexArray(0)  // unbind，防止后续意外修改
  → eprintln!("[main] GL context ready")
```

### 2.3 阶段 2：动画循环 (每帧 ~16ms)

```
GTK Frame Clock (vsync)
  → tick_cb(gl_area, frame_clock)
      → CubeRenderer::update()
          now = Instant::now()
          dt = now - last_time           // ~0.016s at 60fps
          rotation += dt * 1.57          // 约 90°/s
          last_time = now
      → gl_area.queue_draw()             // 请求 GTK 触发 render 信号
  → render_cb(gl_area, gl_context)
      → CubeRenderer::draw(w, h)
          → if w<=0 || h<=0 → return     // widget 未分配尺寸，跳过
          → glEnable(GL_DEPTH_TEST)
          → glDepthFunc(GL_LESS)         // 标准深度测试
          → glClearColor(0.08, 0.08, 0.12, 1.0)
          → glClear(COLOR | DEPTH)       // 清屏为深蓝灰色
          → glViewport(0, 0, w, h)       // 设置渲染区域
          → glUseProgram(program)        // 激活 shader
          → 计算 MVP:
              proj = perspective(45°, aspect, 0.1, 100.0)
              view = lookAt(eye=(0,1.5,5), center=(0,0,0), up=(0,1,0))
              model = rotateY(rotation) * rotateX(0.4)
              mvp = proj * view * model
          → glUniformMatrix4fv(u_mvp_loc, 1, GL_TRUE, mvp)  // 上传 MVP
          → glBindVertexArray(vao)
          → glDrawArrays(GL_TRIANGLES, 0, 36)               // 绘制 36 顶点
          → glBindVertexArray(0)                             // unbind
```

### 2.4 阶段 3：资源销毁 (`unrealize_cb`)

```
unrealize_cb(gl_area)
  → gl_area.make_current()
  → CubeRenderer::free_gl()
      → glDeleteProgram(program)
      → glDeleteBuffers(1, &vbo)
      → glDeleteVertexArrays(1, &vao)
```

## 3. 关键实现细节

### 3.1 矩阵存储约定

四个矩阵函数（`perspective`, `look_at`, `rotate_y`, `rotate_x`）均按 **row-major** 格式写入 `Mat4.0[row][col]`。上传时用 `glUniformMatrix4fv(…, GL_TRUE, …)` —— `GL_TRUE` 指示 OpenGL 在 GPU 端做一次转置，恢复为正确的列优先格式。

| 矩阵 | shader 中正确的形式 | 本代码 row-major 存储后对应 GL_TRUE |
|------|---------------------|--------------------------------------|
| perspective | `[0][0]=f/aspect, [1][1]=f, [2][2]=-(f+n)/(f-n), [2][3]=-2fn/(f-n), [3][2]=-1` | 逐元素写入 `m[row][col]` |
| look_at | 标准 view matrix | 同上 |
| rotate_y | `[0][0]=cos, [0][2]=sin, [2][0]=-sin, [2][2]=cos` | 同上 |

### 3.2 Shader 编译的错误处理

每次 `glCompileShader` / `glLinkProgram` 后立刻查询 `GL_COMPILE_STATUS` / `GL_LINK_STATUS`。失败时通过 `glGetShaderInfoLog` 读取编译器的错误信息并打印到 stderr——这是调试"为什么黑屏"的第一步诊断信息。

### 3.3 动画时间步进

使用 `Instant::now()` 而非帧计数——帧时钟频率可能变化（60/144Hz、compositor 降速），按实际时间计算旋转角保证动画速度恒定。

### 3.4 视口零尺寸保护

`draw()` 入口检查 `viewport_w <= 0 || viewport_h <= 0`——GLArea 可能在首帧尚未分配尺寸时触发 render，此时 `aspect` 计算会除以 0，`glViewport(0,0,0,0)` 会导致全屏无效。
