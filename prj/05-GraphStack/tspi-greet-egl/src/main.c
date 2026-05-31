/*
 * tspi-greet-egl — Weston kiosk-shell 全屏欢迎页客户端 (EGL/GLES2 版)
 *
 * 与 tspi-greet (SHM + cairo) 形成对照：
 *   - 客户端侧链 libEGL.so / libGLESv2.so / libwayland-egl.so
 *   - 运行时由 ld.so 把 libEGL stub 加载，stub 转发到 libmali.so
 *   - libmali 通过 ioctl(/dev/mali0) 提交 GLES 命令到 Mali-G52 GPU
 *   - eglSwapBuffers 导出 dma-buf fd，经 zwp_linux_dmabuf_v1 协议
 *     送给 Weston，Weston 通过 rockchip-drm 上屏
 *
 * EGL 抽象层概念参见 note/Subsystem/Graph-Stack/06-EGL.md。
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "xdg-shell-client.h"
#include "render.h"

struct app {
	/* wayland */
	struct wl_display     *dpy;
	struct wl_registry    *reg;
	struct wl_compositor  *compositor;
	struct xdg_wm_base    *wm_base;
	struct wl_surface     *surf;
	struct xdg_surface    *xsurf;
	struct xdg_toplevel   *top;

	/* egl */
	EGLDisplay  egl_dpy;
	EGLContext  egl_ctx;
	EGLConfig   egl_cfg;
	EGLSurface  egl_surf;
	struct wl_egl_window *egl_win;

	/* state */
	int  width, height;     /* 当前 EGL window 尺寸 */
	int  pending_w, pending_h; /* 待应用的 configure 尺寸 */
	int  configured;
	int  running;
	uint64_t frames;
};

static volatile sig_atomic_t g_stop;
static void on_signal(int s) { (void)s; g_stop = 1; }

/* ===== xdg_wm_base ping/pong ========================================== */
static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial)
{
	(void)d;
	xdg_wm_base_pong(b, serial);
}
static const struct xdg_wm_base_listener wm_listener = { .ping = wm_ping };

/* ===== registry ======================================================= */
static void reg_global(void *data, struct wl_registry *reg, uint32_t name,
                       const char *iface, uint32_t version)
{
	(void)version;
	struct app *a = data;
	if (!strcmp(iface, wl_compositor_interface.name))
		a->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		a->wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(a->wm_base, &wm_listener, a);
	}
	/* 注意：不再需要 wl_shm —— EGL 路径走 wl_egl_window + dma-buf */
}
static void reg_global_remove(void *d, struct wl_registry *r, uint32_t n) {
	(void)d; (void)r; (void)n;
}
static const struct wl_registry_listener reg_listener = {
	.global = reg_global, .global_remove = reg_global_remove,
};

/* ===== xdg_surface / xdg_toplevel ===================================== */
static void xsurf_configure(void *data, struct xdg_surface *xs, uint32_t serial)
{
	struct app *a = data;
	xdg_surface_ack_configure(xs, serial);
	a->configured = 1;
}
static const struct xdg_surface_listener xsurf_listener = {
	.configure = xsurf_configure,
};

static void top_configure(void *data, struct xdg_toplevel *t,
                          int32_t w, int32_t h, struct wl_array *states)
{
	(void)t; (void)states;
	struct app *a = data;
	if (w > 0 && h > 0) { a->pending_w = w; a->pending_h = h; }
}
static void top_close(void *data, struct xdg_toplevel *t)
{
	(void)t;
	struct app *a = data;
	a->running = 0;
}
static void top_configure_bounds(void *d, struct xdg_toplevel *t, int32_t w, int32_t h)
{ (void)d; (void)t; (void)w; (void)h; }
static void top_wm_capabilities(void *d, struct xdg_toplevel *t, struct wl_array *caps)
{ (void)d; (void)t; (void)caps; }
static const struct xdg_toplevel_listener top_listener = {
	.configure          = top_configure,
	.close              = top_close,
	.configure_bounds   = top_configure_bounds,
	.wm_capabilities    = top_wm_capabilities,
};

