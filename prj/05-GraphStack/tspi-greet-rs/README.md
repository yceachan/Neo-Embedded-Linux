---
title: tspi-greet-rs —— Weston kiosk-shell 全屏欢迎页（wayland-rs + cairo-rs 版）
tags: [demo, rust, wayland-rs, cairo, cross-compile, kiosk-shell, tspi, rk3566, sysroot, pkg-config]
desc: 用 wayland-rs + cairo-rs 重写 prj/05-GraphStack/tspi-greet 的 C 版本 demo，并完整演示 Rust 交叉编译四件套 (rustup target / linker / pkg-config / cc)
update: 2026-05-21
---


# tspi-greet-rs —— Weston kiosk-shell 全屏欢迎页 (Rust 版)

> [!note]
> **Ref:**
> 设计文档：[`Design-CrossCompile.md`](Design-CrossCompile.md) ; [`Design-Wayland-Rs.md`](Design-Wayland-Rs.md) ; [`Design-TimeSeq.md`](Design-TimeSeq.md) ; [`Conclude.md`](Conclude.md)
> 对照原版：[`prj/05-GraphStack/tspi-greet/`](../tspi-greet/)（C + cairo + wayland-scanner 版本）
> 知识库：[`note/sdk/tspi/05-rust-crosscompile.md`](../../../note/sdk/tspi/05-rust-crosscompile.md) ; [`note/sdk/tspi/06-rust-gui-on-embedded.md`](../../../note/sdk/tspi/06-rust-gui-on-embedded.md) ; [`note/Subsystem/Graph-Stack/05-tspi-buildroot-weston-de.md`](../../../note/Subsystem/Graph-Stack/05-tspi-buildroot-weston-de.md)


## 目标 / 现象

把 [C 版 tspi-greet](../tspi-greet/)  这个wayland client -cairo应用改写为 Rust 版本，引用`cairo-sys`;`wayland-rs` 

1. Rust 工程能用 **rustup target + buildroot linker + pkg-config 三件套**把 cairo / wayland 客户端**交叉编译**到 RK3566 aarch64；
2. 产物 ELF 是 **PIE aarch64**，dynamic NEEDED 只含 **`libcairo.so.2` + `libgcc_s.so.1` + `libc.so.6`**，wayland-rs 全部静态进 .text；
3. 板上 weston (kiosk-shell) 环境下，应用通过 `set_app_id("com.tspi.greet")` 被 [`etc/00-kiosk.ini`](etc/00-kiosk.ini:13) 接管全屏，进入 main loop 等帧事件。

整套 demo 的真正"主角"是 **`.cargo/config.toml`**：它压缩了 `SYSROOT`、`PKG_CONFIG_*`、`CC_<triple>` 等所有交叉编译知识点 —— 详见 [`Design-CrossCompile.md`](Design-CrossCompile.md)。


## 项目结构

```text
tspi-greet-rs/
├── README.md                # 本文件
├── Design-CrossCompile.md   # ★ Rust 交叉编译四件套详解（SYSROOT/PKG_CONFIG）
├── Design-Wayland-Rs.md     # wayland-rs 应用层架构 + 关键 API 走读
├── Design-TimeSeq.md        # 启动握手 / 主循环 / 退出三张时序图
├── Conclude.md              # 真实捕获的 ./test.sh 输出 + 逐现象分析
├── Cargo.toml               # 依赖：wayland-client / wayland-protocols / cairo-rs / memmap2 / tempfile / libc
├── .cargo/
│   └── config.toml          # ★ build.target / linker / [env] PKG_CONFIG_*  四件套全配置
├── src/
│   ├── main.rs              # 连接 / 绑定 globals / dispatch / redraw 主体
│   └── draw.rs              # cairo 渲染（渐变背景 + 标语 + 时钟）
├── etc/
│   └── 00-kiosk.ini         # weston kiosk-shell autolaunch 配置（与 C 版同）
├── output/                  # 所有产物 / 日志统一收口（demo skill 规约）
│   ├── target/              # cargo target-dir 重定向到这里
│   ├── bin/tspi-greet-rs    # final binary（strip 后 ~456KB）
│   └── log/{build,verify,run}.log
├── build.sh                 # 等价于 `cargo build --release`，加 file/readelf 输出
├── deploy.sh                # 部署到 NFS_LOCAL/tspi-greet-rs/
├── test.sh                  # ★ 一键: clean→build→静态校验→deploy→板上跑→tee log
└── .gitignore
```


