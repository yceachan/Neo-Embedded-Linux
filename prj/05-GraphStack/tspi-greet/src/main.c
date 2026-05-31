/*
 * tspi-greet — Weston kiosk-shell 全屏欢迎页客户端
 *
 * - wayland-client + xdg-shell + wl_shm + cairo
 * - set_app_id("com.tspi.greet") 由 kiosk-shell 通过 weston.ini
 *   [output].app-ids 绑定，自动全屏占据 DSI 输出
 * - 每帧重绘居中 "Welcome to TSPI" + 实时 HH:MM:SS
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <cairo/cairo.h>
#include <wayland-client.h>

#include "xdg-shell-client.h"

struct app {
	struct wl_display     *dpy;
	struct wl_registry    *reg;
	struct wl_compositor  *compositor;
	struct wl_shm         *shm;
	struct xdg_wm_base    *wm_base;

	struct wl_surface     *surf;
	struct xdg_surface    *xsurf;
	struct xdg_toplevel   *top;

	int  width, height;
	int  configured;
	int  running;
	int  needs_redraw;
};

/* ---- xdg_wm_base ping/pong ------------------------------------------- */
static void wm_ping(void *d, struct xdg_wm_base *b, uint32_t serial)
{
	xdg_wm_base_pong(b, serial);
}
static const struct xdg_wm_base_listener wm_listener = { .ping = wm_ping };

/* ---- registry -------------------------------------------------------- */
/*============================================================**
*@READNME:
- 当客户端调用 wl_display_get_registry() 并添加此监听器后，
  合成器会为每一个它能提供的全局服务（如 wl_compositor、wl_shm、xdg_wm_base 等）
  触发一次 global 事件。
  global回调中，client的流程代码就需要把这些全局对象资源的指针binding到自己的数据结构里。
*=============================================================*/
static void reg_global(void *data, struct wl_registry *reg, uint32_t name,
                       const char *iface, uint32_t version)
{
	struct app *a = data;
	if (!strcmp(iface, wl_compositor_interface.name))
		a->compositor = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
	else if (!strcmp(iface, wl_shm_interface.name))
		a->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
	else if (!strcmp(iface, xdg_wm_base_interface.name)) {
		a->wm_base = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(a->wm_base, &wm_listener, a);
	}
}
static void reg_global_remove(void *d, struct wl_registry *r, uint32_t n) {}
static const struct wl_registry_listener reg_listener = {
	.global = reg_global, .global_remove = reg_global_remove,
};

/* ---- xdg_surface / xdg_toplevel -------------------------------------- */
static void xsurf_configure(void *data, struct xdg_surface *xs, uint32_t serial)
{
	struct app *a = data;
	xdg_surface_ack_configure(xs, serial);
	a->configured = 1;
	a->needs_redraw = 1;
}
static const struct xdg_surface_listener xsurf_listener = {
	.configure = xsurf_configure,
};

static void top_configure(void *data, struct xdg_toplevel *t,
                          int32_t w, int32_t h, struct wl_array *states)
{
	struct app *a = data;
	if (w > 0 && h > 0) { a->width = w; a->height = h; }
}
static void top_close(void *data, struct xdg_toplevel *t)
{
	struct app *a = data;
	a->running = 0;
}
static void top_configure_bounds(void *d, struct xdg_toplevel *t, int32_t w, int32_t h) {}
static void top_wm_capabilities(void *d, struct xdg_toplevel *t, struct wl_array *caps) {}
static const struct xdg_toplevel_listener top_listener = {
	.configure          = top_configure,
	.close              = top_close,
	.configure_bounds   = top_configure_bounds,
	.wm_capabilities    = top_wm_capabilities,
};

/* ---- SHM buffer ------------------------------------------------------ */
struct buffer {
	struct wl_buffer *wl;
	void   *data;
	size_t  size;
};

static void buf_release(void *data, struct wl_buffer *wl)
{
	struct buffer *b = data;
	wl_buffer_destroy(b->wl);
	munmap(b->data, b->size);
	free(b);
}
static const struct wl_buffer_listener buf_listener = { .release = buf_release };

static struct buffer *create_buffer(struct app *a, int w, int h)
{
	int stride = w * 4;
	size_t size = (size_t)stride * h;

	char tmpl[] = "/tmp/tspi-greet-XXXXXX";
	int fd = mkstemp(tmpl);
	if (fd < 0) { perror("mkstemp"); return NULL; }
	unlink(tmpl);
	if (ftruncate(fd, size) < 0) { perror("ftruncate"); close(fd); return NULL; }
	void *map = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED) { perror("mmap"); close(fd); return NULL; }

	struct wl_shm_pool *pool = wl_shm_create_pool(a->shm, fd, size);
	close(fd);
	struct wl_buffer *wl = wl_shm_pool_create_buffer(pool, 0, w, h, stride,
	                                                 WL_SHM_FORMAT_ARGB8888);
	wl_shm_pool_destroy(pool);

	struct buffer *b = calloc(1, sizeof(*b));
	b->wl = wl; b->data = map; b->size = size;
	wl_buffer_add_listener(wl, &buf_listener, b);
	return b;
}

