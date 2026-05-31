# RK3568 当作 USB 音频设备 (UAC2 麦+扬声器)

## 结论

**有现成方案。** Rockchip Linux SDK 已经把 USB Audio Class (UAC1/UAC2) 整合到 `/etc/init.d/S50usbdevice` 一键脚本里,内核 driver 直接虚拟出 PCM 声卡,**Device 端无须真实 ACodec** 就能跑通 UAC2。最小步骤:

```sh
echo usb_uac2_en > /tmp/.usb_config
/etc/init.d/S50usbdevice restart
```

PC 立即识别到一个 USB Audio 输入(麦克风)+输出(扬声器)设备;Device 端 `aplay -l` 会出现 `USB Audio` 声卡。

---

## 大致流程

```
1. 内核侧打开 UAC config              →  CONFIG_USB_CONFIGFS_F_UAC2=y
2. (硬件) RK3568 OTG 口确认能进 Device 模式
                                       (Type-C/Micro-B 自动协商,或软件强切)
3. (运行时) S50usbdevice 通过 configfs
   构造 gadget,挂上 uac2.gs0 function,
   bind 到 /sys/class/udc/<otg> →  PC 枚举出 USB 音频设备
4. PC 选 USB Audio 作输入+输出 →  双向音频流通
```

### 第 1 步:内核配置

`menuconfig` → `Device Drivers → USB support → USB Gadget Support → USB Gadget Drivers`,确认:

```
CONFIG_USB_CONFIGFS_F_UAC1=y    # 可选
CONFIG_USB_CONFIGFS_F_UAC2=y    # 推荐(UAC2 = USB High Speed)
```

RK356x SDK 的默认 defconfig 通常已带这两个;若自己裁剪过 kernel,先确认。

### 第 2 步:OTG 端口方向

RK3566/RK3568 的 OTG 口默认走 ID/CC 自动协商。如果硬件没接 CC 检测、或者上电默认进 Host 看不到 Device 枚举,可以软件强切到 peripheral (RK356x USB 指南 p.31):

```sh
# Linux-5.10+ 推荐接口
echo device > /sys/kernel/debug/usb/fcc00000.usb/mode

# Legacy(依赖 DTS 的 extcon 配置)
echo peripheral > /sys/devices/platform/fe8a0000.usb2-phy/otg_mode
```

(节点地址以板子实际为准。RK3568 USB 3.0 OTG 控制器一般是 `fcc00000.usb`。)

或者直接在 DTS 把 OTG 节点 `dr_mode = "peripheral";` 写死。

### 第 3 步:运行时启用 UAC2

最简方式 (Buildroot SDK 默认就有):

```sh
echo usb_uac2_en > /tmp/.usb_config
/etc/init.d/S50usbdevice restart
```

成功 dmesg:

```
android_work: sent uevent USB_STATE=CONNECTED
configfs-gadget gadget: high-speed config #1: b
android_work: sent uevent USB_STATE=CONFIGURED
```

底下脚本做的事:在 `/sys/kernel/config/usb_gadget/rockchip/` 里 `mkdir` 出 gadget,写 `idVendor=0x2207 / idProduct=...`,在 `functions/uac2.gs0/` 下创建参数节点,symlink 到 `configs/b.1/`,最后把 UDC 名字写进 `UDC` 触发枚举 (UAC PDF p.9-11)。

### 第 4 步:参数微调

`/sys/kernel/config/usb_gadget/rockchip/functions/uac2.gs0/` 关键节点(UAC PDF p.13-14):

| 节点 | 含义 | 典型值 |
|------|------|--------|
| `p_chmask` / `c_chmask` | 播放/录音通道位图 | `3` 双声道 / `0x3F` 5.1 |
| `p_ssize` / `c_ssize` | 位深(字节) | `2`(16-bit) `3`(24-bit) |
| `p_srate` / `c_srate` | 采样率(可多值) | `8000,16000,44100,48000` |
| `p_feature_unit` / `c_feature_unit` | 音量控制开关 (UAC1) | UAC1=1, **UAC2 在 Windows 上必须=0** |
| `req_number` | URB buffer 数 | 默认 2,Mac 兼容性差时调大 |

记住方向定义:`c_*` (capture) = PC 录音 = Device 当**麦克风**上传;`p_*` (playback) = PC 放音 = Device 当**扬声器**接收 (UAC PDF p.13)。"麦+喇叭" 两个方向就是默认配置,不需要额外开启。

### 第 5 步:验证

