//! 3D rotating cube rendered via `gtk::GLArea` + OpenGL 3.3 core.
//!
//! Demonstrates the GTK4 rendering pipeline:
//!   `realize` → compile shaders + upload geometry
//!   `tick_callback` → update rotation MVP
//!   `render` → clear → draw → swap

use std::ffi::CString;
use std::ptr;
use crate::gl::*;
use crate::math::{self};

// ── geometry: cube with per-face colors + normals ──────────
// 6 faces × 2 triangles × 3 verts = 36 vertices, 9 floats each

const VERTEX_COUNT: i32 = 36;
const FLOATS_PER_VERT: usize = 9; // pos(3) + normal(3) + color(3)

fn cube_vertices() -> [f32; VERTEX_COUNT as usize * FLOATS_PER_VERT] {
    // Each face is defined as 4 corners in CCW winding from the front
    // Face helper: returns two triangles (6 vertices) for a face defined by 4 points
    fn face(points: [[f32; 3]; 4], normal: [f32; 3], color: [f32; 3]) -> Vec<f32> {
        let idx = [0, 1, 2, 0, 2, 3]; // triangulate quad
        let mut v = Vec::with_capacity(6 * 9);
        for &i in &idx {
            let p = points[i];
            v.extend_from_slice(&p);
            v.extend_from_slice(&normal);
            v.extend_from_slice(&color);
        }
        v
    }

    let c = 1.0f32; // half-extent

    // color palette — six distinct saturated colors
    let red    = [1.0, 0.2, 0.2];
    let blue   = [0.2, 0.3, 1.0];
    let green  = [0.2, 0.8, 0.3];
    let yellow = [0.9, 0.9, 0.2];
    let cyan   = [0.2, 0.8, 0.9];
    let magenta= [0.8, 0.3, 0.8];

    let mut verts: Vec<f32> = Vec::with_capacity(36 * 9);

    // +Z front (red)
    verts.extend(face(
        [[-c, -c,  c], [ c, -c,  c], [ c,  c,  c], [-c,  c,  c]],
        [0.0, 0.0, 1.0], red,
    ));
    // -Z back (blue)
    verts.extend(face(
        [[ c, -c, -c], [-c, -c, -c], [-c,  c, -c], [ c,  c, -c]],
        [0.0, 0.0, -1.0], blue,
    ));
    // -X left (green)
    verts.extend(face(
        [[-c, -c, -c], [-c, -c,  c], [-c,  c,  c], [-c,  c, -c]],
        [-1.0, 0.0, 0.0], green,
    ));
    // +X right (yellow)
    verts.extend(face(
        [[ c, -c,  c], [ c, -c, -c], [ c,  c, -c], [ c,  c,  c]],
        [1.0, 0.0, 0.0], yellow,
    ));
    // +Y top (cyan)
    verts.extend(face(
        [[-c,  c,  c], [ c,  c,  c], [ c,  c, -c], [-c,  c, -c]],
        [0.0, 1.0, 0.0], cyan,
    ));
    // -Y bottom (magenta)
    verts.extend(face(
        [[-c, -c, -c], [ c, -c, -c], [ c, -c,  c], [-c, -c,  c]],
        [0.0, -1.0, 0.0], magenta,
    ));

    let mut arr = [0.0f32; VERTEX_COUNT as usize * FLOATS_PER_VERT];
    arr.copy_from_slice(&verts);
    arr
}

// ── shaders ───────────────────────────────────────────────

const VERTEX_SHADER: &str = r#"#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

uniform mat4 uMVP;

out vec3 vColor;
out vec3 vNormal;
out vec3 vFragPos;

void main() {
    vec4 worldPos = vec4(aPos, 1.0); // model is identity, so worldPos == aPos
    gl_Position = uMVP * worldPos;
    vColor = aColor;
    vNormal = aNormal;
    vFragPos = aPos;
}
"#;

const FRAGMENT_SHADER: &str = r#"#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec3 vFragPos;

out vec4 fragColor;

