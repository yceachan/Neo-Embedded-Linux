//! Button-Greet 组件 —— 演示 GTK4 的三项核心 UI 范式：
//! 1. **声明式 UI**：Widget 树通过代码组装，结构即文档
//! 2. **CSS 样式**：`gtk::CssProvider` 注入自定义样式，支持 `:hover` 伪类
//! 3. **事件系统**：`clicked` 信号 + `EventControllerMotion` (enter/leave)
//!
//! 布局:
//!   vbox
//!   ├── title_label  "Button Greet 演示"
//!   ├── entry        (输入名字，placeholder="输入你的名字...")
//!   ├── button       "🎉 Greet!"
//!   ├── greet_label  (默认 "👋 等待点击...")
//!   └── event_log    (最近事件列表，最多 5 条)

use gtk4::prelude::*;
use gtk4 as gtk;

pub struct GreetPanel {
    pub root: gtk::Box,
    #[allow(dead_code)]
    greet_label: gtk::Label,
    #[allow(dead_code)]
    event_log: gtk::Label,
}

impl GreetPanel {
    pub fn new() -> Self {
        // ── Widget tree (declarative layout via code) ─────
        let root = gtk::Box::new(gtk::Orientation::Vertical, 12);
        root.set_margin_top(24);
        root.set_margin_bottom(24);
        root.set_margin_start(48);
        root.set_margin_end(48);
        root.set_halign(gtk::Align::Center);
        root.set_valign(gtk::Align::Center);

        // title
        let title = gtk::Label::new(Some("Button Greet 演示"));
        title.add_css_class("greet-title");
        root.append(&title);

        // entry
        let entry = gtk::Entry::new();
        entry.set_placeholder_text(Some("输入你的名字..."));
        entry.set_max_width_chars(30);
        entry.add_css_class("greet-entry");
        root.append(&entry);

        // button
        let button = gtk::Button::with_label("Greet !");
        button.add_css_class("greet-button");
        root.append(&button);

        // greet result label
        let greet_label = gtk::Label::new(Some("等待点击..."));
        greet_label.add_css_class("greet-result");
        root.append(&greet_label);

        // separator
        let sep = gtk::Separator::new(gtk::Orientation::Horizontal);
        root.append(&sep);

        // event log header
        let log_header = gtk::Label::new(Some("事件日志:"));
        log_header.set_halign(gtk::Align::Start);
        log_header.add_css_class("greet-log-header");
        root.append(&log_header);

        // event log content
        let event_log = gtk::Label::new(Some("（等待交互...）"));
        event_log.set_halign(gtk::Align::Start);
        event_log.set_selectable(true);
        event_log.add_css_class("greet-log");
        root.append(&event_log);

        // ── CSS styling ──────────────────────────────────
        let css_provider = gtk::CssProvider::new();
        css_provider.load_from_data(include_str!("style.css"));

        gtk::style_context_add_provider_for_display(
            &gdk4::Display::default().expect("display"),
            &css_provider,
            gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
        );

        // ── Signal handlers (event system) ───────────────

        // click event — the core UI event
        {
            let greet_label = greet_label.clone();
            let event_log = event_log.clone();
            let entry = entry.clone();
            button.connect_clicked(move |_| {
                let name = entry.text();
                let name = if name.is_empty() { "世界" } else { &name };
                greet_label.set_text(&format!("Hello, {}!", name));
                let text = format!(
                    "[clicked] 问候: \"{}\"\n{}",
                    name,
                    event_log.text()
                );
                let lines: Vec<&str> = text.lines().take(5).collect();
                event_log.set_text(&lines.join("\n"));
            });
        }

        // enter / leave via EventControllerMotion (GTK4 way)
        {
            let event_log1 = event_log.clone();
            let motion_ctrl = gtk::EventControllerMotion::new();
            motion_ctrl.connect_enter(move |_, _x, _y| {
                let text = format!(
                    "[enter]  鼠标进入按钮区域 (CSS :hover 生效)\n{}",
                    event_log1.text()
                );
                let lines: Vec<&str> = text.lines().take(5).collect();
                event_log1.set_text(&lines.join("\n"));
            });
            let event_log2 = event_log.clone();
            motion_ctrl.connect_leave(move |_| {
                let text = format!(
                    "[leave]  鼠标离开按钮区域 (CSS :hover 失效)\n{}",
                    event_log2.text()
                );
                let lines: Vec<&str> = text.lines().take(5).collect();
                event_log2.set_text(&lines.join("\n"));
            });
            button.add_controller(motion_ctrl);
        }

        GreetPanel {
            root,
            greet_label,
            event_log,
        }
    }
}
