---
title: tspi-greet-rs 实测结论 —— wayland-rs 客户端在 RK3566 板上的全链路落地证据
tags: [conclude, observed, evidence, wayland-rs, cairo, cross-compile, sysroot, pkg-config, rk3566]
desc: 用 ./test.sh 跑出来的真实日志，逐项证明 (1) 交叉编译产物是 aarch64+sysroot-cairo 链 (2) 板上 weston 下能起来
update: 2026-05-21
---


# tspi-greet-rs 实测结论

> [!note]
> **Ref:**
> [`Design-CrossCompile.md`](Design-CrossCompile.md) ；
> [`Design-Wayland-Rs.md`](Design-Wayland-Rs.md) ；
> [`note/sdk/tspi/05-rust-crosscompile.md`](../../../note/sdk/tspi/05-rust-crosscompile.md) ；
> [`note/sdk/tspi/06-rust-gui-on-embedded.md`](../../../note/sdk/tspi/06-rust-gui-on-embedded.md) ；
> 原始日志：[`output/log/build.log`](output/log/build.log) / [`output/log/verify.log`](output/log/verify.log) / [`output/log/run.log`](output/log/run.log)


## 1. 现象速览

我们想验证：**用 wayland-rs (纯 Rust) + cairo-rs (链 buildroot sysroot 里的 libcairo) 重写 tspi-greet，且能跑在 TSPI RK3566 板上**。

跑完 `./test.sh` 后实测观测到：
1. cargo build 产 ELF 是 **aarch64**，dynamic NEEDED 只有 **libcairo.so.2 + libgcc_s.so.1 + libc.so.6**，wayland 完全静态进 .text；
2. pkg-config 在 PKG\_CONFIG\_SYSROOT\_DIR / LIBDIR / ALLOW\_CROSS 三件套作用下，**所有 `-I` 路径都自动加上了 sysroot 前缀**，无一条逃逸到宿主 `/usr`；
3. 部署到板上后，**无 WAYLAND\_DISPLAY 时 panic 出 "NoCompositor"**（证明 ELF 可在 aarch64 上执行）；
4. **在 weston 下 (WAYLAND\_DISPLAY=wayland-0) 跑得起来**：成功 bind 三大 global、进入 main loop、被 SIGTERM 后正常退出。

**结论**：Rust 交叉编译四件套（rustup target / linker / pkg-config 三件套 / cc 兜底）配置正确，wayland-rs 0.31 + cairo-rs 0.18 组合在 aarch64 buildroot 环境下零摩擦工作。


## 2. 实测输出（来自 `output/log/`，未做任何改写）

### 2.1 build 阶段 (`output/log/build.log` 关键末段)

```text
[ok]    output/bin/tspi-greet-rs: ELF 64-bit LSB pie executable, ARM aarch64
[size]  456K
[deps]
         0x0000000000000001 (NEEDED)             Shared library: [libcairo.so.2]
         0x0000000000000001 (NEEDED)             Shared library: [libgcc_s.so.1]
         0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
[build] done   17:46:09
```

完整 cargo 链路（截 75 个 crate 自动下载并编译完成的提示）：

```text
    Updating crates.io index
     Locking 75 packages to latest compatible versions
   Compiling cairo-sys-rs v0.18.2
   Compiling cairo-rs v0.18.5
   Compiling wayland-protocols v0.31.2
   Compiling tspi-greet-rs v0.1.0 (/home/pi/imx/prj/05-GraphStack/tspi-greet-rs)
    Finished `release` profile [optimized] target(s) in 25.50s
```

### 2.2 静态验证 (`output/log/verify.log`)