/* ===== EGL init ======================================================= */
static int egl_setup(struct app *a)
{
	/* 1) 用 platform-specific 入口，明确告诉 EGL 我们要 Wayland 平台
	 *    —— 比 eglGetDisplay(EGLNativeDisplayType) 更确定，
	 *    Mali blob 在 EGL_PLATFORM_WAYLAND_KHR 路径上行为完整。 */
	a->egl_dpy = eglGetDisplay((EGLNativeDisplayType)a->dpy);
	if (a->egl_dpy == EGL_NO_DISPLAY) {
		fprintf(stderr, "[egl] eglGetDisplay failed\n");
		return -1;
	}

	EGLint major = 0, minor = 0;
	if (!eglInitialize(a->egl_dpy, &major, &minor)) {
		fprintf(stderr, "[egl] eglInitialize failed: 0x%x\n", eglGetError());
		return -1;
	}

	/* —— 关键诊断输出：板上栈身份 —— */
	const char *egl_vendor = eglQueryString(a->egl_dpy, EGL_VENDOR);
	const char *egl_ver    = eglQueryString(a->egl_dpy, EGL_VERSION);
	const char *egl_apis   = eglQueryString(a->egl_dpy, EGL_CLIENT_APIS);
	const char *egl_exts   = eglQueryString(a->egl_dpy, EGL_EXTENSIONS);
	printf("[egl] EGL_VENDOR     = %s\n", egl_vendor ? egl_vendor : "(null)");
	printf("[egl] EGL_VERSION    = %s (init %d.%d)\n", egl_ver ? egl_ver : "(null)", major, minor);
	printf("[egl] EGL_CLIENT_APIS= %s\n", egl_apis ? egl_apis : "(null)");
	printf("[egl] EGL_EXTENSIONS = %s\n", egl_exts ? egl_exts : "(null)");

	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "[egl] eglBindAPI(GLES) failed: 0x%x\n", eglGetError());
		return -1;
	}

	/* 2) 选 config：GLES2 可渲染 + 至少 8888 RGBA + 双缓冲 window 表面 */
	EGLint cfg_attrs[] = {
		EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE,        8,
		EGL_GREEN_SIZE,      8,
		EGL_BLUE_SIZE,       8,
		EGL_ALPHA_SIZE,      8,
		EGL_NONE,
	};
	EGLint n_cfg = 0;
	if (!eglChooseConfig(a->egl_dpy, cfg_attrs, &a->egl_cfg, 1, &n_cfg) || n_cfg < 1) {
		fprintf(stderr, "[egl] eglChooseConfig failed: 0x%x\n", eglGetError());
		return -1;
	}

	/* 3) 创建 GLES2 context */
	EGLint ctx_attrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	a->egl_ctx = eglCreateContext(a->egl_dpy, a->egl_cfg, EGL_NO_CONTEXT, ctx_attrs);
	if (a->egl_ctx == EGL_NO_CONTEXT) {
		fprintf(stderr, "[egl] eglCreateContext failed: 0x%x\n", eglGetError());
		return -1;
	}
	return 0;
}

