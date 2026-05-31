//! Minimal 4×4 matrix for MVP (Model-View-Projection) calculation.
//! Kept inline instead of pulling nalgebra — the demo ships with no math deps.

use std::ops::Mul;

#[derive(Debug, Clone, Copy)]
pub struct Mat4(pub [[f32; 4]; 4]);

impl Mat4 {
    pub const fn identity() -> Self {
        let mut m = [[0.0f32; 4]; 4];
        m[0][0] = 1.0;
        m[1][1] = 1.0;
        m[2][2] = 1.0;
        m[3][3] = 1.0;
        Mat4(m)
    }

    pub fn as_ptr(&self) -> *const f32 {
        self.0.as_ptr() as *const f32
    }
}

impl Mul<Mat4> for Mat4 {
    type Output = Mat4;
    fn mul(self, rhs: Mat4) -> Mat4 {
        let a = self.0;
        let b = rhs.0;
        let mut r = [[0.0f32; 4]; 4];
        for i in 0..4 {
            for j in 0..4 {
                r[i][j] = a[i][0] * b[0][j]
                        + a[i][1] * b[1][j]
                        + a[i][2] * b[2][j]
                        + a[i][3] * b[3][j];
            }
        }
        Mat4(r)
    }
}

/// Build a perspective projection matrix (right-handed, OpenGL [-1,1] depth).
/// Stored row-major — caller must pass GL_TRUE to glUniformMatrix4fv for correct upload.
pub fn perspective(fov_y_rad: f32, aspect: f32, near: f32, far: f32) -> Mat4 {
    let f = 1.0 / (fov_y_rad / 2.0).tan();
    let mut m = Mat4::identity();
    m.0[0][0] = f / aspect;
    m.0[1][1] = f;
    m.0[2][2] = -(far + near) / (far - near);
    m.0[2][3] = -2.0 * far * near / (far - near);
    m.0[3][2] = -1.0;
    m.0[3][3] = 0.0;
    m
}

/// Build a look-at view matrix (right-handed).
pub fn look_at(eye: [f32; 3], center: [f32; 3], up: [f32; 3]) -> Mat4 {
    let f = normalize(sub(center, eye));
    let s = normalize(cross(f, up));
    let u = cross(s, f);

    // Stored row-major (compatible with GL_TRUE for glUniformMatrix4fv).
    // Mathematically this is the standard view matrix:
    // |  s.x   s.y   s.z  -s·e |
    // |  u.x   u.y   u.z  -u·e |
    // | -f.x  -f.y  -f.z   f·e |
    // |   0     0     0     1  |
    let mut m = Mat4::identity();
    m.0[0][0] = s[0]; m.0[0][1] = s[1]; m.0[0][2] = s[2];
    m.0[1][0] = u[0]; m.0[1][1] = u[1]; m.0[1][2] = u[2];
    m.0[2][0] = -f[0]; m.0[2][1] = -f[1]; m.0[2][2] = -f[2];
    m.0[0][3] = -dot(s, eye);
    m.0[1][3] = -dot(u, eye);
    m.0[2][3] = dot(f, eye);
    m
}

/// Build a rotation matrix around the Y axis (angle in radians).
/// Stored row-major — use with GL_TRUE in glUniformMatrix4fv.
pub fn rotate_y(angle: f32) -> Mat4 {
    let (s, c) = angle.sin_cos();
    let mut m = Mat4::identity();
    m.0[0][0] = c;
    m.0[0][2] = s;   // row 0: [c, 0, s, 0]
    m.0[2][0] = -s;
    m.0[2][2] = c;   // row 2: [-s, 0, c, 0]
    m
}

/// Build a rotation matrix around the X axis (angle in radians).
/// Stored row-major — use with GL_TRUE in glUniformMatrix4fv.
pub fn rotate_x(angle: f32) -> Mat4 {
    let (s, c) = angle.sin_cos();
    let mut m = Mat4::identity();
    m.0[1][1] = c;
    m.0[1][2] = -s;  // row 1: [0, c, -s, 0]
    m.0[2][1] = s;
    m.0[2][2] = c;   // row 2: [0, s, c, 0]
    m
}

// --- helpers ---

fn sub(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}

fn dot(a: [f32; 3], b: [f32; 3]) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

fn cross(a: [f32; 3], b: [f32; 3]) -> [f32; 3] {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

fn normalize(v: [f32; 3]) -> [f32; 3] {
    let len = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if len > 1e-8 {
        [v[0] / len, v[1] / len, v[2] / len]
    } else {
        [0.0, 0.0, 0.0]
    }
}