void main() {
    // Simple directional light from upper-right-front
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.8));
    // Ambient base so shadowed faces aren't pitch black
    float ambient = 0.25;
    float diff = max(dot(normalize(vNormal), lightDir), 0.0);
    float lighting = ambient + (1.0 - ambient) * diff;
    fragColor = vec4(vColor * lighting, 1.0);
}
"#;

// ── helpers ───────────────────────────────────────────────

/// Compile a single shader. Returns 0 on failure (logs to stderr).
unsafe fn compile_shader(src: &str, shader_type: GLenum) -> GLuint {
    let shader = glCreateShader(shader_type);
    if shader == 0 {
        eprintln!("glCreateShader({}) returned 0", shader_type);
        return 0;
    }
    let cstr = CString::new(src).unwrap();
    let cstr_ptr: *const GLchar = cstr.as_ptr() as *const GLchar;
    let len = src.len() as GLint;
    glShaderSource(shader, 1, &cstr_ptr, &len);
    glCompileShader(shader);

    let mut ok: GLint = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &mut ok);
    if ok == 0 {
        let mut log_len: GLint = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &mut log_len);
        if log_len > 1 {
            let mut buf = vec![0u8; log_len as usize];
            glGetShaderInfoLog(shader, log_len, ptr::null_mut(), buf.as_mut_ptr() as *mut GLchar);
            eprintln!(
                "shader compile error (type={}): {}",
                shader_type,
                String::from_utf8_lossy(&buf[..log_len as usize - 1])
            );
        }
        glDeleteShader(shader);
        return 0;
    }
    shader
}

/// Link vertex + fragment into a program. Returns 0 on failure.
unsafe fn link_program(vs: GLuint, fs: GLuint) -> GLuint {
    let prog = glCreateProgram();
    if prog == 0 {
        eprintln!("glCreateProgram() returned 0");
        return 0;
    }
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    let mut ok: GLint = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &mut ok);
    if ok == 0 {
        let mut log_len: GLint = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &mut log_len);
        if log_len > 1 {
            let mut buf = vec![0u8; log_len as usize];
            glGetProgramInfoLog(prog, log_len, ptr::null_mut(), buf.as_mut_ptr() as *mut GLchar);
            eprintln!(
                "program link error: {}",
                String::from_utf8_lossy(&buf[..log_len as usize - 1])
            );
        }
        glDeleteProgram(prog);
        return 0;
    }
    prog
}

// ── CubeRenderer ─────────────────────────────────────────

pub struct CubeRenderer {
    program: GLuint,
    vao: GLuint,
    vbo: GLuint,
    u_mvp_loc: GLint,
    rotation: f32,
    last_time: Option<std::time::Instant>,
}

impl CubeRenderer {
    pub fn new() -> Self {
        CubeRenderer {
            program: 0,
            vao: 0,
            vbo: 0,
            u_mvp_loc: -1,
            rotation: 0.0,
            last_time: None,
        }
    }

    /// Called from GLArea `realize` signal.  Must be called while GL context is current.
    /// Returns true on success.
    pub unsafe fn realize(&mut self) -> bool {
        // compile shaders
        let vs = compile_shader(VERTEX_SHADER, GL_VERTEX_SHADER);
        let fs = compile_shader(FRAGMENT_SHADER, GL_FRAGMENT_SHADER);
        if vs == 0 || fs == 0 {
            if vs != 0 { glDeleteShader(vs); }
            if fs != 0 { glDeleteShader(fs); }
            return false;
        }

        self.program = link_program(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        if self.program == 0 {
            return false;
        }

        // uniform location
        let name = CString::new("uMVP").unwrap();
        self.u_mvp_loc = glGetUniformLocation(self.program, name.as_ptr() as *const GLchar);

        // geometry
        let verts = cube_vertices();
        glGenVertexArrays(1, &mut self.vao);
        glGenBuffers(1, &mut self.vbo);

        glBindVertexArray(self.vao);
        glBindBuffer(GL_ARRAY_BUFFER, self.vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            (verts.len() * std::mem::size_of::<f32>()) as GLsizeiptr,
            verts.as_ptr() as *const std::ffi::c_void,
            GL_STATIC_DRAW,
        );

        // stride = 9 floats (pos + normal + color) * 4 bytes
        let stride = (FLOATS_PER_VERT * std::mem::size_of::<f32>()) as GLsizei;

        // location 0: position (3f, offset 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, ptr::null());
        glEnableVertexAttribArray(0);

        // location 1: normal (3f, offset 12)
        glVertexAttribPointer(
            1,
            3,
            GL_FLOAT,
            GL_FALSE,
            stride,
            (3 * std::mem::size_of::<f32>()) as *const std::ffi::c_void,
        );
        glEnableVertexAttribArray(1);

        // location 2: color (3f, offset 24)
        glVertexAttribPointer(
            2,
            3,
            GL_FLOAT,
            GL_FALSE,
            stride,
            (6 * std::mem::size_of::<f32>()) as *const std::ffi::c_void,
        );
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);

        eprintln!(
            "[cube_renderer] realize: program={}, vao={}, vbo={}, u_mvp={}",
            self.program, self.vao, self.vbo, self.u_mvp_loc
        );
        true
    }

