---
title: Rockchip SDK 文档库 Resources Map
tags: [rk, skill, reference, resources-map]
desc: 模块/关键词 → PDF 路径的 lookup table，供 /rk skill 在 cache miss 时定位源 PDF
update: 2026-05-27
---


# RK SDK 文档 Resources Map

> [!note]
> **Ref:** [`sdk/tspi-rk3566-sdk/docs/cn/readme_cn.md`](../../../../sdk/tspi-rk3566-sdk/docs/readme_cn.md) — 这份 map 是对 readme_cn 的查表化重组,只保留"关键词→路径"映射,不复述模块说明。
>
> **路径基准 `$RK_DOCS`** = `sdk/tspi-rk3566-sdk/docs/`(相对仓库根)。所有 PDF 路径都从 `cn/` 或 `en/` 开始。中英文目录结构镜像一致,只列 cn,英文版把 `cn/` 替换为 `en/`、文件名后缀 `_CN.pdf` 替换为 `_EN.pdf` 即可。

## 怎么用

1. 用户问"RK XX 怎么 YY"时,先在本文件 grep 关键词(中英都试,如 `pinctrl`、`引脚`、`CAN FD`)。
2. 命中后取 PDF 路径,交给 `scripts/pdf_text.py` 提 outline / 关键段。
3. 抽出的核心知识写入 `references/distilled/<topic>.md`,并登记到 `distilled/INDEX.md`。
4. 同一关键词二次询问时,先查 INDEX,命中 distilled 文件就直接用,不再开 PDF。


## 目录

