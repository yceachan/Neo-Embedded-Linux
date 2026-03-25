# i.MX 6ULL 芯片架构特性笔记

基于数据手册 `IMX6ULLIEC` 和 `ARM GIC Architecture Specification` 的整理。

## 1. 处理器核心 (Arm Cortex-A7 Core)
- **核心架构**: 单核 Arm Cortex-A7 MPCore，集成 TrustZone 支持。
- **主频**: 最高可达 900 MHz (部分型号 528 MHz 或 792 MHz)。
- **缓存**:
  - L1 指令缓存: 32 KB
  - L1 数据缓存: 32 KB
  - L2 统一缓存: 128 KB
- **协处理器**:
  - NEON Media Processing Engine (MPE): 支持 SIMD 媒体处理。
  - 浮点运算单元 (VFPv3): 支持双精度浮点运算。

## 2. 存储器支持 (Memory Interfaces)
- **外部存储器接口 (MMDC)**:
  - 16-bit LP-DDR2-800
  - 16-bit DDR3-800 / DDR3L-800
- **NAND Flash**: 支持 Raw NAND (SLC/MLC) 和 Managed NAND (eMMC 4.5/SD 3.0)。
- **NOR Flash**: 支持并行 NOR Flash 和 Quad SPI 串行 Flash。

## 3. 多媒体功能 (Multimedia)
- **显示 (Display)**:
  - **PXP (Pixel Processing Pipeline)**: 2D 图像加速器，支持缩放、旋转、色彩空间转换 (CSC) 和 Alpha 混合。
  - **LCDIF**: 并行 LCD 接口，最高支持 WXGA (1366x768 @ 60Hz)，支持 8/16/18/24-bit 接口。
  - **EPD (Electrophoretic Display)**: 部分型号支持电子墨水屏控制器。
- **摄像头 (Camera)**:
  - **CSI (CMOS Sensor Interface)**: 并行接口，支持 8/10/16/24-bit 输入，兼容 BT.656。
- **音频 (Audio)**:
  - 3x SAI/I2S (支持全双工，TDM)。
  - 1x ESAI (Enhanced Serial Audio Interface)。
  - S/PDIF (Tx/Rx)。

## 4. 连接性与接口 (Connectivity)
- **USB**: 2x USB 2.0 OTG，集成 HS PHY。
- **Ethernet**: 2x 10/100 Mbps 以太网控制器 (支持 IEEE 1588)。
- **CAN**: 2x FlexCAN 控制器 (支持 CAN 2.0B)。
- **UART**: 8x UART (最高 5.0 Mbps)。
- **SPI/I2C**: 4x eCSPI, 4x I2C。
- **SD/MMC**: 2x uSDHC (支持 SD 3.0/eMMC 4.5)。

## 5. 模拟外设 (Analog)
- **ADC**: 2x 12-bit ADC，共支持 10 个输入通道。
- **电源管理 (PMU)**: 集成 LDO 稳压器 (LDO_ARM, LDO_SOC, LDO_USB 等)，简化外部电源设计。
- **温度传感器**: 集成片上温度监测。

## 6. 安全特性 (Security)
- **TrustZone**: 硬件级安全隔离。
- **HAB (High Assurance Boot)**: 安全启动。
- **加解密引擎 (DCP)**: 支持 AES-128, SHA-1, SHA-256。
- **SNVS**: 安全非易失性存储，含安全 RTC 和防篡改检测。
- **SJC**: 系统 JTAG 控制器，支持安全调试。

## 7. 中断控制器 (Generic Interrupt Controller - GIC)
i.MX 6ULL 集成了基于 **GICv2** 架构的中断控制器：
- **支持数量**: 支持 128 个中断 ID (Interrupt IDs)。
- **架构组成**:
  - **Distributor (分发器)**: 负责中断的优先级仲裁、分发到具体的 CPU 接口。
  - **CPU Interface**: 连接处理器核心，负责将最高优先级中断发送给 CPU，处理中断确认 (ACK) 和结束 (EOI)。
- **中断类型**:
  - **SGI (Software Generated Interrupts)**: ID 0-15，用于核间通信（在单核上主要用于自身）。
  - **PPI (Private Peripheral Interrupts)**: ID 16-31，私有外设中断（如本地定时器）。
  - **SPI (Shared Peripheral Interrupts)**: ID 32-127，共享外设中断（来自 UART, DMA 等外设）。
- **安全扩展 (Security Extensions)**:
  - 支持将中断分组为 **Group 0 (Secure)** 和 **Group 1 (Non-secure)**。
  - Group 0 通常作为 FIQ 处理，Group 1 作为 IRQ 处理（在开启 TrustZone 的系统中）。
- **优先级**: 支持 8-bit 优先级配置（实现位数取决于具体设计，通常为高 4-5 位）。
