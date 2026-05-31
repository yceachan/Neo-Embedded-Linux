//! Minimal OpenGL 3.3 FFI — only the ~26 entry points needed for the cube demo.
//! Links directly to `libGL.so` via libglvnd.

#![allow(non_camel_case_types)]
#![allow(dead_code)]

// ── types ──────────────────────────────────────────
pub type GLenum = u32;
pub type GLboolean = u8;
pub type GLint = i32;
pub type GLsizei = i32;
pub type GLuint = u32;
pub type GLfloat = f32;
pub type GLchar = i8;
pub type GLsizeiptr = isize;
pub type GLintptr = isize;

// ── consts ─────────────────────────────────────────
pub const GL_VERTEX_SHADER: GLenum = 0x8B31;
pub const GL_FRAGMENT_SHADER: GLenum = 0x8B30;
pub const GL_COMPILE_STATUS: GLenum = 0x8B81;
pub const GL_LINK_STATUS: GLenum = 0x8B82;
pub const GL_INFO_LOG_LENGTH: GLenum = 0x8B84;
pub const GL_ARRAY_BUFFER: GLenum = 0x8892;
pub const GL_ELEMENT_ARRAY_BUFFER: GLenum = 0x8893;
pub const GL_STATIC_DRAW: GLenum = 0x88E4;
pub const GL_FLOAT: GLenum = 0x1406;
pub const GL_TRIANGLES: GLenum = 0x0004;
pub const GL_UNSIGNED_INT: GLenum = 0x1405;
pub const GL_DEPTH_TEST: GLenum = 0x0B71;
pub const GL_LESS: GLenum = 0x0201;
pub const GL_CULL_FACE: GLenum = 0x0B44;
pub const GL_BACK: GLenum = 0x0405;
pub const GL_COLOR_BUFFER_BIT: GLenum = 0x4000;
pub const GL_DEPTH_BUFFER_BIT: GLenum = 0x0100;

pub const GL_FALSE: GLboolean = 0;
pub const GL_TRUE: GLboolean = 1;

// ── FFI ────────────────────────────────────────────
#[link(name = "GL")]
extern "C" {
    // shader
    pub fn glCreateShader(shader_type: GLenum) -> GLuint;
    pub fn glShaderSource(
        shader: GLuint,
        count: GLsizei,
        string: *const *const GLchar,
        length: *const GLint,
    );
    pub fn glCompileShader(shader: GLuint);
    pub fn glGetShaderiv(shader: GLuint, pname: GLenum, params: *mut GLint);
    pub fn glGetShaderInfoLog(
        shader: GLuint,
        buf_size: GLsizei,
        length: *mut GLsizei,
        info_log: *mut GLchar,
    );
    pub fn glDeleteShader(shader: GLuint);

    // program
    pub fn glCreateProgram() -> GLuint;
    pub fn glAttachShader(program: GLuint, shader: GLuint);
    pub fn glLinkProgram(program: GLuint);
    pub fn glGetProgramiv(program: GLuint, pname: GLenum, params: *mut GLint);
    pub fn glGetProgramInfoLog(
        program: GLuint,
        buf_size: GLsizei,
        length: *mut GLsizei,
        info_log: *mut GLchar,
    );
    pub fn glUseProgram(program: GLuint);
    pub fn glDeleteProgram(program: GLuint);

    // VAO / VBO / EBO
    pub fn glGenVertexArrays(n: GLsizei, arrays: *mut GLuint);
    pub fn glBindVertexArray(array: GLuint);
    pub fn glDeleteVertexArrays(n: GLsizei, arrays: *const GLuint);
    pub fn glGenBuffers(n: GLsizei, buffers: *mut GLuint);
    pub fn glBindBuffer(target: GLenum, buffer: GLuint);
    pub fn glBufferData(
        target: GLenum,
        size: GLsizeiptr,
        data: *const std::ffi::c_void,
        usage: GLenum,
    );
    pub fn glDeleteBuffers(n: GLsizei, buffers: *const GLuint);

    // vertex attrib
    pub fn glVertexAttribPointer(
        index: GLuint,
        size: GLint,
        type_: GLenum,
        normalized: GLboolean,
        stride: GLsizei,
        pointer: *const std::ffi::c_void,
    );
    pub fn glEnableVertexAttribArray(index: GLuint);

    // uniform
    pub fn glGetUniformLocation(program: GLuint, name: *const GLchar) -> GLint;
    pub fn glUniformMatrix4fv(
        location: GLint,
        count: GLsizei,
        transpose: GLboolean,
        value: *const GLfloat,
    );

    // draw / state
    pub fn glClear(mask: GLenum);
    pub fn glClearColor(r: GLfloat, g: GLfloat, b: GLfloat, a: GLfloat);
    pub fn glViewport(x: GLint, y: GLint, w: GLsizei, h: GLsizei);
    pub fn glEnable(cap: GLenum);
    pub fn glDepthFunc(func: GLenum);
    pub fn glDrawElements(
        mode: GLenum,
        count: GLsizei,
        type_: GLenum,
        indices: *const std::ffi::c_void,
    );
    pub fn glDrawArrays(mode: GLenum, first: GLint, count: GLsizei);
}
