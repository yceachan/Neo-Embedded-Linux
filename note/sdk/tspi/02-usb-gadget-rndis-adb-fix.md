---
title: TSPI RK3566 USB Gadget (RNDIS/ADB) 与 NFS 修复复盘
tags: [sdk, rk3566, tspi, usb-gadget, rndis, adb, nfs]
desc: 泰山派 RK3566 NFS 挂载与 RNDIS/ADB 复合设备共存修复笔记。
update: 2026-04-26
---

# TSPI RK3566 RNDIS/ADB 与 NFS 修复精炼

> 本文档记录了解决 NFS 挂载失败及 USB Gadget 冲突，最终实现 ADB 与 RNDIS 复合设备完美共存的核心流程与代码补丁。为提高可读性，按系统层级重构了原理说明与具体补丁的映射关系。

## 1. Kernel 层：RNDIS 支持与 UDC 硬件释放

### 1.1 清理旧版驱动，开启 ConfigFS
*   **痛点**：内核默认开启了传统网卡 Gadget，阻碍了动态 ConfigFS 接管 USB 控制器。
*   **方案**：在内核配置 (`./build.sh kconfig`) 中彻底禁用 `CONFIG_USB_ETH` 及相关旧架构，开启 ConfigFS ECM/RNDIS。
*   **补丁 (`sdk/tspi-rk3566-sdk/kernel-6.1/arch/arm64/configs/rockchip_linux_defconfig`)**:
    ```diff
    --- a/arch/arm64/configs/rockchip_linux_defconfig
    +++ b/arch/arm64/configs/rockchip_linux_defconfig
    @@ -502,9 +502,15 @@ CONFIG_USB_GADGET_VBUS_DRAW=500
     CONFIG_USB_CONFIGFS=y
     CONFIG_USB_CONFIGFS_UEVENT=y
     CONFIG_USB_CONFIGFS_ACM=y
    +CONFIG_USB_CONFIGFS_ECM=y
    +CONFIG_USB_CONFIGFS_ECM_SUBSET=y
    +CONFIG_USB_CONFIGFS_RNDIS=y
    +CONFIG_USB_CONFIGFS_EEM=y
     CONFIG_USB_CONFIGFS_MASS_STORAGE=y
     CONFIG_USB_CONFIGFS_F_FS=y
     CONFIG_USB_CONFIGFS_F_UVC=y
    +# CONFIG_USB_ETH is not set
    +# CONFIG_USB_ETH_RNDIS is not set
     CONFIG_TYPEC_TCPM=y
     CONFIG_TYPEC_TCPCI=y
     CONFIG_TYPEC_HUSB311=y
    ```

### 1.2 屏蔽物理引脚干涉
*   **痛点**：底层设备树会监听 Type-C 物理状态，易误判为 Host 模式导致外设注册失败。
*   **方案**：强制删除 `extcon` 属性，固定为从机 (peripheral) 模式。
*   **补丁 (`sdk/tspi-rk3566-sdk/kernel-6.1/arch/arm64/boot/dts/rockchip/tspi-rk3566-user-v10-linux.dts`)**:
    ```diff
    --- a/arch/arm64/boot/dts/rockchip/tspi-rk3566-user-v10-linux.dts
    +++ b/arch/arm64/boot/dts/rockchip/tspi-rk3566-user-v10-linux.dts
    @@ -194,3 +194,10 @@ ir_key1 {
                            <0xe6   KEY_0>;
            };
     };
    +
    +//Force USB Type-C to act as peripheral (Gadget) for RNDIS/ADB
    +&usbdrd_dwc3 {
    +       dr_mode = "peripheral";
    +       /delete-property/ extcon;
    +       status = "okay";
    +};
    ```

---

## 2. Buildroot 层：NFS 客户端支持

