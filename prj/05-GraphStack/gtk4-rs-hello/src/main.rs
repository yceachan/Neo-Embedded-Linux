use gtk4::{self as gtk, Button, Label, Box};
use gtk::prelude::*;
use gtk::{glib, Application, ApplicationWindow};
use gtk::Align;
use std::cell::Cell;
use std::rc::Rc;

const APP_ID: &str = "com.hello.gtk";

fn main() -> glib::ExitCode {
    let app = Application::builder()
        .application_id(APP_ID)
        .build();

    // ① 共享状态：Rc<Cell<u32>> — Cell 只适用于实现了 Copy trait 的对象。对于其他对象，则应使用RefCell.
    let count: Rc<Cell<u32>> = Rc::new(Cell::new(0));
    

    //|xxx| 是闭包的参数列表，是闭包被调用时由外界传进来的值，不是捕获的环境变量。
    app.connect_activate(move |app| {
        build_ui(app, count.clone());
    });

    app.run()
}

fn build_ui(app: &Application, count: Rc<Cell<u32>>) {
    let window = ApplicationWindow::builder()
        .application(app)
        .default_width(320)
        .default_height(200)
        .title("Hello, World!")
        .build();

    // Label 显示当前 count
    let label = Label::builder()
        .label("Count: 0")
        .margin_top(12)
        .margin_start(12)
        .margin_end(12)
        .halign(Align::Center)
        .build();

    let button = Button::builder()
        .label("Press me!")
        .margin_top(12)
        .margin_bottom(12)
        .margin_start(12)
        .margin_end(12)
        .build();

    // ③ button 回调也需要读 label（更新文本），所以 clone 两个 Rc
    let count_btn = count.clone();
    // Macro for passing variables as strong or weak references into a closure.
    /*============================================================**
    *@READNME:
    - GTK系统的回调函数签名
    pub fn connect_clicked<F>(&self, f: F) -> SignalHandlerId
     where F: Fn(&Self) + 'static : 
    需要静态生命周期的不可变闭包Fn
    *=============================================================*/
    button.connect_clicked(glib::clone!(@weak label => move |_btn| {
        let new_val = count_btn.get() + 1;
        count_btn.set(new_val);
        label.set_label(&format!("Count: {}", new_val));
    }));

    // 纵向布局
    let vbox = Box::builder()
        .orientation(gtk::Orientation::Vertical)
        .build();
    vbox.append(&label);
    vbox.append(&button);
    window.set_child(Some(&vbox));

    window.present();
}