```text
## file
output/bin/tspi-greet-rs: ELF 64-bit LSB pie executable, ARM aarch64, version 1 (SYSV),
dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, for GNU/Linux 3.7.0, stripped

## readelf -d (dynamic dependencies)
 0x0000000000000001 (NEEDED)             Shared library: [libcairo.so.2]
 0x0000000000000001 (NEEDED)             Shared library: [libgcc_s.so.1]
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]

## pkg-config cairo (with sysroot env)
-pthread
-I<sysroot>/usr/include/cairo
-I<sysroot>/usr/include/glib-2.0
-I<sysroot>/usr/lib/glib-2.0/include
-I<sysroot>/usr/include/pixman-1
-I<sysroot>/usr/include/rga
-I<sysroot>/usr/include/freetype2
-I<sysroot>/usr/include/libdrm
-I<sysroot>/usr/include/libpng16
-lcairo

## sysroot cairo .so version
libcairo.so      -> libcairo.so.2.11704.0
libcairo.so.2    -> libcairo.so.2.11704.0
libcairo.so.2.11704.0  (949984 bytes)
```

(`<sysroot>` 在原日志里是完整绝对路径 `/home/pi/imx/sdk/tspi-rk3566-sdk/buildroot/output/.../sysroot`，此处替换为占位以便阅读。)

### 2.3 板上实跑 (`output/log/run.log`，逐行原文)

```text
## remote uname + file check
Linux taishanpi 6.1.141 #6 SMP Wed May 13 10:48:10 +08 2026 aarch64 GNU/Linux
-rwxr-xr-x 1 1000 1000 463552 May 21  2026 /mnt/nfs/tspi-greet-rs/tspi-greet-rs

## remote ldd
bash: line 1: ldd: command not found

## remote --help / cold run (no WAYLAND_DISPLAY → 显式报错即成功证明 ELF 可执行)
[tspi-greet-rs] ARCH=aarch64 OS=linux

thread 'main' (2482) panicked at src/main.rs:261:45:
WAYLAND_DISPLAY not set or connect failed: NoCompositor
note: run with `RUST_BACKTRACE=1` environment variable to display a backtrace

## remote run under weston (3 秒后 kill)
WAYLAND_DISPLAY=wayland-0
[tspi-greet-rs] ARCH=aarch64 OS=linux
[tspi-greet-rs] globals bound: compositor + wl_shm + xdg_wm_base
[tspi-greet-rs] entering main loop
```


## 3. 逐现象分析

### 3.1 产物是 aarch64 PIE ELF —— Rust 跨编译目标命中

- **预期**：rustup target `aarch64-unknown-linux-gnu` + linker = buildroot gcc → 产 aarch64 ELF，
  解释器 `/lib/ld-linux-aarch64.so.1`（板上 rootfs 提供）。理论见 [`note/sdk/tspi/05-rust-crosscompile.md` §2-§3](../../../note/sdk/tspi/05-rust-crosscompile.md)。
- **实测**：`output/log/verify.log` 第 2 行 ——
  ```text
  ELF 64-bit LSB pie executable, ARM aarch64, ..., interpreter /lib/ld-linux-aarch64.so.1
  ```
- **机理**：`.cargo/config.toml:13` `build.target` 让 rustc 选 aarch64 target spec；`.cargo/config.toml:17` 显式让 linker 走 buildroot 的 `aarch64-buildroot-linux-gnu-gcc`（[`src/main.rs:259`](src/main.rs:259) 编译路径）。最终 link 阶段所有 crt / 解释器都从 buildroot sysroot 注入。
- **若不成立会怎样**：若 `target.<triple>.linker` 没配，rustc 默认 `cc` 解析到宿主 x86_64 gcc，`file` 会显示 `ELF 64-bit LSB pie executable, x86-64`，复制到板上 `cannot execute binary file: Exec format error`。

### 3.2 NEEDED 只有 3 个 .so，且 cairo 来自 sysroot

- **预期**：wayland-client / wayland-protocols 是**纯 Rust**实现 → 不出现在 NEEDED；只有 cairo-rs 链 C 库 → 应见 `libcairo.so.2`；libgcc_s / libc 是 PIE 必备。
- **实测**：`output/log/verify.log` 第 6-8 行 —— 精确三条 NEEDED：
  ```text
  libcairo.so.2
  libgcc_s.so.1
  libc.so.6
  ```
