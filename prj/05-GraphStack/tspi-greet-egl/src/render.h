#ifndef TSPI_GREET_EGL_RENDER_H
#define TSPI_GREET_EGL_RENDER_H

/*
 * GLES2 渲染模块 —— 最小化：编一对 vertex/fragment shader，
 * 用一个全屏四边形 + u_time uniform 驱动颜色渐变。
 *
 * 设计意图：让 demo 在 60 fps 下肉眼可见"动起来"，
 * 同时把所有 GLES 调用收口到一个 .c 文件，方便走读。
 */

/* 0 = OK, <0 = err. 必须在 eglMakeCurrent 之后调用。 */
int  render_init(void);

/* 画一帧到当前绑定的 EGL window surface。t 单位秒（自启动）。 */
void render_frame(int width, int height, float t);

void render_fini(void);

#endif
