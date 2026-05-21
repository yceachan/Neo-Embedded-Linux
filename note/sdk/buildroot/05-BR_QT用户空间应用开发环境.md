---
title: Buildroot Qt 用户空间应用开发：从 tspi-rk3566 实测工具链到第一个跑起来的 Qt5 应用
tags: [buildroot, qt5, qt6, cross-compile, sysroot, tspi, rk3566, application]
desc: 以 tspi-rk3566-sdk 实测 buildroot/output 为起点，盘点已就绪 vs 还缺的工具链，分九步给出启用 Qt5、搭开发 env、写 helloworld、部署到板子的完整流程。
update: 2026-05-16

---


# Buildroot Qt 用户空间应用开发：从 tspi-rk3566 实测工具链到第一个跑起来的 Qt5 应用

> [!note]
> **Ref:**
> - tspi 实测 buildroot 输出：[`buildroot/output/latest/`](../../../sdk/tspi-rk3566-sdk/buildroot/output/) → 指向 `rockchip_rk3566_taishanpi_1m_v10/rockchip_rk3566/`
> - 主 defconfig：[`buildroot/configs/rockchip_rk3566_defconfig`](../../../sdk/tspi-rk3566-sdk/buildroot/configs/rockchip_rk3566_defconfig)
> - 模块化 config fragments：[`buildroot/configs/rockchip/gui/`](../../../sdk/tspi-rk3566-sdk/buildroot/configs/rockchip/gui/)
> - 相关笔记：[[03-BR_custom]] §4（cpp 预处理 defconfig）、[[02-BR_usage]] §4（`make sdk`）、[[00-用户空间应用开发与sysroot]]


## 1. 实测：tspi 当前的工具链状态

直接 ls `/home/pi/imx/sdk/tspi-rk3566-sdk/buildroot/output/latest/host/`，得到的结论：

### 1.1 已就绪（不用再编）

| 组件 | 路径 / 名字 |
|---|---|
| 交叉 gcc | `host/bin/aarch64-buildroot-linux-gnu-gcc 13.4.0` |
| 交叉 g++ | `host/bin/aarch64-buildroot-linux-gnu-g++` |
| binutils | `host/bin/aarch64-buildroot-linux-gnu-{ld,as,ar,strip,readelf,...}` |
| C 库 | glibc（`BR2_TOOLCHAIN_BUILDROOT_GLIBC=y`） |
| sysroot | `host/aarch64-buildroot-linux-gnu/sysroot/` |
| 一键 env | `host/environment-setup`（buildroot 自动生成） |
| pkg-config（host） | `host/bin/pkg-config` |
| cmake（host） | `host/bin/cmake`（buildroot 自带 host-cmake） |

### 1.2 还缺的（Qt 相关全部为 0）

```sh
$ find buildroot/output/latest -name "qmake*"            # 空
$ find buildroot/output/latest -name "Qt5Core.pc"        # 空
$ grep BR2_PACKAGE_QT5BASE buildroot/output/latest/.config
BR2_PACKAGE_QT5_JSCORE_AVAILABLE=y     ← 只有 capability 标志
BR2_PACKAGE_QT6_ARCH_SUPPORTS=y        ← 同上
# 真正的 BR2_PACKAGE_QT5BASE=y 一条都没有
```

`buildroot/configs/rockchip/gui/` 当前只有 `weston.config / x11.config / lvgl.config`——**没有 qt5.config**，这是要补的第一步。

### 1.3 buildroot 现成可用的 Qt 软件包

```sh
$ ls buildroot/package/ | grep -i '^qt'
qt5
qt5cinex
qt6
qt-webkit-kiosk
```

四个包都没启用。本笔记走 `qt5`。


## 2. 选 Qt5 还是 Qt6