/* ---- 绘制 ------------------------------------------------------------ */
static void draw(struct app *a, void *pixels, int w, int h)
{
	cairo_surface_t *cs = cairo_image_surface_create_for_data(
		pixels, CAIRO_FORMAT_ARGB32, w, h, w * 4);
	cairo_t *cr = cairo_create(cs);

	/* 背景：深蓝渐变 */
	cairo_pattern_t *bg = cairo_pattern_create_linear(0, 0, 0, h);
	cairo_pattern_add_color_stop_rgb(bg, 0.0, 0.04, 0.10, 0.22);
	cairo_pattern_add_color_stop_rgb(bg, 1.0, 0.01, 0.03, 0.08);
	cairo_set_source(cr, bg);
	cairo_paint(cr);
	cairo_pattern_destroy(bg);

	cairo_select_font_face(cr, "DejaVu Sans",
	                       CAIRO_FONT_SLANT_NORMAL,
	                       CAIRO_FONT_WEIGHT_BOLD);

	/* 主标语 */
	const char *title = "Welcome to TSPI";
	cairo_set_font_size(cr, 42);
	cairo_text_extents_t te;
	cairo_text_extents(cr, title, &te);
	cairo_set_source_rgb(cr, 0.95, 0.95, 1.0);
	cairo_move_to(cr,
	              (w - te.width) / 2.0 - te.x_bearing,
	              h * 0.40 - te.y_bearing - te.height / 2.0);
	cairo_show_text(cr, title);

	/* 副标 */
	const char *subtitle = "Rockchip RK3566  ·  Weston kiosk-shell";
	cairo_select_font_face(cr, "DejaVu Sans",
	                       CAIRO_FONT_SLANT_NORMAL,
	                       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 18);
	cairo_text_extents(cr, subtitle, &te);
	cairo_set_source_rgba(cr, 0.75, 0.85, 1.0, 0.85);
	cairo_move_to(cr,
	              (w - te.width) / 2.0 - te.x_bearing,
	              h * 0.48 - te.y_bearing);
	cairo_show_text(cr, subtitle);

	/* 时间 */
	char tbuf[32];
	time_t now = time(NULL);
	struct tm tm;
	localtime_r(&now, &tm);
	strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm);
	cairo_set_font_size(cr, 56);
	cairo_text_extents(cr, tbuf, &te);
	cairo_set_source_rgb(cr, 0.95, 0.95, 1.0);
	cairo_move_to(cr,
	              (w - te.width) / 2.0 - te.x_bearing,
	              h * 0.68 - te.y_bearing);
	cairo_show_text(cr, tbuf);

	/* 日期 */
	char dbuf[32];
	strftime(dbuf, sizeof(dbuf), "%Y-%m-%d  %a", &tm);
	cairo_set_font_size(cr, 18);
	cairo_text_extents(cr, dbuf, &te);
	cairo_set_source_rgba(cr, 0.75, 0.85, 1.0, 0.85);
	cairo_move_to(cr,
	              (w - te.width) / 2.0 - te.x_bearing,
	              h * 0.76 - te.y_bearing);
	cairo_show_text(cr, dbuf);

	cairo_destroy(cr);
	cairo_surface_destroy(cs);
}

/* ---- frame callback -------------------------------------------------- */
static const struct wl_callback_listener frame_listener;
static void redraw(struct app *a);

static void frame_done(void *data, struct wl_callback *cb, uint32_t time_ms)
{
	struct app *a = data;
	wl_callback_destroy(cb);
	redraw(a);
}
static const struct wl_callback_listener frame_listener = { .done = frame_done };

static void redraw(struct app *a)
{
	int w = a->width  > 0 ? a->width  : 480;
	int h = a->height > 0 ? a->height : 800;

	struct buffer *b = create_buffer(a, w, h);
	if (!b) { a->running = 0; return; }

	draw(a, b->data, w, h);

	wl_surface_attach(a->surf, b->wl, 0, 0);
	wl_surface_damage_buffer(a->surf, 0, 0, w, h);

	struct wl_callback *cb = wl_surface_frame(a->surf);
	wl_callback_add_listener(cb, &frame_listener, a);

	wl_surface_commit(a->surf);
}

/* ---- main ----------------------------------------------------------- */
int main(void)
{
	struct app a = { .width = 480, .height = 800, .running = 1 };

	a.dpy = wl_display_connect(NULL);
	if (!a.dpy) { fprintf(stderr, "wl_display_connect failed\n"); return 1; }

	a.reg = wl_display_get_registry(a.dpy);
	wl_registry_add_listener(a.reg, &reg_listener, &a);
	wl_display_roundtrip(a.dpy);

	if (!a.compositor || !a.shm || !a.wm_base) {
		fprintf(stderr, "missing required globals (compositor=%p shm=%p wm=%p)\n",
		        a.compositor, a.shm, a.wm_base);
		return 1;
	}

	a.surf  = wl_compositor_create_surface(a.compositor);
	a.xsurf = xdg_wm_base_get_xdg_surface(a.wm_base, a.surf);
	xdg_surface_add_listener(a.xsurf, &xsurf_listener, &a);

	a.top = xdg_surface_get_toplevel(a.xsurf);
	xdg_toplevel_add_listener(a.top, &top_listener, &a);
	xdg_toplevel_set_app_id(a.top, "com.tspi.greet");
	xdg_toplevel_set_title(a.top, "TSPI Greet");

	wl_surface_commit(a.surf);
	wl_display_roundtrip(a.dpy);  /* 等首次 configure */

	if (a.needs_redraw) { redraw(&a); a.needs_redraw = 0; }

	while (a.running && wl_display_dispatch(a.dpy) != -1)
		;

	xdg_toplevel_destroy(a.top);
	xdg_surface_destroy(a.xsurf);
	wl_surface_destroy(a.surf);
	wl_display_disconnect(a.dpy);
	return 0;
}