/* 在 xdg_surface 首次 configure 之后才能创建 wl_egl_window 与 surface */
static int egl_attach_surface(struct app *a)
{
	int w = a->width  > 0 ? a->width  : 480;
	int h = a->height > 0 ? a->height : 800;
	a->width = w; a->height = h;

	a->egl_win = wl_egl_window_create(a->surf, w, h);
	if (!a->egl_win) {
		fprintf(stderr, "[egl] wl_egl_window_create failed\n");
		return -1;
	}
	a->egl_surf = eglCreateWindowSurface(a->egl_dpy, a->egl_cfg,
	                                     (EGLNativeWindowType)a->egl_win, NULL);
	if (a->egl_surf == EGL_NO_SURFACE) {
		fprintf(stderr, "[egl] eglCreateWindowSurface failed: 0x%x\n", eglGetError());
		return -1;
	}
	if (!eglMakeCurrent(a->egl_dpy, a->egl_surf, a->egl_surf, a->egl_ctx)) {
		fprintf(stderr, "[egl] eglMakeCurrent failed: 0x%x\n", eglGetError());
		return -1;
	}
	eglSwapInterval(a->egl_dpy, 1);

	/* —— 关键诊断：GLES 侧厂商身份 —— */
	printf("[gl]  GL_VENDOR    = %s\n", glGetString(GL_VENDOR));
	printf("[gl]  GL_RENDERER  = %s\n", glGetString(GL_RENDERER));
	printf("[gl]  GL_VERSION   = %s\n", glGetString(GL_VERSION));
	printf("[gl]  GLSL_VERSION = %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
	fflush(stdout);
	return 0;
}

static void egl_apply_resize(struct app *a)
{
	if (a->pending_w > 0 && a->pending_h > 0 &&
	    (a->pending_w != a->width || a->pending_h != a->height)) {
		a->width = a->pending_w;
		a->height = a->pending_h;
		wl_egl_window_resize(a->egl_win, a->width, a->height, 0, 0);
		printf("[egl] resize → %dx%d\n", a->width, a->height);
		fflush(stdout);
	}
}

/* ===== main =========================================================== */
int main(void)
{
	signal(SIGINT,  on_signal);
	signal(SIGTERM, on_signal);
	setvbuf(stdout, NULL, _IOLBF, 0);

	struct app a = { .width = 0, .height = 0, .running = 1 };

	a.dpy = wl_display_connect(NULL);
	if (!a.dpy) {
		fprintf(stderr, "[wl] wl_display_connect failed (WAYLAND_DISPLAY=%s)\n",
		        getenv("WAYLAND_DISPLAY"));
		return 1;
	}
	printf("[wl]  connected display fd=%d\n", wl_display_get_fd(a.dpy));

	a.reg = wl_display_get_registry(a.dpy);
	wl_registry_add_listener(a.reg, &reg_listener, &a);
	wl_display_roundtrip(a.dpy);

	if (!a.compositor || !a.wm_base) {
		fprintf(stderr, "[wl] missing globals (compositor=%p wm_base=%p)\n",
		        (void*)a.compositor, (void*)a.wm_base);
		return 1;
	}

	if (egl_setup(&a) < 0) return 2;

	a.surf  = wl_compositor_create_surface(a.compositor);
	a.xsurf = xdg_wm_base_get_xdg_surface(a.wm_base, a.surf);
	xdg_surface_add_listener(a.xsurf, &xsurf_listener, &a);

	a.top = xdg_surface_get_toplevel(a.xsurf);
	xdg_toplevel_add_listener(a.top, &top_listener, &a);
	xdg_toplevel_set_app_id(a.top, "com.tspi.greet");
	xdg_toplevel_set_title(a.top, "TSPI Greet (EGL)");

	wl_surface_commit(a.surf);
	wl_display_roundtrip(a.dpy);  /* 拿首次 configure */

	/* configure 回调里 pending_w/h 已被填入 (kiosk-shell 全屏尺寸) */
	if (a.pending_w > 0 && a.pending_h > 0) {
		a.width  = a.pending_w;
		a.height = a.pending_h;
	}

	if (egl_attach_surface(&a) < 0) return 3;
	if (render_init() < 0) {
		fprintf(stderr, "[gl] render_init failed\n");
		return 4;
	}

	struct timespec t0;
	clock_gettime(CLOCK_MONOTONIC, &t0);

	while (a.running && !g_stop) {
		/* 派发 wayland 事件 (configure / ping / close) */
		while (wl_display_prepare_read(a.dpy) != 0)
			wl_display_dispatch_pending(a.dpy);
		wl_display_flush(a.dpy);
		wl_display_read_events(a.dpy);
		wl_display_dispatch_pending(a.dpy);

		egl_apply_resize(&a);

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		float t = (now.tv_sec - t0.tv_sec) + (now.tv_nsec - t0.tv_nsec) * 1e-9f;

		render_frame(a.width, a.height, t);

		if (!eglSwapBuffers(a.egl_dpy, a.egl_surf)) {
			fprintf(stderr, "[egl] eglSwapBuffers failed: 0x%x\n", eglGetError());
			break;
		}
		a.frames++;
	}

	printf("[main] exit. frames=%llu\n", (unsigned long long)a.frames);

	render_fini();
	eglMakeCurrent(a.egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroySurface(a.egl_dpy, a.egl_surf);
	wl_egl_window_destroy(a.egl_win);
	eglDestroyContext(a.egl_dpy, a.egl_ctx);
	eglTerminate(a.egl_dpy);

	xdg_toplevel_destroy(a.top);
	xdg_surface_destroy(a.xsurf);
	wl_surface_destroy(a.surf);
	wl_display_disconnect(a.dpy);
	return 0;
}