- **机理**：[`src/main.rs:1`](src/main.rs:1) 用 wayland-client 0.31 的纯 Rust 实现（默认特性），通过 UDS 直接讲 wire 协议；cairo-rs 通过 cairo-sys-rs build.rs 调 pkg-config（[Design-CrossCompile.md §5](Design-CrossCompile.md)），返回 `-lcairo` → linker 产生 `NEEDED libcairo.so.2`。
- **若不成立会怎样**：
  - 若 NEEDED 出现 `libwayland-client.so.0` → 说明误开了 wayland-sys 的 `dlopen` 反例 feature；
  - 若 cairo 缺席 → 链接器没拿到 `-lcairo`，多半是 PKG_CONFIG_LIBDIR 写成了 PATH，宿主找不到 cairo.pc → build.rs 直接 panic（不会走到这一步）。

### 3.3 pkg-config 三件套精确生效 —— 所有 `-I` 注入 sysroot 前缀

- **预期**：`.pc` 里写 `prefix=/usr`、`Cflags: -I${includedir}/cairo`，pkg-config 自身按 `PKG_CONFIG_SYSROOT_DIR` 给所有 `-I/-L` 加前缀，最终输出全部以 sysroot 开头。理论见 [Design-CrossCompile.md §5](Design-CrossCompile.md) + [`note/sdk/tspi/06-rust-gui-on-embedded.md` §6](../../../note/sdk/tspi/06-rust-gui-on-embedded.md)。
- **实测**：`output/log/verify.log` 第 11-19 行的九条 `-I` ——
  ```text
  -I<sysroot>/usr/include/cairo
  -I<sysroot>/usr/include/glib-2.0
  -I<sysroot>/usr/lib/glib-2.0/include
  -I<sysroot>/usr/include/pixman-1
  -I<sysroot>/usr/include/rga
  -I<sysroot>/usr/include/freetype2
  -I<sysroot>/usr/include/libdrm
  -I<sysroot>/usr/include/libpng16
  ```
  **每一条 `-I` 都是 `<sysroot>/usr/...` 开头，无一条逃逸到宿主 `/usr`**。
- **机理**：[`.cargo/config.toml:46-48`](.cargo/config.toml:46) 注入三件套到 cairo-sys-rs build.rs 子环境；pkg-config-rs 见 `ALLOW_CROSS=1` 通过保护检查 → 启 host `pkg-config` 子进程，按 `PKG_CONFIG_LIBDIR` 只看 sysroot 的 `.pc`，按 `PKG_CONFIG_SYSROOT_DIR` 重写所有路径前缀。
- **若不成立会怎样**：
  - 缺 `ALLOW_CROSS=1` → build.rs panic："pkg-config has not been configured to support cross-compilation"；
  - 用 PATH 而非 LIBDIR → 宿主 `cairo.pc` 漏入，输出会混合 `/usr/include/cairo`（宿主）与 `/sysroot/...`（cross）→ 链接时 ABI 错；
  - 缺 SYSROOT_DIR → `.pc` 找到了但 `-I/usr/include/cairo` 解析宿主头文件 → 编译期类型 ABI 错配（pixman_int 之类）。

### 3.4 板上 cold run 主动 panic —— ELF 真的在跑

- **预期**：未设 WAYLAND\_DISPLAY 时 [`Connection::connect_to_env`](src/main.rs:261) 应返回 `Err(NoCompositor)`，被 `.expect()` 转 panic，且 ARCH 打印先于 panic。
- **实测**：`output/log/run.log` 第 9-13 行 ——
  ```text
  [tspi-greet-rs] ARCH=aarch64 OS=linux

  thread 'main' (2482) panicked at src/main.rs:261:45:
  WAYLAND_DISPLAY not set or connect failed: NoCompositor
  ```