## 部署 / 前置条件

1. 已按 [`note/sdk/tspi/05-rust-crosscompile.md` §4](../../../note/sdk/tspi/05-rust-crosscompile.md) 装好：
   ```bash
   rustup target add aarch64-unknown-linux-gnu
   ```
2. 已编出 TSPI buildroot sysroot（路径在 `.cargo/config.toml` 里写死）。
3. NFS 与板：本地 `/home/pi/imx/prj/nfs_tspi/` 挂在板上 `/mnt/nfs`（与 `imx6ull` 路线不同 —— 见 [memory: TSPI infra paths](../../../../home/pi/.claude/projects/-home-pi-imx/memory/reference_tspi_infra.md)）。
4. `ssh tspi` 通（需 `-o ConnectTimeout=30`，详见 [memory: SSH tspi timeout](../../../../home/pi/.claude/projects/-home-pi-imx/memory/feedback_ssh_tspi_timeout.md)）；test.sh 已经按 30s 配置，若 ssh 不通会自动跳过远程跑步骤。


## 一键测试

```bash
cd prj/05-GraphStack/tspi-greet-rs
./test.sh
```

预期：

- 退出码 **0**
- `output/log/build.log`：`Finished release profile … in ~25s` + `ELF 64-bit LSB pie executable, ARM aarch64`
- `output/log/verify.log`：所有 `-I` 路径以 `<sysroot>` 开头，NEEDED 三条
- `output/log/run.log`：板上 `entering main loop`（被 `timeout 3` 中止）

日志解读详见 [`Conclude.md`](Conclude.md)。


## 在板上手工跑（绕过 test.sh）

```bash
ssh -o ConnectTimeout=30 tspi
# 板上：
export XDG_RUNTIME_DIR=/run
export WAYLAND_DISPLAY=$(basename $(ls /run/wayland-* | head -1))
/mnt/nfs/tspi-greet-rs/tspi-greet-rs
# Ctrl-C 退出
```

或安装为 weston 自动拉起的应用：

```bash
cp etc/00-kiosk.ini  /etc/xdg/weston/weston.ini
cp output/bin/tspi-greet-rs /usr/bin/
systemctl restart weston-launch   # （服务名依板上 init 链而异）
```


## 清理

```bash
cd prj/05-GraphStack/tspi-greet-rs
cargo clean --target-dir output/target
rm -rf output/bin/* output/log/*
```


## 与 C 版的差异速览

| 维度 | C (`tspi-greet/`) | Rust (`tspi-greet-rs/`) |
|------|-------------------|-------------------------|
| 协议绑定 | `wayland-scanner` 生成 `xdg-shell-client.h` / `.c` | `wayland-protocols` crate 内置，无需 scanner |
| 事件分发 | C struct of fn pointers (`*_listener`) | `impl Dispatch<I, U>` trait + Enum match |
| 资源回收 | 每个回调里手写 `wl_*_destroy + munmap + close` | `BufferState` UserData RAII 自动 |
| 构建系统 | Makefile 显式 `CC` / `CFLAGS` / `LDFLAGS` | `cargo build` + `.cargo/config.toml` |
| `--sysroot` | 显式写在 CFLAGS | **隐式** —— linker 是 buildroot gcc，spec 内置 |
| 体积 | ~29 KB | ~456 KB |
| 板上 .so 依赖 | libcairo + libwayland-client + libc | libcairo + libgcc_s + libc（无 libwayland） |

完整对照表见 [`Design-Wayland-Rs.md` §4](Design-Wayland-Rs.md) 与 [`Design-CrossCompile.md` §7](Design-CrossCompile.md)。