- [一、总览类入口文档](#一总览类入口文档)
- [二、Common 模块文档](#二common-模块文档)
- [三、Linux 子系统文档](#三linux-子系统文档)
- [四、RK3566/RK3568 平台专属](#四rk3566rk3568-平台专属)
- [五、NPU / RKNN / RKLLM](#五npu--rknn--rkllm)
- [六、AVL 选型与硬件](#六avl-选型与硬件)
- [七、Others](#七others)
- [八、RT-Thread (RTOS)](#八rt-thread-rtos)


## 一、总览类入口文档

| 用途 | 路径 |
|------|------|
| SDK 通用软件开发入门 | `cn/Rockchip_Developer_Guide_Linux_Software_CN.pdf` |
| RK3566/3568 SDK 发布说明 | `cn/RK3566_RK3568/Rockchip_RK3566_RK3568_Linux6.1_SDK_Release_V1.3.0_20251220_CN.pdf` |
| docs 全树 | `cn/docs_list_cn.txt` |
| 官方 readme(模块清单+说明) | `docs/readme_cn.md` |


## 二、Common 模块文档

路径前缀统一为 `cn/Common/<MODULE>/`。

### 通信总线

| 关键词 | 模块 | PDF |
|--------|------|-----|
| I2C | I2C | `I2C/Rockchip_Developer_Guide_I2C_CN.pdf` |
| SPI、SPI Flash | SPI | `SPI/Rockchip_Developer_Guide_Linux_SPI_CN.pdf` |
| UART、串口、调试串口 | UART | `UART/Rockchip_Developer_Guide_UART_CN.pdf`<br>FAQ: `UART/Rockchip_Developer_Guide_UART_FAQ_CN.pdf` |
| CAN | CAN | `CAN/Rockchip_Developer_Guide_Can_CN.pdf` |
| CAN FD | CAN | `CAN/Rockchip_Developer_Guide_CAN_FD_CN.pdf` |
| PWM | PWM | `PWM/Rockchip_Developer_Guide_Linux_PWM_CN.pdf` |
| SARADC、ADC | SARADC | `SARADC/Rockchip_Developer_Guide_Linux_SARADC_CN.pdf` |
| FlexBus(可配置串行总线) | FLEXBUS | `FLEXBUS/Rockchip_Developer_Guide_Linux_FLEXBUS_CN.pdf` + ADC/DAC / FSPI 两份模式说明 |
| DSMC(双倍率串行存储控制器) | DSMC | `DSMC/Rockchip_Developer_Guide_DSMC_CN.pdf`, `SLAVE_DSMC` |

### GPIO / Pinctrl / 时钟 / 电源域

| 关键词 | 模块 | PDF |
|--------|------|-----|
| pinctrl、iomux、pinctrl-names | PINCTRL | `PINCTRL/Rockchip_Developer_Guide_Linux_Pinctrl_CN.pdf` |
| GPIO sysfs / libgpiod | GPIO | `GPIO/Rockchip_Developer_Guide_GPIO_CN.pdf` |
| Clock 框架、时钟树 | CLK | `CLK/Rockchip_Developer_Guide_Clock_CN.pdf` |
| GPIO 输出时钟 | CLK | `CLK/Rockchip_Developer_Guide_Gpio_Output_Clocks_CN.pdf` |
| PLL 展频 | CLK | `CLK/Rockchip_Developer_Guide_Pll_Ssmod_Clock_CN.pdf` |
| IO 电源域、io-domain | IO-DOMAIN | `IO-DOMAIN/Rockchip_Developer_Guide_Linux_IO_DOMAIN_CN.pdf` |

### 存储

| 关键词 | 模块 | PDF |
|--------|------|-----|
| eMMC、SDMMC、SDIO | MMC | `MMC/Rockchip_Developer_Guide_SDMMC_SDIO_eMMC_CN.pdf` |
| SD Boot 启动 | MMC | `MMC/Rockchip_Developer_Guide_SD_Boot_CN.pdf` |
| 存储分区、烧录 | NVM | `NVM/Rockchip_Application_Notes_Storage_CN.pdf` |
| 双存储、A/B 系统 | NVM | `NVM/Rockchip_Developer_Guide_Dual_Storage_CN.pdf` |
| Flash 开源方案(MTD/UBIFS) | NVM | `NVM/Rockchip_Developer_Guide_Linux_Flash_Open_Source_Solution_CN.pdf` |
| 掉电保护 | NVM | `NVM/Rockchip_Application_Notes_Power_Loss_Protection_CN.pdf` |
| SATA | NVM | `NVM/Rockchip_Developer_Guide_SATA_CN.pdf` |
| UFS | NVM | `NVM/Rockchip_Developer_Guide_UFS_CN.pdf` |
| Vendor Storage(隐藏分区) | NVM | `NVM/RK_Vendor_Storage_Application_Note.pdf` |
| 存储 FAQ | NVM | `NVM/Rockchip_Developer_FAQ_Storage_CN.pdf` |
| DDR 开发指南 | DDR | `DDR/Rockchip_Developer_Guide_DDR_CN.pdf` |

### 内存管理

| 关键词 | 路径(`cn/Common/MEMORY/`) |
|--------|----------------|
| CMA | `Rockchip_Developer_Guide_Linux_CMA_CN.pdf` |
| dma-buf | `Rockchip_Developer_Guide_Linux_DMABUF_CN.pdf` |
| meminfo 解读 | `Rockchip_Developer_Guide_Linux_Meminfo_CN.pdf` |
| 内存分配器 | `Rockchip_Developer_Guide_Linux_Memory_Allocator_CN.pdf` |

### 显示子系统(DISPLAY)

路径前缀 `cn/Common/DISPLAY/`。

| 关键词 | 路径 |
|--------|------|
| DRM 显示驱动 | `DRM/Rockchip_Developer_Guide_DRM_Display_Driver_CN.pdf` |
| DRM Direct Show | `DRM/Rockchip_Developer_Guide_DRM_Direct_Show_CN.pdf` |
| HDMI | `HDMI/Rockchip_Developer_Guide_HDMI_CN.pdf`<br>CEC: `HDMI/Rockchip_Developer_Guide_HDMI-CEC_CN.pdf` |
| DisplayPort(DP) | `DP/Rockchip_Developer_Guide_DisplayPort_CN.pdf` |
| eDP | `eDP/Rockchip_Developer_Guide_eDP_CN.pdf` |
| MIPI DSI(屏) | `MIPI/Rockchip_Developer_Guide_MIPI_DSI2_CN.pdf`<br>面板适配: `MIPI/Rockchip_Developer_Guide_MIPI_DSI_Compatible_With_Different_Panels_CN.pdf` |
| LVDS | `LVDS/Rockchip_Developer_Guide_LVDS_CN.pdf` |
| RGB(MCU 屏) | `RGB/Rockchip_Developer_Guide_RGB_MCU_CN.pdf` |
| BT656/BT1120 输入 | `BT656-BT1120/Rockchip_Developer_Guide_BT656_TX_AND_BT1120_TX_CN.pdf` |
| HDCP | `HDCP/Rockchip_RK3588_Developer_Guide_HDCP_CN.pdf` |
| Vsync 调整 | `Vsync/Rockchip_RK3588_Developer_Guide_Vsync_Adjust_CN.pdf` |
| RK628 桥接(HDMI-IN/CVBS-IN/RGB→HDMI 等) | `RK628/Rockchip_Developer_Guide_RK628_For_All_Porting_CN.pdf` |

### HDMI-IN

| 关键词 | 路径(`cn/Common/HDMI-IN/`) |
|--------|----------------|
| HDMI-IN 驱动 | `Rockchip_Developer_Guide_HDMI_RX_CN.pdf` |
| HDMI-IN + CameraHal3 应用 | `Rockchip_Developer_Guide_HDMI_IN_Based_On_CameraHal3_CN.pdf` |

### 图像处理 / ISP / 编解码 / 2D 加速

| 关键词 | 模块 | PDF |
|--------|------|-----|
| MPP 视频编解码中间件 | MPP | `MPP/Rockchip_Developer_Guide_MPP_CN.pdf` |
| RGA 2D 加速(缩放/格式转换/旋转) | RGA | `RGA/Rockchip_Developer_Guide_RGA_CN.pdf`<br>FAQ: `RGA/Rockchip_FAQ_RGA_CN.pdf` |
| ISP(图像信号处理) | ISP | `ISP/The-Latest-Camera-Documents-Link.txt`(指向 redmine,本地无 PDF) |

### USB

路径前缀 `cn/Common/USB/`。

| 关键词 | 路径 |
|--------|------|
| USB 通用 | `Rockchip_Developer_Guide_USB_CN.pdf` |
| RK356x USB 平台特性 | `Rockchip_RK356x_Developer_Guide_USB_CN.pdf` |
| USB Gadget UAC(音频) | `Rockchip_Developer_Guide_USB_Gadget_UAC_CN.pdf` |
| USB FFS 测试 | `Rockchip_Developer_Guide_USB_FFS_Test_Demo_CN.pdf` |
| USB 初始化日志分析 | `Rockchip_Developer_Guide_Linux_USB_Initialization_Log_Analysis_CN.pdf` |
| USB 性能分析 | `Rockchip_Developer_Guide_Linux_USB_Performance_Analysis_CN.pdf` |
| USB2 合规性测试 | `Rockchip_Developer_Guide_USB2_Compliance_Test_CN.pdf` |
| USB 信号质量(SQ)测试 | `Rockchip_Developer_Guide_USB_SQ_Test_CN.pdf` + 工具说明 |
| USB Gadget UVC 故障排查 | `Rockchip_Trouble_Shooting_Linux4.19_USB_Gadget_UVC_CN.pdf` |
| USB Host UVC 故障排查 | `Rockchip_Trouble_Shooting_Linux_USB_Host_UVC_CN.pdf` |

### 网络

| 关键词 | 模块 | PDF |
|--------|------|-----|
| GMAC 以太网 | GMAC | `GMAC/Rockchip_Developer_Guide_Linux_GMAC_CN.pdf` |
| GMAC 模式配置(RGMII/RMII) | GMAC | `GMAC/Rockchip_Developer_Guide_Linux_GMAC_Mode_Configuration_CN.pdf` |
| RGMII delayline 调试 | GMAC | `GMAC/Rockchip_Developer_Guide_Linux_GMAC_RGMII_Delayline_CN.pdf` |
| MAC-to-MAC 直连 | GMAC | `GMAC/Rockchip_Developer_Guide_Linux_MAC_TO_MAC_CN.pdf` |
| GMAC + DPDK | GMAC | `GMAC/Rockchip_Developer_Guide_Linux_GMAC_DPDK_CN.pdf` |

### PCIe / IOMMU

| 关键词 | 模块 | PDF |
|--------|------|-----|
| PCIe RC(Root Complex) | PCIe | `PCIe/Rockchip_Developer_Guide_PCIe_CN.pdf` |
| PCIe EP(Endpoint) | PCIe | `PCIe/Rockchip_Developer_Guide_PCIe_EP_CN.pdf` |
| PCIe 性能调优 | PCIe | `PCIe/Rockchip_Developer_Guide_PCIe_Performance_CN.pdf` |
| PCIe 虚拟化 | PCIe | `PCIe/Rockchip_PCIe_Virtualization_Developer_Guide_CN.pdf` |
| IOMMU(32 位地址转换) | IOMMU | `IOMMU/Rockchip_Developer_Guide_Linux_IOMMU_CN.pdf` |

### 电源 / 频率 / 温度

| 关键词 | 模块 | PDF |
|--------|------|-----|
| CPU 调频 cpufreq | DVFS | `DVFS/Rockchip_Developer_Guide_CPUFreq_CN.pdf` |
| GPU/DDR 调频 devfreq | DVFS | `DVFS/Rockchip_Developer_Guide_Devfreq_CN.pdf` |
| Thermal 热管理 | THERMAL | `THERMAL/Rockchip_Developer_Guide_Thermal_CN.pdf` |
| 功耗分析 | POWER | `POWER/Rockchip_Developer_Guide_Power_Analysis_CN.pdf` |
| 实时性能 / RT 测试 | PERF | `PERF/Rockchip_Developer_Guide_Linux_RealTime_Performance_Test_Report_CN.pdf` |
| Perf 工具入门 | PERF | `PERF/Rockchip_Quick_Start_Linux_Performance_Analyse_CN.pdf` |

### PMIC

路径前缀 `cn/Common/PMIC/`。芯片型号 → PDF 一一对应:

| 芯片 | PDF |
|------|-----|
| RK801 | `Rockchip_RK801_Developer_Guide_CN.pdf` |
| RK805 | `Rockchip_RK805_Developer_Guide_CN.pdf` |
| RK806 | `Rockchip_RK806_Developer_Guide_CN.pdf` |
| RK808 | `Rockchip_RK808_Developer_Guide_CN.pdf` |
| RK809 | `Rockchip_RK809_Developer_Guide_CN.pdf` |
| RK816/RK818 + Fuel Gauge | `Rockchip_RK816/818_*_CN.pdf` 系列 |
| RK817 | `Rockchip_RK817_Developer_Guide_CN.pdf` |

### 系统启动 / 安全 / 休眠

| 关键词 | 模块 | PDF |
|--------|------|-----|
| U-Boot 新版开发 | UBOOT | `UBOOT/Rockchip_Developer_Guide_UBoot_Nextdev_CN.pdf` |
| Trust / ATF / TEE 基础 | TRUST | `TRUST/Rockchip_Developer_Guide_Trust_CN.pdf` |
| RK356x 休眠唤醒 | TRUST | `TRUST/Rockchip_RK356X_Developer_Guide_System_Suspend_CN.pdf` |
| RK3588 休眠 | TRUST | `TRUST/Rockchip_RK3588_Developer_Guide_System_Suspend_CN.pdf` |
| RK3576 休眠 | TRUST | `TRUST/Rockchip_RK3576_Developer_Guide_System_Suspend_CN.pdf` |
| TEE SDK | SECURITY | `SECURITY/Rockchip_Developer_Guide_TEE_SDK_CN.pdf` |
| OTP 烧录 | SECURITY | `SECURITY/Rockchip_Developer_Guide_OTP_CN.pdf` |
| 防抄板 | SECURITY | `SECURITY/Rockchip_Developer_Guide_Anti_Copy_Board_CN.pdf` |
| Crypto + HWRNG | CRYPTO | `CRYPTO/Rockchip_Developer_Guide_Crypto_HWRNG_CN.pdf` |

### 工具与杂项

| 关键词 | 模块 | PDF |
|--------|------|-----|
| 分区结构 | TOOL | `TOOL/Rockchip_Introduction_Partition_CN.pdf` |
| 量产烧录工具 | TOOL | `TOOL/Rockchip_User_Guide_Production_For_Firmware_Download_CN.pdf` |
| Watchdog | WATCHDOG | `WATCHDOG/Rockchip_Developer_Guide_Linux_WDT_CN.pdf` |
| AMP(多核异构) | AMP | `AMP/Rockchip_Developer_Guide_AMP_CN.pdf` |
| Audio(通用) | AUDIO | `AUDIO/Rockchip_Developer_Guide_Audio_CN.pdf` |
| RV 系列 ACodec | AUDIO | `AUDIO/Rockchip_Developer_Guide_Linux_RV_Series_ACodec_CN.pdf` |
| MCU(M0/M4 协处理器) | MCU | `MCU/Rockchip_RK3399_Developer_Guide_MCU_CN.pdf` |
| GNU MCU Eclipse + OpenOCD | DEBUG | `DEBUG/Rockchip_Developer_Guide_GNU_MCU_Eclipse_OpenOCD_CN.pdf` |


## 三、Linux 子系统文档

路径前缀 `cn/Linux/`。这部分讲"芯片之上、应用之下"的子系统/中间件。

### 系统集成

| 关键词 | 路径 |
|--------|------|
| Buildroot 构建 | `System/Rockchip_Developer_Guide_Buildroot_CN.pdf` |
| Debian 构建/移植 | `System/Rockchip_Developer_Guide_Debian_CN.pdf` |
| 第三方系统适配 | `System/Rockchip_Developer_Guide_Third_Party_System_Adaptation_CN.pdf` |
| UEFI 启动 | `Uefi/Rockchip_Developer_Guide_UEFI_CN.pdf` |
| OTA Recovery | `Recovery/Rockchip_Developer_Guide_Linux_Recovery_CN.pdf` |
| OTA 升级流程 | `Recovery/Rockchip_Developer_Guide_Linux_Upgrade_CN.pdf` |
| Linux Secure Boot | `Security/Rockchip_Developer_Guide_Linux_Secure_Boot_CN.pdf` |

### 图形栈

| 关键词 | 路径 |
|--------|------|
| Linux 图形栈总览(EGL/Mesa/GBM/Mali) | `Graphics/Rockchip_Developer_Guide_Linux_Graphics_CN.pdf` |
| Buildroot Weston(Wayland) | `Graphics/Rockchip_Developer_Guide_Buildroot_Weston_CN.pdf` |
| LVGL | `Graphics/Rockchip_Developer_Guide_Linux_LVGL_CN.pdf` |

### 多媒体

| 关键词 | 路径 |
|--------|------|
| GStreamer 集成 | `Multimedia/Rockchip_User_Guide_Linux_Gstreamer_CN.pdf` |
| Rockit(RKMedia 后继) | `Multimedia/Rockchip_User_Guide_Linux_Rockit_CN.pdf` + Runtime |
| RKADK(应用 SDK) | `Multimedia/Rockchip_Developer_Guide_Linux_Rkadk_CN.pdf` |

### Camera

| 关键词 | 路径 |
|--------|------|
| Camera 驱动开发(MIPI/DVP) | `Camera/Rockchip_Developer_Guide_Camera_Driver_CN.pdf` |
| Linux 5.10 Camera 故障排查 | `Camera/Rockchip_Trouble_Shooting_Linux5.10_Camera_CN.pdf` |
| Linux 4.4 Camera 故障排查 | `Camera/Rockchip_Trouble_Shooting_Linux4.4_Camera_CN.pdf` |
| RMSL(远程 sensor 同步) | `Camera/Rockchip_Developer_Guide_Linux_RMSL_CN.pdf` |

### 音频应用

| 关键词 | 路径 |
|--------|------|
| PulseAudio | `Audio/Rockchip_Developer_Guide_PulseAudio_CN.pdf` |
| 自研麦克风算法 | `Audio/Algorithms/` |

### WiFi/BT

| 关键词 | 路径 |
|--------|------|
| WiFi/BT 总指南 | `Wifibt/Rockchip_Developer_Guide_Linux_WIFI_BT_CN.pdf` |
| 最新版本入口 | `Wifibt/Latest-Release-Wifibt-Link.txt` |

### 容器 / DPDK / 应用

| 关键词 | 路径 |
|--------|------|
| Buildroot Docker | `Docker/Rockchip_User_Guide_SDK_Docker_CN.pdf`(若有) |
| Debian Docker | `Docker/Rockchip_Developer_Guide_Debian_Docker_CN.pdf` |
| Docker 部署 | `Docker/Rockchip_Developer_Guide_Linux_Docker_Deploy_CN.pdf` |
| DPDK | `DPDK/Rockchip_Developer_Guide_Linux_DPDK_CN.pdf` |
| RKIPC 应用 | `ApplicationNote/Rockchip_Developer_Guide_Linux_RKIPC_CN.pdf` |
| ROS / ROS2 | `ApplicationNote/Rockchip_Instruction_Linux_ROS_CN.pdf` / `ROS2_CN.pdf` |
| EtherCAT(IgH) | `ApplicationNote/Rockchip_Use_Guide_Linux_EtherCAT_IgH_CN.pdf` |
| USB Gadget 快速入门 | `ApplicationNote/Rockchip_Quick_Start_Linux_USB_Gadget_CN.pdf` |

### Profile / 测试

| 关键词 | 路径 |
|--------|------|
| PCBA 测试 | `Profile/Rockchip_Developer_Guide_Linux_PCBA_CN.pdf` |
| Benchmark KPI | `Profile/Rockchip_Introduction_Linux_Benchmark_KPI_CN.pdf` |
| 软件测试规范 | `Profile/Rockchip_User_Guide_Linux_Software_Test_CN.pdf` |
| PLT 优化 | `Profile/Rockchip_Introduction_Linux_PLT_CN.pdf` |


## 四、RK3566/RK3568 平台专属

路径前缀 `cn/RK3566_RK3568/`。

| 类别 | 路径 |
|------|------|
| SDK 发布说明 | `Rockchip_RK3566_RK3568_Linux6.1_SDK_Release_V1.3.0_20251220_CN.pdf` |
| SDK 笔记 | `RK3566_RK3568_Linux6.1_SDK_Note.md` |
| 平台软件开发指南 | `Rockchip_Developer_Guide_Linux_Software_CN.pdf` |
| IO 电源域配置(平台专属) | `Rockchip_RK356X_Introduction_IO_Power_Domains_Configuration_CN.pdf` |
| RK3566 Datasheet | `Datasheet/Rockchip_RK3566_Datasheet_V1.4-20240621.pdf` |
| RK3568 Datasheet | `Datasheet/Rockchip_RK3568_Datasheet_V2.1-20240621.pdf` |
| RK3568B2 Datasheet | `Datasheet/Rockchip_RK3568B2_Datasheet-V2.1-20240621.pdf` |
| RK3568J(工业级) Datasheet | `Datasheet/Rockchip_RK3568J_Datasheet_V2.1-20240621.pdf` |
| RK3566 EVB2 板卡用户指南 | `Hardware/Rockchip_RK3566_EVB2_User_Guide_V1.1_CN.pdf` |
| RK3566 硬件设计指南 | `Hardware/Rockchip_RK3566_Hardware_Design_Guide_V1.1_20220206_CN.pdf` |
| RK3568 EVB 板卡用户指南 | `Hardware/Rockchip_RK3568_EVB_User_Guide_V1.2_CN.pdf` |
| RK3568 硬件设计指南 | `Hardware/Rockchip_RK3568_Hardware_Design_Guide_V1.2_20211107_CN.pdf` |
| 快速入门(Linux) | `Quick-start/Rockchip_RK356X_Quick_Start_Linux_CN.pdf` |
| 快速入门(RK3568J 工业) | `Quick-start/Rockchip_RK3568J_Industry_Application_Quick_Start_Linux_CN.pdf` |


## 五、NPU / RKNN / RKLLM

路径前缀 `cn/Common/NPU/rknpu2/`。

| 关键词 | 路径 |
|--------|------|
| RKNN 快速开始 | `01_Rockchip_RKNPU_Quick_Start_RKNN_SDK_V2.3.2_CN.pdf` |
| RKNN 用户指南 | `02_Rockchip_RKNPU_User_Guide_RKNN_SDK_V2.3.2_CN.pdf` |
| RKNN Toolkit2 API(PC 端模型转换) | `03_Rockchip_RKNPU_API_Reference_RKNN_Toolkit2_V2.3.2_CN.pdf` |
| RKNNRT API(板端推理 C/C++) | `04_Rockchip_RKNPU_API_Reference_RKNNRT_V2.3.2_CN.pdf` |
| 编译器支持算子列表 | `05_RKNN_Compiler_Support_Operator_List_V2.3.2.pdf` |
| RKLLM(大语言模型推理框架) | `Rockchip_RKLLM_SDK_CN_1.2.3.pdf` |
| Toolkit2 vs Toolkit1 差异 | `RKNNToolKit2_API_Difference_With_Toolkit1-2.3.2.md` |
| Toolkit2 OP 支持表 | `RKNNToolKit2_OP_Support-2.3.2.md` |
| WSL 中使用 RKNN_ToolKit2 | `WSL中使用RKNN_ToolKit2.md` |
| rknn_server 代理 | `rknn_server_proxy.md` |

NPU 内核驱动在 `kernel-6.1/drivers/rknpu/`,本文档库不展开。


## 六、AVL 选型与硬件

路径前缀 `cn/Common/AVL/`。AVL = Approved Vendor List(已验证厂商)。

| 类别 | 路径 |
|------|------|
| DDR 通过列表 | `Rockchip_DDR_Approved_Vendor_List_ver3.01.pdf` |
| DDR 参考列表 | `Rockchip_DDR_Reference_Vendor_List_ver3.01.pdf` |
| eMMC 通过列表 | `Rockchip_EMMC_Approved_Vendor_List_Ver1.93.pdf` |
| Flash(NOR/NAND/SLC) 通过列表 | `Rockchip_Flash_Approved_Vendor_List_Ver2.03.pdf` |
| UFS 通过列表 | `Rockchip_UFS_Approved_Vendor_List_Ver1.04.pdf` |
| WiFi/BT 支持列表 | `Rockchip_Support_List_Linux_WiFi_BT_Ver2.0_20250619.pdf` |
| Type-C / PD 支持列表 | `Rockchip_TYPE-C_and_PD_Support_List.xlsx` |
| 最新 AVL 链接(在线) | `Latest-Release-AVL-Link.txt` |

支持等级:`√` 量产通过 / `T/A` 测试通过 / `S/A` 样品风险 / `D/A` 仅 datasheet / `N/A` 不适用。只选 `√`、`T/A`。


## 七、Others

路径前缀 `cn/Others/`。

| 关键词 | 路径 |
|--------|------|
| Repo Mirror 服务器搭建 | `Rockchip_Developer_Guide_Repo_Mirror_Server_Deploy_CN.pdf` |
| Bug 系统使用 | `Rockchip_User_Guide_Bug_System_CN.pdf` |
| SDK 申请与同步指南 | `Rockchip_User_Guide_SDK_Application_And_Synchronization_CN.pdf` |


## 八、RT-Thread (RTOS)

路径前缀 `cn/Common/RTT/`。RT-Thread 是 Rockchip 在裸核/AMP 场景下的 RTOS 选项,与 Linux 侧文档独立。

| 关键词 | 路径 |
|--------|------|
| CAN / CAN FD | `Rockchip_Developer_Guide_RT-Thread_CAN_CANFD_CN.pdf` |
| Display | `Rockchip_Developer_Guide_RT-Thread_Display_CN.pdf` |
| I2C | `Rockchip_Developer_Guide_RT-Thread_I2C_CN.pdf` |
| SPI | `Rockchip_Developer_Guide_RT-Thread_SPI_CN.pdf` |
| SPI Flash | `Rockchip_Developer_Guide_RT-Thread_SPIFLASH_CN.pdf` |
| SPI to APB 桥 | `Rockchip_Developer_Guide_RT-Thread_SPI2APB_CN.pdf` |
| UART | `Rockchip_Developer_Guide_RT-Thread_UART.pdf` |
| USB | `Rockchip_Developer_Guide_RT-Thread_USB_CN.pdf` |
| Ymodem | `Rockchip_Developer_Guide_RT-Thread_Ymodem_CN.pdf` |

外侧还有独立的 `rtt-docs/` 顶级目录,详见 `sdk/tspi-rk3566-sdk/docs/rtt-docs/`。


## 附:RTT 子库快速入口

```
sdk/tspi-rk3566-sdk/docs/rtt-docs/      # RT-Thread 完整文档(若需要)
sdk/tspi-rk3566-sdk/docs/sbom/          # 软件物料清单 + CVE 跟踪
sdk/tspi-rk3566-sdk/docs/Patches/       # 官方 patch 集
```
