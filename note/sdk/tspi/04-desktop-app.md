---
title: Tspi-rk3566-sdk 桌面 App 体系拆解与 LVGL Demo 运行手册
tags: [tspi, rk3566, buildroot, weston, lvgl, rk_demo, drm]
desc: 拆解 tspi-rk3566-sdk 中两条互斥的桌面/UI 路径——Weston Wayland 桌面 与 LVGL rk_demo 直打 DRM——并给出 lvgl_demo 的开关、编译、运行步骤。
update: 2026-05-13

---


# Tspi-rk3566-sdk 桌面 App 体系拆解与 LVGL Demo 运行手册

> [!note]
> **Ref:**
> - [buildroot/configs/rockchip/gui/](../../../sdk/tspi-rk3566-sdk/buildroot/configs/rockchip/gui/)
> - [buildroot/package/rockchip/lvgl_demo/](../../../sdk/tspi-rk3566-sdk/buildroot/package/rockchip/lvgl_demo/)
> - [buildroot/package/weston/S49weston](../../../sdk/tspi-rk3566-sdk/buildroot/package/weston/S49weston)
> - [buildroot/board/rockchip/common/overlays/10-weston/](../../../sdk/tspi-rk3566-sdk/buildroot/board/rockchip/common/overlays/10-weston/)
> - [app/lvgl_demo/rk_demo/](../../../sdk/tspi-rk3566-sdk/app/lvgl_demo/rk_demo/)
> - 关联笔记：[[03-buildroot-board]]（overlay/post-build 机制）

## 1. 总览：两条互斥的 UI 路径

tspi-rk3566-sdk 把 "桌面 / 整屏 app" 抽象成**三套 GUI Stack**，由 `buildroot/configs/rockchip/gui/` 三个互斥的 `.config` 片段决定：

| 配置片段 | UI 栈 | 桌面性质 | 典型场景 |
|---|---|---|---|
| `weston.config` | Wayland + Weston compositor | 现代合成器 + panel + launcher | 通用 Linux 桌面 |
| `x11.config` | Xorg + Openbox + xterm/xeyes | 传统 X11 | 老软件兼容 |
| `lvgl.config` | LVGL 直打 DRM/KMS | 没有 compositor，纯整屏 app | 工业控制屏 / 智能家居中控 |

tspi 默认走 **Weston**：

```sh
$ grep gui/ buildroot/configs/rockchip_rk3566_defconfig
#include "gui/weston.config"
```

且当前 build 出来的 `.config` 验证：`BR2_PACKAGE_WESTON=y`、**没有** `BR2_PACKAGE_LVGL_DEMO`。

## 2. Weston 桌面是怎么"长出来"的

两件事拼出来：

### 2.1 进程：buildroot 的 init.d 脚本

`buildroot/package/weston/S49weston` 装到 `/etc/init.d/`，开机执行：

```sh
. /etc/profile
/usr/bin/weston 2>&1 | tee /var/log/weston.log &
```

走的是经典 SysV `Default-Start: 2 3 4 5`，没有用 systemd。

### 2.2 外观：fs-overlay 注入

桌面背景、panel、launcher 全部在 `buildroot/board/rockchip/common/overlays/10-weston/`，由 [[03-buildroot-board]] 描述的 **Stage 2 二级 overlay** 机制按 `BR2_PACKAGE_WESTON` 条件注入：

```
10-weston/
├── prepare.sh                       # [ "$BR2_PACKAGE_WESTON" ] 守门
├── etc/xdg/weston/weston.ini        # backend=drm-backend.so / panel / 关锁屏
├── etc/xdg/weston/weston.ini.d/
│   ├── 01-launcher.ini              # panel 上的 terminal/camera/video 按钮
│   ├── 02-desktop.ini               # 桌面背景图 + panel-position=bottom
│   ├── 03-desktop-launcher.ini      # 桌面图标 (editor/EGL test/glmark2)
│   └── 04-desktop-launcher-group.ini# 图标分组排版
└── usr/share/backgrounds/background_linux.jpg
```

旁边的 `11-weston-chromium/` 是同一机制——再叠一个 overlay 把 Chromium 当 launcher 塞进 weston。

## 3. LVGL 系 app：`app/lvgl_demo/`

这是 tspi 出厂屏幕真正"花哨"的那套 UI，**不走 Weston，直接抢 DRM**。

### 3.1 仓库 → buildroot 的接线

`buildroot/package/rockchip/lvgl_demo/lvgl_demo.mk`：

```mk
LVGL_DEMO_SITE = $(TOPDIR)/../app/lvgl_demo
LVGL_DEMO_SITE_METHOD = local
LVGL_DEMO_DEPENDENCIES += lvgl
```

把仓库根的 `app/lvgl_demo/` 当**本地 CMake 包**接进 buildroot；这是 Rockchip "在仓库里写应用代码，又被 buildroot 编出来"的标准做法。

### 3.2 `Config.in` 旋钮速查