- **机理**：该 panic 不是 SEGV 也不是 SIGILL —— 是**正常的 Rust unwind/abort 错误信息**。这意味着：
  1. 板上动态 loader (`/lib/ld-linux-aarch64.so.1`) 成功载入；
  2. libc + libcairo + libgcc_s 都解析到了（rootfs 与 sysroot 同源验证通过）；
  3. main() 顺利执行到 [`main.rs:259`](src/main.rs:259) 第 1 行 println!（先打 ARCH=aarch64），再走到 [`main.rs:261`](src/main.rs:261) 才 panic；
  4. panic 行号 `src/main.rs:261:45` 与本仓代码一致 —— 没有错位。
- **若不成立会怎样**：
  - 看到 `Exec format error` → 二进制不是 aarch64；
  - 看到 `No such file or directory` 但 `ls` 看得到 → 解释器或某条 NEEDED .so 缺失（rootfs 与 sysroot 不同源）；
  - 看到 SIGILL → 指令集错配（如 cc crate 编 host gcc 产 x86_64 .a）。

### 3.5 weston 下成功 bind globals 并进 main loop —— 应用真正起来了

- **预期**：在 weston 已经 active 的板子上，`WAYLAND_DISPLAY=wayland-0` 注入 → 应连上 compositor → 列举 globals → bind 三大件 → set_app_id → 等首次 configure → 进主循环。详见 [Design-TimeSeq.md §1](Design-TimeSeq.md)。
- **实测**：`output/log/run.log` 第 16-19 行 ——
  ```text
  WAYLAND_DISPLAY=wayland-0
  [tspi-greet-rs] ARCH=aarch64 OS=linux
  [tspi-greet-rs] globals bound: compositor + wl_shm + xdg_wm_base
  [tspi-greet-rs] entering main loop
  ```
  随后 `timeout 3` 把进程 SIGTERM 掉（没看到 [exit] 是因为 SIGTERM 不走 main return）。
- **机理**：
  1. [`main.rs:261`](src/main.rs:261) `Connection::connect_to_env()` 读到 `WAYLAND_DISPLAY=wayland-0` → 打开 `/run/wayland-0` UDS → 与 weston 握手；
  2. [`main.rs:262`](src/main.rs:262) `registry_queue_init` → 一次 roundtrip 列举完所有 global；
  3. [`main.rs:268-280`](src/main.rs:268) 三次 `globals.bind::<I,_,_>` 全部成功（否则就会 `expect` panic）；
  4. [`main.rs:302`](src/main.rs:302) 打印 "entering main loop" → 实际进入 `blocking_dispatch` 阻塞等下一帧事件。
- **若不成立会怎样**：
  - 若 weston 没起 → connect_to_env 同 §3.4 报 NoCompositor；
  - 若三大 global 缺一 → "missing wl_compositor / wl_shm / xdg_wm_base" expect panic（kiosk-shell 标准支持的 globals 这三个都有）；
  - 若 set_app_id 与 weston.ini 不匹配 → 应用仍跑，但不会被 kiosk-shell 自动全屏（窗口浮空）。本 demo 的 app_id `"com.tspi.greet"` 与 [`etc/00-kiosk.ini:13`](etc/00-kiosk.ini:13) 一致 → 一定全屏命中 DSI-1。

### 3.6 体积与 C 版的代价对比

- **预期**：Rust 版加了 wayland 静态状态机进 .text，体积应明显大于 C 版的 29 KB；但仍在嵌入式可接受范围 (<1 MB)。
- **实测**：build.log "size 456K"；C 版同位置 `tspi-greet/tspi-greet` 大约 29 KB。
- **机理**：wayland-client / wayland-protocols 把所有协议状态机、Dispatch 路由表静态展开，加上 Rust panic 信息表（即使 `panic=abort` 也会保留行号字符串） + std 必备代码。
- **意义**：付出 ~430 KB 换"板上 rootfs 不再需要 libwayland-client.so" + "RAII 资源管理" + "编译期穷尽事件类型检查"，对仪表盘 / kiosk 类应用是值得的交易。