### 2.1 添加 NFS 组件解决挂载失败
*   **痛点**：RK3566 默认极简文件系统无 NFS 客户端，导致 `mount -a` 失败。
*   **方案**：开启 `nfs-utils` 和 `NFS client`（可取消 `rpc.nfsd` 节省空间），重新编译 `rootfs.img`。
*   **补丁 (`sdk/tspi-rk3566-sdk/buildroot/configs/rockchip_rk3566_defconfig`)**:
    ```diff
    --- a/configs/rockchip_rk3566_defconfig
    +++ b/configs/rockchip_rk3566_defconfig
    @@ -22,6 +22,9 @@
     #include "gui/weston.config"
     BR2_PACKAGE_IPERF3=y
     BR2_PACKAGE_MINICOM=y
    +BR2_PACKAGE_NFS_UTILS=y
    +BR2_PACKAGE_NFS_UTILS_NFSV4=y
    +# BR2_PACKAGE_NFS_UTILS_RPC_NFSD is not set
     BR2_PACKAGE_OPENSSH=y
     BR2_PACKAGE_QUECTEL_CM=y
     BR2_PACKAGE_TINYALSA=y
    ```

---

## 3. Rootfs 层：ADB-RNDIS 兼容与网络自启
### 3.1 规避 USB 脚本正则解析陷阱
*   **痛点**：官方 `/usr/bin/usb-gadget` 脚本对配置文件解析存在正则缺陷，遇到 `=on` 等赋值会截断异常。
*   **方案**：配置文件中**严禁使用 `=on`**，仅保留宏名本身。
*   **写入文件 (`/etc/.usb_config`)**:
    ```text
    usb_adb_en
    usb_rndis_en
    ```

### 3.2 RNDIS 自动分配 IP 与路由配置
*   **痛点**：RNDIS 虚拟网卡 `usb0` 生成后默认无网络配置。
*   **方案**：编写 `usb-gadget` 启动后置钩子脚本，自动配置 IP 并打通 Windows 网络共享 (ICS) 的 `192.168.137.x` 网段路由及 DNS。
*   **写入文件 (`/etc/usb-gadget.d/rndis.sh`)**:
    ```bash
    #!/bin/sh
    rndis_post_start_hook()
    {
        while ! ifconfig usb0 >/dev/null 2>&1; do
            sleep .1
        done
        ifconfig usb0 192.168.137.2 netmask 255.255.255.0 up
        route add default gw 192.168.137.1
        echo "nameserver 8.8.8.8" > /etc/resolv.conf
        echo "nameserver 114.114.114.114" >> /etc/resolv.conf
    }
    ```

---

## 4. Host (宿主机) 端配置：ADB 识别

### 4.1 添加 Vendor ID (VID) 白名单
*   **痛点**：由于泰山派作为复合设备时的 Vendor ID 为 `0x2207` (Rockchip)，某些情况下宿主机的 ADB 进程无法自动识别该非标/第三方设备的 USB 接口，导致 `adb devices` 找不到设备。
*   **方案**：在宿主机的用户目录下创建/修改 `.android/adb_usb.ini` 文件，强制 ADB 进程扫描该 VID。
*   **写入文件 (Windows: `C:\Users\<Name>\.android\adb_usb.ini` / Linux/WSL: `~/.android/adb_usb.ini`)**:
    ```ini
    0x2207
    ```
*   **生效命令**：配置完成后在宿主机终端执行 `adb kill-server` 与 `adb start-server`。

### 4.2 Windows 设备管理器状态验证
在 Windows “设备管理器” -> “设备属性” -> “详细信息” -> “硬件 Id” 中，该复合设备的接口通常显示为：
```text
USB\VID_2207&PID_0013&REV_0310&MI_02
USB\VID_2207&PID_0013&MI_02
```
*注：`PID_0013` 是 Rockchip 为 RNDIS+ADB 组合分配的 Product ID，`MI_02` 代表这是复合设备中的对应接口（通常代表 ADB 接口）。*

---

## 5. 结果验证
烧录并启动后，通过电脑端连接 Type-C 接口：
1. **多路网络**：开发板通过 `usb0` 顺畅连接外网，本地宿主机可通过局域网 IP (`192.168.137.2`) 进行 NFS 和 SSH 连接。
2. **复合 Gadget**：设备成功变身复合设备，支持 USB 物理底层 `adb shell` 及网络端 `adb connect 192.168.137.2:5555` 并行使用。