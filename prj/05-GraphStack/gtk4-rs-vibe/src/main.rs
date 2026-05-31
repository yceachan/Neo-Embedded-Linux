//! gtk4-rs GUI 范式演示
//!
//! 单窗口三 Tab：
//!   Tab 0 — 3D 渲染管线（GLArea + 旋转立方体）
//!   Tab 1 — 声明式 UI + CSS + 事件系统（Button-Greet）
//!   Tab 2 — 窗口与表面管理（GdkSurface 状态监控）
//!
//! 运行: `cargo run`
//! 依赖: libgtk-4-dev, libgl-dev

mod cube_renderer;
mod gl;
mod greet_panel;
mod math;
mod surface_monitor;

use gtk4::prelude::*;
use gtk4 as gtk;

//Rc<Refcell<T>>  这是 Rust 中单线程共享可变状态的标准模式。 对应Arc<Mutex>
//RefCell<T> 用检查从编译期推迟到运行时的容器——允许你通过不可变引用 &RefCell<T> 拿到内部 T 的可变引用。
use std::cell::RefCell;
use std::rc::Rc;

fn main() {
    let app = gtk::Application::new(Some("com.neo.demo.gtk4rs"), Default::default());

    app.connect_activate(|app| {
        build_ui(app);
    });
    app.build
    app.run();
}

fn build_ui(app: &gtk::Application) {
    // ── Window ─────────────────────────────────
    let window = gtk::ApplicationWindow::new(app);
    window.set_title(Some("gtk4-rs GUI 范式演示"));
    window.set_default_size(960, 680);

    // ── Notebook (tab container) ───────────────
    let notebook = gtk::Notebook::new();
    notebook.set_tab_pos(gtk::PositionType::Top);

    // ── Tab 0: 3D Cube ─────────────────────────
    {
        let gl_area = gtk::GLArea::new();
        gl_area.set_required_version(3, 3);
        gl_area.set_hexpand(true);
        gl_area.set_vexpand(true);

        let renderer = Rc::new(RefCell::new(cube_renderer::CubeRenderer::new()));

        // realize: init GL resources
        {
            let renderer = renderer.clone();
            gl_area.connect_realize(move |gl_area| {
                gl_area.make_current();
                let ok = unsafe { renderer.borrow_mut().realize() };
                if !ok {
                    eprintln!("[main] CubeRenderer::realize() failed!");
                } else {
                    eprintln!("[main] GL context ready, renderer initialized.");
                }
            });
        }

        // unrealize: free GL resources
        {
            let renderer = renderer.clone();
            gl_area.connect_unrealize(move |gl_area| {
                gl_area.make_current();
                unsafe { renderer.borrow_mut().free_gl(); }
                eprintln!("[main] GL resources freed.");
            });
        }

        // render callback
        {
            let renderer = renderer.clone();
            gl_area.connect_render(move |gl_area, _ctx| {
                let r = renderer.borrow();
                let w = gl_area.width();
                let h = gl_area.height();
                unsafe { r.draw(w, h); }
                glib::Propagation::Stop
            });
        }

        // tick callback: update rotation + queue redraw
        {
            let renderer = renderer.clone();
            gl_area.add_tick_callback(move |gl_area, _frame_clock| {
                renderer.borrow_mut().update();
                gl_area.queue_draw();
                glib::ControlFlow::Continue
            });
        }

        let tab_label = gtk::Label::new(Some("[3D] 渲染管线"));
        notebook.append_page(&gl_area, Some(&tab_label));
    }

    // ── Tab 1: Button Greet ────────────────────
    {
        let greet = greet_panel::GreetPanel::new();
        let tab_label = gtk::Label::new(Some("[UI] 声明式 UI"));
        notebook.append_page(&greet.root, Some(&tab_label));
    }

    // ── Tab 2: Surface Monitor ─────────────────
    {
        let monitor = surface_monitor::SurfaceMonitor::new(&window);
        let tab_label = gtk::Label::new(Some("[Win] 窗口表面"));
        notebook.append_page(&monitor.root, Some(&tab_label));
    }

    // ── Window assembly ────────────────────────
    window.set_child(Some(&notebook));

    // CSS for surface monitor
    let css = gtk::CssProvider::new();
    css.load_from_data(include_str!("surface.css"));

    gtk::style_context_add_provider_for_display(
        &gdk4::Display::default().expect("display"),
        &css,
        gtk::STYLE_PROVIDER_PRIORITY_APPLICATION,
    );

    window.present();
}