| 维度 | Qt5（5.15） | Qt6（6.x） |
|---|---|---|
| buildroot 支持 | 长期稳定，模块全 | 新，部分模块没移植 |
| 编译时间 | 30–60 分钟 | 60–120 分钟（更慢） |
| GUI 后端 | EGLFS / Wayland / X11 都成熟 | Wayland 是主推 |
| C++ 标准 | C++11/14 默认 | C++17 默认 |
| 推荐场景 | 量产、生态库丰富 | 长期演进 |

**建议先 Qt5**——tspi 当前 Weston 已启用（`BR2_PACKAGE_WESTON=y` 在 [`gui/weston.config`](../../../sdk/tspi-rk3566-sdk/buildroot/configs/rockchip/gui/weston.config) 里），Qt5 走 Wayland backend 是最稳的组合。


## 3. Step 1 · 写 Qt5 config fragment 并接入

新建 [`buildroot/configs/rockchip/gui/qt5.config`](../../../sdk/tspi-rk3566-sdk/buildroot/configs/rockchip/gui/qt5.config)：

```text
# Qt5 base
BR2_PACKAGE_QT5=y
BR2_PACKAGE_QT5BASE=y
BR2_PACKAGE_QT5BASE_GUI=y
BR2_PACKAGE_QT5BASE_WIDGETS=y
BR2_PACKAGE_QT5BASE_NETWORK=y
BR2_PACKAGE_QT5BASE_LINUXFB=y
BR2_PACKAGE_QT5BASE_FONTCONFIG=y
BR2_PACKAGE_QT5BASE_DBUS=y

# Wayland backend (走 weston)
BR2_PACKAGE_QT5WAYLAND=y
BR2_PACKAGE_QT5WAYLAND_CLIENT=y

# QML / QtQuick (按需开)
BR2_PACKAGE_QT5DECLARATIVE=y
BR2_PACKAGE_QT5DECLARATIVE_QUICK=y
BR2_PACKAGE_QT5QUICKCONTROLS=y
BR2_PACKAGE_QT5QUICKCONTROLS2=y

# 图片格式
BR2_PACKAGE_QT5IMAGEFORMATS=y
BR2_PACKAGE_QT5SVG=y

# 多媒体（按需）
# BR2_PACKAGE_QT5MULTIMEDIA=y

# 中文字体 + i18n
BR2_PACKAGE_QT5TRANSLATIONS=y
```

接到主 defconfig：在 [`buildroot/configs/rockchip_rk3566_defconfig`](../../../sdk/tspi-rk3566-sdk/buildroot/configs/rockchip_rk3566_defconfig) 的 `#include "gui/weston.config"` 下面加一行：

```text
#include "gui/qt5.config"
```

> [!warning]
> 因为 cpp 预处理是 SDK 加的（见 [[03-BR_custom]] §4），**改 defconfig 后必须重新跑 SDK 入口 `./build.sh buildroot`**，不要直接进 buildroot 目录跑 make——否则新加的 `#include` 不会被展开成扁平 `BR2_*` 落到 `.config`。


## 4. Step 2 · 编译 Qt5

```sh
cd ~/imx/sdk/tspi-rk3566-sdk
./build.sh buildroot                              # 整个 rootfs 重做
# 或单包：
./build.sh bmake qt5base qt5wayland qt5declarative
```

预期时间：首次 30–90 分钟（取决于 host CPU）。完成后验证：

```sh
ls buildroot/output/latest/host/bin/qmake-qt5
ls buildroot/output/latest/host/aarch64-buildroot-linux-gnu/sysroot/usr/lib/libQt5Core.so.5
ls buildroot/output/latest/host/aarch64-buildroot-linux-gnu/sysroot/usr/lib/pkgconfig/Qt5Core.pc
```

三条都有输出就成。


## 5. Step 3 · 准备开发 env（直接用 output/latest，不打 SDK tarball）

buildroot 已自动生成 [`host/environment-setup`](../../../sdk/tspi-rk3566-sdk/buildroot/output/latest/host/environment-setup)，但它**只设了 CC/CXX/AR 这类 autotools 变量**，没设 `PKG_CONFIG_SYSROOT_DIR`、没把 `host/bin` 排到 PATH 最前。写一个补充脚本：