```
BR2_PACKAGE_LVGL_DEMO
├── BR2_LVGL_DEMO_OFFICIAL_DEMOS      # LVGL 上游官方 demo
│   └── choice: widgets / benchmark / music
└── BR2_LVGL_DEMO_RK_DEMO             # Rockchip 自家整屏 UI（推荐）
    ├── BR2_RK_DEMO_ENABLE_MULTIMEDIA # 自动拉 rkadk + rockit + rkwifibt-app
    ├── BR2_RK_DEMO_ENABLE_SENSOR
    ├── BR2_RK_DEMO_ENABLE_WIFIBT
    └── BR2_RK_DEMO_ENABLE_ASR

渲染后端三选一（默认 DRM）：
    LV_DRV_USE_DRM    → 依赖 libdrm + libevdev
    LV_DRV_USE_SDL_GPU→ 依赖 sdl2
    LV_DRV_USE_RKADK  → 走 Rockchip MPP 通路
```

### 3.3 rk_demo 子模块

`app/lvgl_demo/rk_demo/` 是一整套智能家居中控 UI，子目录都是 LVGL 屏：

| 目录 | 功能 |
|---|---|
| `home/` | 首页 |
| `smart_home/` | 整屏智能家居（空调 / 音乐 / 天气 / 场景 / 开关 / 日期） |
| `furniture_control/` | 家具控制 |
| `intercom_homepage/` | 楼宇对讲 |
| `amp_monitor/` | 功放监控 |
| `gallery/`、`flexbus/`、`motor_demo/` | 其他独立 demo |
| `asr/`、`audio_algorithm/`、`rockit/`、`wifibt/` | multimedia 旋钮打开时的能力模块 |
| `resource/` | 图标 / 动画 / 字体 |

### 3.4 自启动入口

`app/lvgl_demo/rk_demo/CMakeLists.txt:67`：

```cmake
install(PROGRAMS S10lv_demo DESTINATION /etc/init.d)
```

`S10lv_demo` 脚本本身就干一件事：

```sh
export LV_DRIVERS_SET_PLANE=CURSOR  # 用 cursor plane，不抢 primary
ulimit -n 1024                       # rk_demo 资源文件多，默认 fd 不够
rk_demo &
sleep 1                              # 让 rk_demo 先抢到屏幕
```

## 4. 整体串成一张图

```mermaid
flowchart TD
    A["device/rockchip/rk3566_rk3568/<br/>rockchip_rk3566_taishanpi_1m_v10_defconfig"] --> B["buildroot/configs/<br/>rockchip_rk3566_defconfig"]

    B -.include.-> C["gui/weston.config"]
    B -.可选 include.-> D["gui/lvgl.config"]

    subgraph WST["Weston 路径"]
        direction TB
        C --> E["package/weston<br/>S49weston (init.d)"]
        C --> F["board/rockchip/common/overlays/<br/>10-weston/ (fs-overlay)"]
        F -.post-build.sh.-> R["rootfs"]
        E --> R
    end

    subgraph LVG["LVGL 路径"]
        direction TB
        D --> G["package/rockchip/lvgl_demo<br/>(SITE = app/lvgl_demo)"]
        G --> H["/usr/bin/rk_demo<br/>+ /etc/init.d/S10lv_demo"]
        H --> R
        G -.opt: MULTIMEDIA.-> M["rkadk / rockit /<br/>rkwifibt-app"]
        M --> R
    end

    R --> Z["/dev/dri/card0 (DRM)"]
```

**一句话总结**：tspi 的"桌面"其实是**两套不相干的可选体系**——一套是 *Weston compositor + fs-overlay 注入的 launcher*（buildroot 视角的标准 Wayland 桌面），另一套是 *LVGL 写的 rk_demo 整屏 app*（直接打 DRM，靠 `S10lv_demo` 抢屏，appearance = application）。两者通过 `buildroot/configs/rockchip/gui/*.config` 和 `Config.in` 旋钮在编译期选定，靠 `common/post-build.sh` + `overlays/*/prepare.sh` 把对应文件铺进 rootfs。

## 5. 同级 `app/` 的另两个目录不是"桌面 app"

- `app/rkadk/` —— Rockchip Audio Dev Kit（RTSP / 录像 / 播放器 / 相机封装），是 rk_demo 在 multimedia 旋钮打开时调用的库。
- `app/rkpartybox/` —— K 歌派对音箱 reference design，整机 demo，跟桌面无关。

## 6. 实战：把 lvgl_demo 跑起来

> [!IMPORTANT]
> 当前 tspi 默认 build 里 **只装了 Weston，没装 lvgl_demo**（`.config` 中没有 `BR2_PACKAGE_LVGL_DEMO=y`）。要跑 lvgl_demo 必须先开关、再重编、再上板。

### 6.1 进 SDK 环境

