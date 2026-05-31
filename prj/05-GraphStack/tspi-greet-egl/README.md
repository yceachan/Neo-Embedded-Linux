---
title: tspi-greet-egl —— wayland-egl + GLES2 客户端,板上点亮 ARM Mali 闭源栈
tags: [tspi, rk3566, egl, gles2, wayland-egl, mali, demo, kiosk-shell]
desc: 以 tspi-greet (C, wl_shm+cairo) 为对照,改走 wayland-egl + GLES2 路径,真正触达 libmali → /dev/mali0,渲染一段时变色环作为现象证据
update: 2026-05-21

---


# tspi-greet-egl

> [!note]
> **Ref:**
> - 设计文档：[`Design-EGL-Path.md`](Design-EGL-Path.md) (控制流轴) ; [`Design-LinkChain.md`](Design-LinkChain.md) (链接/运行时轴)
> - 实测结论：[`Conclude.md`](Conclude.md)
> - 对照样本：[`../tspi-greet/`](../tspi-greet/) (C 版 SHM) ; [`../tspi-greet-rs/`](../tspi-greet-rs/) (Rust 版 SHM)


## 1. 目标 / 现象

本 demo 与 `tspi-greet` (SHM + cairo CPU 软渲染) 形成对照，**走 wayland-egl + GLES2 真正点亮 tspi (RK3566) 板上的 ARM Mali 闭源用户态栈**：

> 客户端 → `-lEGL/-lGLESv2`（sysroot 软链塌缩）→ ELF.NEEDED libmali.so.1 → 板上 ld.so 加载 libmali.so.1.9.0 (56 MB) → libmali 内部 `open("/dev/mali0")` → Mali-G52 GPU 渲染 → eglSwapBuffers 导出 dma-buf → 经 zwp_linux_dmabuf_v1 协议给 Weston → rockchip-drm KMS 上屏

成功标志是 7 项可直接观测的证据全部齐备 —— 详见 [`Conclude.md §1`](Conclude.md)。


## 2. 项目结构

```
tspi-greet-egl/
├── README.md              # 本文档
├── Design-EGL-Path.md     # 控制流轴: EGL/GLES API 走读 + 时序图
├── Design-LinkChain.md    # 链接/运行时轴: NEEDED → ld.so → /dev/mali0
├── Conclude.md            # 真实日志 + 逐现象分析 + 结论表
├── Makefile               # 跨编入口 (sysroot + wayland-scanner)
├── build.sh / deploy.sh / test.sh
├── etc/
│   └── 00-kiosk-egl.ini   # weston kiosk-shell 配置(替换 SHM 版)
├── src/
│   ├── main.c             # wayland + xdg-shell + wl_egl_window + EGL ctx
│   ├── render.c / render.h# GLES2 时变渐变 shader
│   ├── xdg-shell-client.h # wayland-scanner 生成
│   └── xdg-shell-protocol.c
└── output/
    ├── bin/tspi-greet-egl
    └── log/{build,verify,run}.log
```


## 3. 部署

本包`app-ids=com.tspi.greet` 与板上weston-kisok-shell.ini 侦听toplevel一致，准备PATH后直接启动即可

```sh
export $WAYLAND_DISPLAY=wayland-0
```


## 4. 一键测试

```sh
cd prj/05-GraphStack/tspi-greet-egl
./test.sh
```

**链路**（5 步）：

1. `make clean`
2. 交叉编译（buildroot aarch64-gcc + wayland-scanner 生成 xdg-shell-client.h）
3. 静态校验：`file` / `readelf -d` / `pkg-config egl glesv2 wayland-egl`
4. `deploy.sh` 把 binary + ini 推到 `prj/nfs_tspi/tspi-greet-egl/`
5. ssh 到板上：杀掉旧 greet 客户端 → 后台启动本 demo → 2 秒后抓 `/proc/$PID/{maps,fd}` + `weston-screenshooter`（best-effort）→ 4 秒后 kill → tee stdout

**断言**：

- 静态：ELF 是 aarch64 + NEEDED 包含 `libmali.so.1` + NEEDED **不**含 `libEGL.so`/`libGLESv2.so`（反向断言）
- 运行时：`/proc/maps` 见 `libmali.so.1.9.0` + `/proc/fd` 见 `/dev/mali0` + stdout 见 `GL_RENDERER` 含 `Mali|G52` + 渲染帧数 > 0

**退出码**：

- 0：本地校验过 + 板上 ssh 通且证据齐
- 0：本地校验过 + ssh 不通（降级，只本地证明工具链 OK）
- 2：静态校验失败
- 4：板上跑通但证据缺失

**日志**：`output/log/build.log`、`output/log/verify.log`、`output/log/run.log`。

**首页推荐阅读**：跑完先看 [`Conclude.md §2.2`](Conclude.md) 的"板上运行段" —— 那里有 EGL/GL 字符串、`/proc/maps`、`/proc/fd` 三组直接证据。


## 5. 清理

```sh
make clean                  # 清 output/ 与 wayland-scanner 生成产物
rm -rf output/log/*          # 仅清日志
```

如果做过 §3.2 的板上替换，回退：

```sh
ssh tspi 'mv /etc/xdg/weston/weston.ini.d/00-kiosk.ini.bak.shm /etc/xdg/weston/weston.ini.d/00-kiosk.ini \
          && /etc/init.d/S49weston restart'
```
