---
title: 泰山派(RK3566) WLAN与WPA网络配置指南
tags: [tspi, rk3566, wifi, wpa_supplicant, network]
desc: 泰山派开发板通过 wpa_supplicant 配置无线网络及排错指南。
update: 2026-04-26

---


# TSPI WLAN & WPA Network Configuration

本文档整理了在泰山派（RK3566）开发板上使用 `wpa_supplicant` 和 `wpa_cli` 进行 WiFi 连接的完整流程、常见报错排查以及开机自启机制。

## 1. 使用 wpa_cli 管理网络

`wpa_cli` 是与 `wpa_supplicant` 守护进程交互的命令行工具。

### 1.1 清理已有网络

当配置文件混乱时，可通过交互模式清理：

```bash
wpa_cli
> list_networks       # 列出所有网络，第一列为 network id
> remove_network <id> # 删除指定 ID 的网络
> remove_network all  # 删除所有网络
> save_config         # 保存更改到配置文件
> quit
```

## 2. 配置文件设置

配置文件通常位于 `/etc/wpa_supplicant.conf`（在泰山派中，`/var/run/wpa_supplicant/wpa_supplicant.conf` 是指向它的软链接）。

### 2.1 推荐配置（避免 P2P 报错）

为了连接标准的 WPA2-Personal（WPA2-PSK）网络，并屏蔽无关的 P2P（Wi-Fi Direct）报错，建议在配置文件顶部添加全局配置，并在网络块中指定 `key_mgmt`。

```text
# 全局配置
ctrl_interface=/var/run/wpa_supplicant
update_config=1
p2p_disabled=1  # 关键：禁用 P2P 以消除 p2p-dev-wlan0 初始化失败的报错

# 网络配置
network={
    ssid="Your_SSID"
    psk="Your_Password"
    key_mgmt=WPA-PSK    # 兼容 WPA/WPA2 个人版
    proto=RSN           # 推荐：强制指定使用 WPA2 (RSN)
    pairwise=CCMP       # 推荐：指定 AES 加密
    group=CCMP
}
```

## 3. 手动启动与连接验证

修改配置后，需要重启守护进程并获取 IP。

### 3.1 重启 wpa_supplicant

```bash
killall wpa_supplicant
# -B 表示后台运行，-i 指定接口，-c 指定配置
wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant.conf
```

> [!note]
> **P2P 报错说明**：如果未配置 `p2p_disabled=1`，启动时会出现 `Failed to enable P2P Device interface` 等错误。这仅仅是板子 Wi-Fi 驱动不支持或未开启 Wi-Fi 直连功能，**完全不影响连接路由器（Station模式）**，可忽略。

### 3.2 确认连接状态与获取 IP

```bash
# 1. 检查状态
wpa_cli status
# 关注 wpa_state=COMPLETED 表示密码正确，连接路由器成功

# 2. 手动获取 IP (DHCP)
udhcpc -i wlan0

# 3. 验证网络
ifconfig wlan0
ping -c 4 114.114.114.114
```

### 3.3 DNS 配置补充

如果能 Ping 通 IP（如 114.114.114.114）但无法 Ping 通域名（如 www.baidu.com），需配置 DNS：
```bash
echo "nameserver 114.114.114.114" > /etc/resolv.conf
```

## 4. 泰山派(RK3566) 网络接口与开机自启

### 4.1 网络接口解析

通过 `ifconfig -a` 和 `ls -l /sys/class/net/` 观察到的接口：
- **`wlan0`**: Wi-Fi Station 模式接口（连接外部路由器）。
- **`wlan1`** (或 p2p0): Wi-Fi AP 模式或直连虚拟接口。
- **`eth0`**: 原生以太网接口（RJ45网口）。挂载在平台总线 (`fe010000.ethernet`) 上，由 `rk_gmac-dwmac` / `stmmac` 驱动。
- **`usb0`** (如有): 开启 USB Gadget RNDIS 功能后虚拟出的网卡。

### 4.2 自动连接机制

泰山派的 Buildroot 固件中包含了完整的自动化网络启动链路，配置保存后**重启会自动连接并获取 IP**：

1. **驱动加载**: `/etc/init.d/S36wifibt-init.sh` 负责探测芯片、加载驱动、并启动接口。
2. **连接热点**: 系统初始化时，依靠 `/etc/network/interfaces.d/wlan0` 的配置触发 `wpa_supplicant` 读取您的密码配置。
3. **自动获取 IP**: `/etc/init.d/S41dhcpcd` 服务会在后台监听接口状态，当 `wlan0` 连接成功后全自动执行 DHCP 流程获取 IP。
