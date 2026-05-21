// =============================================================================
//  cairo 渲染 —— 把一块 ARGB32 像素缓冲填上"渐变背景 + 标语 + 时钟"
//
//  与 C 版的差异：
//  - C 版调 cairo_image_surface_create_for_data(...) 直接拿到 cairo_surface_t*；
//    Rust 版调 ImageSurface::create_for_data_unsafe 直接借走 mmap 切片，
//    生命周期由 RAII 收尾 —— 不需要手写 cairo_surface_destroy。
//  - 时钟字符串：C 用 strftime；这里用 libc::localtime_r + format! 等价实现，
//    避免引入 chrono 这种重量级 crate（保持二进制体积）。
//
//  关键 unsafe: ImageSurface::create_for_data_unsafe
//    - cairo 不接管底层缓冲所有权，drop 时不会 free
//    - 调用方必须保证 pixels.len() == stride * h
// =============================================================================

use cairo::{Context, FontSlant, FontWeight, Format, ImageSurface, LinearGradient};
use std::time::{SystemTime, UNIX_EPOCH};

/// 把一帧画到 ARGB32 的 mmap 缓冲上。
///
/// `pixels`: 长度必须 == `stride * h`（stride = w * 4）。
pub fn render_inplace(pixels: &mut [u8], w: i32, h: i32) {
    let stride = w * 4;

    // SAFETY:
    //   - 调用方已校验 pixels.len() == stride * h
    //   - ImageSurface 不会 free 此缓冲；drop 时只是解除引用
    //   - ARGB32 在小端机器 (RK3566/Cortex-A55) 上是 BGRA 内存字节序，cairo 已假定
    let surface = unsafe {
        ImageSurface::create_for_data_unsafe(pixels.as_mut_ptr(), Format::ARgb32, w, h, stride)
    }
    .expect("cairo surface create_for_data_unsafe failed");

    let cr = Context::new(&surface).expect("cairo Context::new failed");

    // ---- 背景：深蓝竖直渐变 ----
    let bg = LinearGradient::new(0.0, 0.0, 0.0, h as f64);
    bg.add_color_stop_rgb(0.0, 0.04, 0.10, 0.22);
    bg.add_color_stop_rgb(1.0, 0.01, 0.03, 0.08);
    cr.set_source(&bg).unwrap();
    cr.paint().unwrap();

    // ---- 主标语 ----
    cr.select_font_face("DejaVu Sans", FontSlant::Normal, FontWeight::Bold);
    let title = "Welcome to TSPI";
    cr.set_font_size(42.0);
    let te = cr.text_extents(title).unwrap();
    cr.set_source_rgb(0.95, 0.95, 1.0);
    cr.move_to(
        (w as f64 - te.width()) / 2.0 - te.x_bearing(),
        h as f64 * 0.40 - te.y_bearing() - te.height() / 2.0,
    );
    cr.show_text(title).unwrap();

    // ---- 副标 ----
    cr.select_font_face("DejaVu Sans", FontSlant::Normal, FontWeight::Normal);
    cr.set_font_size(18.0);
    let subtitle = "Rockchip RK3566  ·  Weston kiosk-shell  ·  wayland-rs";
    let te = cr.text_extents(subtitle).unwrap();
    cr.set_source_rgba(0.75, 0.85, 1.0, 0.85);
    cr.move_to(
        (w as f64 - te.width()) / 2.0 - te.x_bearing(),
        h as f64 * 0.48 - te.y_bearing(),
    );
    cr.show_text(subtitle).unwrap();

    // ---- 时间 HH:MM:SS ----
    let (hms, ymd) = format_time();
    cr.set_font_size(56.0);
    let te = cr.text_extents(&hms).unwrap();
    cr.set_source_rgb(0.95, 0.95, 1.0);
    cr.move_to(
        (w as f64 - te.width()) / 2.0 - te.x_bearing(),
        h as f64 * 0.68 - te.y_bearing(),
    );
    cr.show_text(&hms).unwrap();

    // ---- 日期 Y-m-d ----
    cr.set_font_size(18.0);
    let te = cr.text_extents(&ymd).unwrap();
    cr.set_source_rgba(0.75, 0.85, 1.0, 0.85);
    cr.move_to(
        (w as f64 - te.width()) / 2.0 - te.x_bearing(),
        h as f64 * 0.76 - te.y_bearing(),
    );
    cr.show_text(&ymd).unwrap();

    // 刷盘：cairo 内部对 ImageSurface 有缓存，必须 flush 后位图才稳定。
    surface.flush();
}

/// 返回 ("HH:MM:SS", "YYYY-mm-dd Wkd")。借 libc::localtime_r 避免引入 chrono。
fn format_time() -> (String, String) {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs() as libc::time_t)
        .unwrap_or(0);

    let mut tm: libc::tm = unsafe { std::mem::zeroed() };
    unsafe {
        libc::localtime_r(&now, &mut tm);
    }

    let hms = format!("{:02}:{:02}:{:02}", tm.tm_hour, tm.tm_min, tm.tm_sec);
    const WDAY: [&str; 7] = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
    let ymd = format!(
        "{:04}-{:02}-{:02}  {}",
        tm.tm_year + 1900,
        tm.tm_mon + 1,
        tm.tm_mday,
        WDAY[(tm.tm_wday as usize) % 7],
    );
    (hms, ymd)
}