```sh
# ~/imx/sdk/tspi-rk3566-sdk/qt-env.sh
SDK_HOST=$HOME/imx/sdk/tspi-rk3566-sdk/buildroot/output/latest/host

# 1. 先 source buildroot 的官方脚本（拿到 CC/CXX/AR/...）
source "$SDK_HOST/environment-setup"

# 2. PATH：host/bin 优先（保证 qmake-qt5、moc-qt5 用 SDK 这份，不是 host apt 装的）
export PATH="$SDK_HOST/bin:$PATH"

# 3. sysroot
export SYSROOT="$SDK_HOST/aarch64-buildroot-linux-gnu/sysroot"

# 4. pkg-config 指向 sysroot
export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"
unset PKG_CONFIG_PATH      # 防止 host 的 .pc 文件被搜到

# 5. qmake 默认选 5
export QT_SELECT=qt5

echo ">>> Qt5 SDK env loaded"
echo "    qmake = $(which qmake-qt5)"
echo "    CC    = $CC"
echo "    SYSROOT = $SYSROOT"
```

使用：

```sh
source ~/imx/sdk/tspi-rk3566-sdk/qt-env.sh
```

**sanity check**：

```sh
qmake-qt5 -query QT_HOST_PREFIX QT_INSTALL_LIBS QT_INSTALL_HEADERS
# QT_HOST_PREFIX  应在 $SDK_HOST
# QT_INSTALL_LIBS 应在 $SYSROOT
pkg-config --cflags Qt5Core
# 应输出 -I$SYSROOT/usr/include/qt5/QtCore 等
```


## 6. Step 4 · 第一个 Qt5 应用（CMake 风格）

```sh
mkdir -p ~/work/qt-hello && cd ~/work/qt-hello
```

`main.cpp`：

```cpp
#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QLabel label("Hello, tspi! Qt " QT_VERSION_STR);
    label.resize(400, 200);
    label.show();
    return app.exec();
}
```

`CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.16)
project(qt-hello LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets)
add_executable(qt-hello main.cpp)
target_link_libraries(qt-hello PRIVATE Qt5::Widgets)
```

构建：

```sh
mkdir build && cd build
cmake .. \
  -DCMAKE_C_COMPILER=aarch64-buildroot-linux-gnu-gcc \
  -DCMAKE_CXX_COMPILER=aarch64-buildroot-linux-gnu-g++ \
  -DCMAKE_PREFIX_PATH=$SYSROOT/usr \
  -DCMAKE_FIND_ROOT_PATH=$SYSROOT
make -j$(nproc)

file qt-hello   # 应显示 ELF 64-bit LSB ... ARM aarch64
```


## 7. Step 5 · qmake 风格（项目用 .pro 文件）

`qt-hello.pro`：

```pro
QT += widgets
SOURCES += main.cpp
TARGET = qt-hello
```

```sh
qmake-qt5 qt-hello.pro
make -j$(nproc)
```

`qmake-qt5` 在编译它本身的时候已经 baked-in 了 sysroot 路径，不用额外参数。


## 8. Step 6 · 部署到板子上

```sh
# 通过 SSH 拷过去
scp qt-hello tspi:/tmp/

# 板上跑（QT_QPA_PLATFORM 二选一）
ssh tspi 'QT_QPA_PLATFORM=wayland /tmp/qt-hello'         # 走 weston
ssh tspi 'QT_QPA_PLATFORM=linuxfb /tmp/qt-hello'         # 直接 framebuffer
```

如果 NFS mount 还活着（参见 [[../../KernelLearning最佳实践]] 提到的本地 `prj/mount/` → EVB `/mnt/` 链路），更轻量：

```sh
cp qt-hello /home/pi/imx/prj/mount/
ssh tspi 'QT_QPA_PLATFORM=wayland /mnt/qt-hello'
```


## 9. Step 7 · 常见错误对照表