```sh
cd ~/imx/sdk/tspi-rk3566-sdk
source ~/imx/tspi_env.sh                                  # 清 WSL Windows PATH，参见 01-build-sh.md
./build.sh rockchip_rk3566_taishanpi_1m_v10_defconfig     # 确认板级
```

### 6.2 menuconfig 勾上 lvgl_demo

```sh
./build.sh bconfig    # = buildroot-config，弹出 menuconfig
```

`/` 搜 `LVGL_DEMO` 跳到：

```
Target packages
└── Rockchip BSP packages
    └── [*] LVGL DEMO                                    ← BR2_PACKAGE_LVGL_DEMO
            [*] Enable demos from Rockchip              ← BR2_LVGL_DEMO_RK_DEMO（推荐）
                [ ] RK demo enable multimedia
                [ ] RK demo enable WiFi and BT
                [ ] RK demo enable sensor
                [ ] RK demo enable ASR
```

同时确认渲染后端依赖（默认应已 `=y`）：

```
Libraries / Graphics
└── [*] lvgl
└── [*] lv_drivers
        [*] Use Linux DRM         ← BR2_LV_DRIVERS_USE_DRM
Libraries / Hardware handling
└── [*] libdrm
└── [*] libevdev
```

> 想跑 LVGL 上游官方 demo（widgets / benchmark / music），把 `BR2_LVGL_DEMO_RK_DEMO` 关掉，改勾 `Enable demos from LVGL team` → 三选一。

存盘退出后立即把改动落到 defconfig，避免下次 `cleanall` 丢失：

```sh
./build.sh bmake savedefconfig
```

### 6.3 重编 rootfs

```sh
./build.sh bmake lvgl_demo-rebuild   # 只重编 lvgl_demo
./build.sh buildroot                  # 把改动卷进 rootfs.img
./build.sh updateimg                  # 重刷整机才用
```

### 6.4 上板（两条路）

**路径 A：NFS 调试**（最省事，已有 `prj/mount/` ↔ EVB `/mnt/`）

```sh
# Host
STAGE=buildroot/output/rockchip_rk3566_taishanpi_1m_v10/rockchip_rk3566/target
cp    $STAGE/usr/bin/rk_demo        ~/imx/prj/mount/
cp -r $STAGE/usr/share/rk_demo      ~/imx/prj/mount/
cp    $STAGE/etc/init.d/S10lv_demo  ~/imx/prj/mount/
```

```sh
# Target (串口控制台，因为接下来要把 weston 干掉)
/etc/init.d/S49weston stop          # 让出屏幕，避免两个 DRM master 抢屏
export LV_DRIVERS_SET_PLANE=CURSOR
ulimit -n 1024
/mnt/rk_demo &
```

**路径 B：重刷整机**

`S10lv_demo` 会被装到 `/etc/init.d/`，但 Weston 的 `S49weston` 也在跑，两者都拿 DRM master 会冲突。二选一：

- **想留 Weston，按需手起 lvgl**：`chmod -x /etc/init.d/S10lv_demo`，需要时 `rk_demo &`。
- **rk_demo 当出厂 UI**：`chmod -x /etc/init.d/S49weston`（或 menuconfig 关 `BR2_PACKAGE_WESTON` 重编），让 `S10lv_demo` 独占屏幕。

```sh
./build.sh updateimg
# rockdev/update.img → RKDevTool / upgrade_tool 烧机
```

### 6.5 关键变量速查

| 变量 / 文件 | 含义 |
|---|---|
| `LV_DRIVERS_SET_PLANE=CURSOR` | 让 lvgl 用 cursor plane，不抢 primary（Weston 共存时的妥协） |
| `ulimit -n 1024` | rk_demo 打开的资源文件多，默认 fd 数不够 |
| `app/lvgl_demo/rk_demo/asound.conf` | 音频默认路由（开 multimedia 时才有意义） |
| `app/lvgl_demo/rk_demo/resource/` | UI 资源，装到 `/usr/share/rk_demo/` |
| `/var/log/weston.log` | Weston 出问题先看它 |

### 6.6 推荐踩坑顺序

1. **先验工具链路径**：`./build.sh shell` 后 `which arm-buildroot-linux-gnueabihf-gcc`，确认 PATH 干净。
2. **先 NFS 跑通再刷机**：`bmake lvgl_demo-rebuild` 5~10 分钟就能出二进制；不要急着 `updateimg`。
3. **第一次跑用串口看 `rk_demo` 的 stderr**：DRM / evdev 任何一个权限不对都会立刻 panic。
4. **想停掉看输出**：`pkill -f rk_demo`，再 `LV_DRIVERS_SET_PLANE=CURSOR rk_demo` 前台跑。

> [!note]
> tspi 默认 `ssh tspi` 走 RNDIS（USB Gadget），RNDIS 没起来时只能用串口控制台——这跟 lvgl_demo 共用 DRM 设备时调试要走串口是同一个 constraint，参见 `02-usb-gadget-rndis-adb-fix.md`。
