//! 窗口表面状态监控 —— 演示 GTK4 的窗口与 GdkSurface 生命周期。
//!
//! 每 500ms 轮询一次 GdkSurface 属性，实时展示：
//!   - surface 是否 mapped
//!   - 窗口尺寸 (width × height)
//!   - display 名称
//!   - 当前 scale factor (HiDPI)
//!   - toplevel 类型 (Wayland / X11)
//!   - map / configure 事件计数

use gtk4::prelude::*;
use gtk4 as gtk;
use std::sync::atomic::{AtomicU32, Ordering};
use std::sync::Arc;

pub struct SurfaceMonitor {
    pub root: gtk::Box,
}

impl SurfaceMonitor {
    pub fn new(window: &gtk::ApplicationWindow) -> Self {
        let root = gtk::Box::new(gtk::Orientation::Vertical, 10);
        root.set_margin_top(24);
        root.set_margin_bottom(24);
        root.set_margin_start(48);
        root.set_margin_end(48);
        root.set_halign(gtk::Align::Fill);

        // title
        let title = gtk::Label::new(Some("窗口与表面管理"));
        title.add_css_class("surface-title");
        title.set_halign(gtk::Align::Start);
        root.append(&title);

        // info grid: 2 columns (property name | value)
        let grid = gtk::Grid::new();
        grid.set_column_spacing(16);
        grid.set_row_spacing(8);
        grid.set_halign(gtk::Align::Start);

        let fields: [(&str, &str); 7] = [
            ("Surface mapped:", "—"),
            ("Window size:", "—"),
            ("Display:", "—"),
            ("Scale factor:", "—"),
            ("Toplevel type:", "—"),
            ("Map count:", "—"),
            ("Configure count:", "—"),
        ];

        let mut val_labels: Vec<gtk::Label> = Vec::new();
        for (i, (prop, val)) in fields.iter().enumerate() {
            let prop_label = gtk::Label::new(Some(prop));
            prop_label.set_halign(gtk::Align::Start);
            prop_label.add_css_class("surface-prop");
            grid.attach(&prop_label, 0, i as i32, 1, 1);

            let val_label = gtk::Label::new(Some(val));
            val_label.set_halign(gtk::Align::Start);
            val_label.add_css_class("surface-val");
            val_label.set_selectable(true);
            grid.attach(&val_label, 1, i as i32, 1, 1);
            val_labels.push(val_label);
        }

        root.append(&grid);

        // event counters (Arc<AtomicU32> — Send+Sync required by GTK signal closures)
        let map_count = Arc::new(AtomicU32::new(0));
        let config_count = Arc::new(AtomicU32::new(0));

        // Track window map signal (GtkWidget level)
        {
            let mc = map_count.clone();
            window.connect_map(move |_| {
                mc.fetch_add(1, Ordering::Relaxed);
            });
        }

        // Track default-width / default-height property changes as proxy for configure
        {
            let cc = config_count.clone();
            window.connect_notify(Some("default-width"), move |_, _| {
                cc.fetch_add(1, Ordering::Relaxed);
            });
        }
        {
            let cc = config_count.clone();
            window.connect_notify(Some("default-height"), move |_, _| {
                cc.fetch_add(1, Ordering::Relaxed);
            });
        }

        // ── periodic poll (500 ms) ─────────────────
        let val_labels = val_labels.clone();
        let map_count = map_count.clone();
        let config_count = config_count.clone();
        let win_ref = window.clone(); // GtkWindow is ref-counted

        glib::timeout_add_local(std::time::Duration::from_millis(500), move || {
            // surface state
            let mapped = win_ref.is_mapped();
            let w = win_ref.width();
            let h = win_ref.height();

            use gdk4::prelude::DisplayExt;
            let display_name: String = gdk4::Display::default()
                .map(|d| d.name().to_string())
                .unwrap_or_else(|| "unknown".into());

            let scale = win_ref.scale_factor();

            // determine toplevel type
            let toplevel = if std::env::var("WAYLAND_DISPLAY").is_ok() {
                "Wayland (xdg_toplevel)"
            } else if std::env::var("DISPLAY").is_ok() {
                "X11"
            } else {
                "unknown"
            };

            // Update labels (fields order: mapped, size, display, scale, toplevel, maps, configs)
            let vals: [String; 7] = [
                format!("{}", if mapped { "Yes" } else { "No" }),
                format!("{} × {}", w, h),
                display_name,
                format!("{}×", scale),
                toplevel.to_string(),
                format!("{}", map_count.load(Ordering::Relaxed)),
                format!("{}", config_count.load(Ordering::Relaxed)),
            ];
            for (label, val) in val_labels.iter().zip(vals.iter()) {
                label.set_text(val);
            }

            glib::ControlFlow::Continue
        });

        // legend
        let legend = gtk::Label::new(Some(
            "说明：每 500ms 轮询窗口属性。关闭窗口或切换虚拟桌面时观察 mapped 状态变化。\
             resize 窗口观察 Configure 计数递增。"
        ));
        legend.add_css_class("surface-legend");
        legend.set_halign(gtk::Align::Start);
        legend.set_wrap(true);
        legend.set_max_width_chars(60);
        root.append(&legend);

        SurfaceMonitor { root }
    }
}