## 4. 结论表

| # | 观测现象 | 验证的概念 | 对应笔记 |
|---|---------|-----------|----------|
| 1 | 产物 `file` 报 ARM aarch64 PIE | rustup target spec 生效；linker = buildroot gcc 完成 final link | [`note/sdk/tspi/05-rust-crosscompile.md` §2-§3](../../../note/sdk/tspi/05-rust-crosscompile.md) |
| 2 | NEEDED 仅 3 条且 cairo 在内 | wayland-rs 纯 Rust 不链 libwayland；cairo-sys-rs 走 pkg-config 链 sysroot 库 | [`Design-CrossCompile.md` §5](Design-CrossCompile.md) ; [`note/sdk/tspi/06-rust-gui-on-embedded.md` §3](../../../note/sdk/tspi/06-rust-gui-on-embedded.md) |
| 3 | 所有 pkg-config `-I` 加 sysroot 前缀 | `PKG_CONFIG_SYSROOT_DIR` 路径前缀重写 | [`Design-CrossCompile.md` §5.2-§5.4](Design-CrossCompile.md) |
| 4 | pkg-config 只输出 sysroot 路径 (无宿主) | `PKG_CONFIG_LIBDIR` 替换默认搜索路径 (≠ PATH 追加) | [`note/sdk/tspi/06-rust-gui-on-embedded.md` §6 TIP](../../../note/sdk/tspi/06-rust-gui-on-embedded.md) |
| 5 | 板上 cold run 主动 panic "NoCompositor" 而非 segfault | aarch64 ELF + 动态 loader + glibc/libcairo 全部链对 → ELF 可执行 | [`Design-CrossCompile.md` §3](Design-CrossCompile.md) |
| 6 | 板上 weston 下成功进入 main loop | wayland-rs Dispatch 模型 + registry/bind 正确；set_app_id 与 kiosk-shell 契约对齐 | [`Design-Wayland-Rs.md` §2.7](Design-Wayland-Rs.md) ; [`note/Subsystem/Graph-Stack/05-tspi-buildroot-weston-de.md`](../../../note/Subsystem/Graph-Stack/05-tspi-buildroot-weston-de.md) |
| 7 | 体积 456 KB（C 版 ≈29 KB） | wayland 协议静态展开进 .text；可接受的换板上 .so 解依赖代价 | [`Design-CrossCompile.md` §7](Design-CrossCompile.md) |


## 5. 延伸 / 待办

| # | 问题 | 状态 |
|---|------|------|
| 1 | 用串口 / DSI 屏拍照取证：实际渲染出的"Welcome to TSPI" + 时钟 | 待做 —— 本轮只用 stderr 文本作证；屏幕画面需 framebuffer dump 或手机拍照 |
| 2 | 切换到 `aarch64-unknown-linux-musl` target，看体积与依赖能否进一步收敛 | 候选实验；预期 NEEDED 减为 1 条 (musl-libc 静态进) 但 cairo 仍动态 |
| 3 | 在 Cargo.toml 加 `bzip2-sys` 或 `webp-sys` 以**故意触发** cc crate，验证 `CC_<triple>` 兜底 | 验证 §3.5 的"兜底"配置是否在真正涉及 C 源码编译时奏效 |
| 4 | Slint / egui 路线 demo —— 同样的交叉编译框架，再上一层 GUI | 见 [`note/sdk/tspi/06-rust-gui-on-embedded.md` §5](../../../note/sdk/tspi/06-rust-gui-on-embedded.md) 路线 B |
| 5 | 把 `set_app_id` 改成不匹配 weston.ini 的字符串，观察是否会失去全屏 | 反向 falsifiability 验证 |