    /// Update rotation angle from elapsed time. Call each frame before draw.
    pub fn update(&mut self) {
        let now = std::time::Instant::now();
        let dt = match self.last_time {
            Some(t) => now.duration_since(t).as_secs_f32(),
            None => 0.016,
        };
        self.last_time = Some(now);
        // ~90° per second
        self.rotation += dt * 1.57;
        if self.rotation > 2.0 * std::f32::consts::PI {
            self.rotation -= 2.0 * std::f32::consts::PI;
        }
    }

    /// Draw one frame. Must be called while GL context is current.
    pub unsafe fn draw(&self, viewport_w: i32, viewport_h: i32) {
        // Guard: skip rendering if the widget hasn't been sized yet
        if viewport_w <= 0 || viewport_h <= 0 {
            return;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        // Disable face culling — the cube is simple enough, and incorrect
        // winding order is a common cause of "black screen" during bring-up.
        // glEnable(GL_CULL_FACE);

        glClearColor(0.08, 0.08, 0.12, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, viewport_w as GLsizei, viewport_h as GLsizei);

        glUseProgram(self.program);

        // Build MVP (row-major matrices → GL_TRUE transpose for correct upload)
        let aspect = viewport_w as f32 / (viewport_h.max(1) as f32);
        let proj = math::perspective(std::f32::consts::FRAC_PI_4, aspect, 0.1, 100.0);
        let view = math::look_at([0.0, 1.5, 5.0], [0.0, 0.0, 0.0], [0.0, 1.0, 0.0]);
        let model = math::rotate_y(self.rotation) * math::rotate_x(0.4);
        let mvp = proj * view * model;

        if self.u_mvp_loc >= 0 {
            glUniformMatrix4fv(self.u_mvp_loc, 1, GL_TRUE, mvp.as_ptr());
        }

        glBindVertexArray(self.vao);
        glDrawArrays(GL_TRIANGLES, 0, VERTEX_COUNT);
        glBindVertexArray(0);
    }
}

impl Drop for CubeRenderer {
    fn drop(&mut self) {
        // GL resources must be freed while context is current.
        // We store the IDs and print a warning if they weren't freed.
        if self.program != 0 || self.vao != 0 || self.vbo != 0 {
            eprintln!(
                "[cube_renderer] WARNING: GL resources not freed before Drop (prog={} vao={} vbo={}). \
                 Call free_gl() while context is current.",
                self.program, self.vao, self.vbo
            );
        }
    }
}

impl CubeRenderer {
    /// Free GL resources. Must be called while GL context is current (e.g. in GLArea `unrealize`).
    pub unsafe fn free_gl(&mut self) {
        if self.program != 0 {
            glDeleteProgram(self.program);
            self.program = 0;
        }
        if self.vbo != 0 {
            glDeleteBuffers(1, &self.vbo);
            self.vbo = 0;
        }
        if self.vao != 0 {
            glDeleteVertexArrays(1, &self.vao);
            self.vao = 0;
        }
    }
}