Device 端把 PC 的 playback 流回环到 capture,看一条命令是否双向通 (Quick Start p.9):

```sh
# Device 上 hw:N 替换为实际 UAC card index
arecord -D "hw:N,0" -f S16_LE -r 48000 -c 2 -t raw -N |
  aplay  -D "hw:N,0" -f S16_LE -r 48000 -c 2 -t raw -N
```

PC 端在 Windows 声音设置 / Linux `pavucontrol` 把 USB Audio 选为输入+输出设备,放音乐 → Device 端听到 PC 端音频(若 Device 接了喇叭),对 PC 麦克风输入说话 → PC 端能收到。

---

## 几个会踩到的坑

1. **UAC2 + Windows 必须关 Feature Unit**:`echo 0 > c_feature_unit; echo 0 > p_feature_unit`,否则 Windows 加载 UAC2 驱动失败 (UAC PDF p.14、p.16 表 2-1)。Windows 10 1703 之前还要自己装第三方 UAC2 驱动,实在不行就退到 UAC1 (`usb_uac1_en`)。
2. **Windows 缓存设备**:改了 idProduct 或 function 组合,要在设备管理器里卸载旧 USB Audio 设备再重插,否则一直用旧描述符 (UAC PDF p.9、p.12)。
3. **`c` 和 `p` 别搞反**:`c_*` 是 Device → PC 的录音方向,`p_*` 是 PC → Device 的放音方向。
4. **异源时钟 underrun**:USB host 时钟 vs Device codec 时钟不同源会周期性 `alsa underrun/overrun`。SDK 在 RV1126-4.19 上有 PPM 补偿,RK356x 当前 SDK **未提供**,实际产品要么不用真实 codec(全虚拟 PCM 就没这问题),要么自己做时钟跟踪 (UAC PDF p.15 §2.3.3)。
5. **物理按键调音量/静音不在 SDK 范围**:需要叠 HID gadget function 自己加,参考内核 `Documentation/usb/gadget-testing.txt` (UAC PDF p.3)。
6. **OTG 默认进 Host**:很多 RK356x 板子 OTG 口默认 Host 模式(为了挂 U 盘),要么改 DTS `dr_mode = "peripheral"`,要么用 `echo device > /sys/kernel/debug/usb/<addr>.usb/mode` 强切。

---

## 涉及到的 PDF

按重要程度排序:

1. **主文档** — `sdk/tspi-rk3566-sdk/docs/cn/Common/USB/Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` (RK-KF-YF-098, V1.3.1, 2025-04-25)
   - p.6   §1.2 Related CONFIGs
   - p.7-9 §2.1-2.2 UAC1/UAC2 信息与默认参数
   - p.9-12 §2.3 configfs 配置脚本(Linux-4.4/4.19 + Linux-5.10 两套)
   - p.13-14 §2.3.1 参数接口 (`p_srate` / `c_chmask` / `feature_unit` / `req_number` ...)
   - p.14-15 §2.3.2 uevent 协议
   - p.16 §2.3.4 不同 Host OS 的兼容性矩阵
   - p.16-24 §2.4 测试方法 (Windows / Ubuntu 操作步骤)

2. **快速入门** — `sdk/tspi-rk3566-sdk/docs/cn/Linux/ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf`
   - p.6-7  `/etc/init.d/S50usbdevice` + `.usb_config` 一键方案的管理流程
   - p.8-10 §2.2.2 UAC 一键启用 + ALSA 测试命令 + 复合设备说明

3. **RK356x 平台 USB** — `sdk/tspi-rk3566-sdk/docs/cn/Common/USB/Rockchip_RK356x_Developer_Guide_USB_CN.pdf`
   - p.4-5 §1 RK356x USB 控制器拓扑(USB2 OTG + USB3 OTG)
   - p.17-30 §3 DTS 配置参考(OTG / Type-C / VBUS)
   - p.31 §4 OTG mode 强制切换命令(Linux-5.10 的 `/sys/kernel/debug/usb/<addr>.usb/mode`)

4. (可选)`sdk/tspi-rk3566-sdk/docs/cn/Common/USB/Rockchip_Developer_Guide_USB_CN.pdf` — USB 通用框架,只在需要打底时翻。

---

## 引用与复用

- 本次蒸馏:`.claude/skills/rk/references/distilled/usb-gadget-uac-rk356x.md`(已登记到 `distilled/INDEX.md`),下次类似问题直接读它就够,不必再开 PDF。