| 现象 | 真因 | 解法 |
|---|---|---|
| `qmake-qt5: command not found` | PATH 没把 `$SDK_HOST/bin` 排前 | 重新 source `qt-env.sh` |
| `Could not find QPA plugin "wayland"` | rootfs 没装 qt5wayland | 回 §3 加 `BR2_PACKAGE_QT5WAYLAND=y` 重打 |
| `error while loading shared libraries: libQt5Core.so.5` | rootfs 没装 qt5base | 同上，检查 `BR2_PACKAGE_QT5BASE=y` |
| `version 'GLIBC_2.36' not found`（板上） | 在 host 环境编的而非 SDK env | 重新 source `qt-env.sh`，确认 `which g++` 指向 `$SDK_HOST/bin/aarch64-...-g++` |
| pkg-config 找的是 host 的 Qt5Core | `PKG_CONFIG_SYSROOT_DIR` 没设或 `PKG_CONFIG_PATH` 干扰 | `unset PKG_CONFIG_PATH; echo $PKG_CONFIG_LIBDIR` 检查 |
| 编出来是 x86 ELF | CC 没切到 cross gcc | `file qt-hello` 确认是 aarch64；不是则检查 `echo $CC` |
| QML 程序起不来 | QtDeclarative + QtQuick 没装 | §3 加 `BR2_PACKAGE_QT5DECLARATIVE=y` + `BR2_PACKAGE_QT5QUICKCONTROLS2=y` |
| 中文方块字 | 没装中文字体；或 fontconfig 没起 | rootfs 装 `BR2_PACKAGE_WQY_ZENHEI=y` 或 `BR2_PACKAGE_DEJAVU=y`，并保证 `BR2_PACKAGE_QT5BASE_FONTCONFIG=y` |


## 10. Step 8 · 想发给同事 / CI 用？打 SDK tarball

```sh
cd ~/imx/sdk/tspi-rk3566-sdk
make -C buildroot O=$(readlink -f buildroot/output/latest) sdk
# 产物：buildroot/output/latest/images/aarch64-buildroot-linux-gnu_sdk-buildroot.tar.gz
```

解开后跑一次 `./relocate-sdk.sh` 即可在任何机器上 source 它的 `environment-setup` 干活。详见 [[02-BR_usage]] §4。


## 11. 最小路径速记

不想读全文，只要最快跑起来：

```mermaid
flowchart LR
    A["写 buildroot/configs/rockchip/gui/qt5.config<br/>(5 行最小集)"]
    B["主 defconfig 加 #include gui/qt5.config"]
    C["./build.sh buildroot<br/>(30-90 分钟)"]
    D["写 qt-env.sh 并 source"]
    E["qmake-qt5 hello.pro && make"]
    F["scp 上板 + QT_QPA_PLATFORM=wayland"]

    A --> B --> C --> D --> E --> F
```

五步完整覆盖：**defconfig fragment → 重编 buildroot → 写 env → 编译 → 部署**。


## 12. 把应用回流进 rootfs（长期方案）

开发期可以 scp，量产必须把 app 包成 buildroot package。在 `<BR2_EXTERNAL>/package/myapp/` 写：

`Config.in`：

```kconfig
config BR2_PACKAGE_MYAPP
    bool "myapp (Qt5 demo)"
    depends on BR2_PACKAGE_QT5BASE
    select BR2_PACKAGE_QT5BASE_WIDGETS
    help
      My first Qt5 demo on tspi.
```

`myapp.mk`：

```make
MYAPP_VERSION = 1.0.0-dev
MYAPP_SITE = $(BR2_EXTERNAL_MYCO_PATH)/../work/qt-hello
MYAPP_SITE_METHOD = local
MYAPP_LICENSE = proprietary
MYAPP_DEPENDENCIES = qt5base

$(eval $(qmake-package))
```

之后 `./build.sh buildroot` 时，app 会被作为 package 自动编译并装进 `/usr/bin/qt-hello`——不再需要手动 scp。详见 [[03-BR_custom]] §10。
