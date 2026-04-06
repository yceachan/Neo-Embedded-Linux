---
title: DTC与Sysfs设备树调试指南
tags: [DTS, dtc, u-boot, sysfs, debug]
desc: 详解设备树编译器dtc使用、U-Boot下的fdt调试命令，以及Linux用户空间基于sysfs和proc的设备树节点调试方法。
update: 2026-04-07
---


# DTC与Sysfs设备树调试指南

在嵌入式Linux开发中，设备树（Device Tree）的调试是一项重要的基本功。本文将介绍从编译阶段、U-Boot引导阶段到Linux用户空间的各种设备树调试方法。

## 1. DTC工具使用 (Device Tree Compiler)

`dtc`（Device Tree Compiler）是用于编译和反编译设备树文件的核心工具。它可以将人类可读的源文件（.dts/.dtsi）编译成内核或U-Boot可解析的二进制文件（.dtb），也可以进行逆向操作。

### 1.1 编译：DTS转DTB

在内核源码之外，或者排查特定错误时，我们有时需要手动调用 `dtc` 编译设备树：

```bash
dtc -I dts -O dtb -o target.dtb source.dts
```

- `-I dts`：输入格式为dts源文件
- `-O dtb`：输出格式为dtb二进制文件
- `-o target.dtb`：指定输出的文件名

> [!note]
> 在实际的Linux内核构建系统中（Kbuild），通常通过 `make dtbs` 或指定板子对应的dtb名称来编译，因为dts通常包含大量C风格的预处理宏（`#include <...>`），`dtc` 本身不支持直接解析宏，Kbuild会先通过C预处理器（cpp）处理后再交给 `dtc`。

### 1.2 反编译：DTB转DTS

在逆向工程或需要验证最终生成的dtb文件是否符合预期（例如确认多个dtsi包含覆盖后的结果）时，反编译非常有用：

```bash
dtc -I dtb -O dts -o result.dts source.dtb
```

- `-I dtb`：输入格式为二进制设备树blob
- `-O dts`：输出格式为可读的dts源码

反编译出的文件所有的注释和预定义宏都会丢失（宏会被展开为具体数值），但可以最真实地反映内核将要解析的数据树结构。

## 2. U-Boot环境下的FDT调试命令

U-Boot提供了强大的 `fdt` 命令族，用于在启动内核之前查看、修改内存中的设备树（Flattened Device Tree）。这对于临时验证引脚复用或节点属性非常方便，避免了频繁编译和烧录的麻烦。

### 2.1 设置DTB地址 (fdt addr)

在使用其他fdt命令之前，必须先将内存中的一段区域指定为当前操作的设备树地址。通常在通过tftp或fatload将dtb加载到内存后执行：

```bash
# 假设dtb被加载到了 0x83000000
=> fdt addr 0x83000000
```

### 2.2 查看节点与属性 (fdt print / fdt list)

- **打印整个设备树：**
  ```bash
  => fdt print
  ```

- **打印特定节点：**
  ```bash
  => fdt print /soc/leds
  ```

- **列出特定节点下的内容（较精简）：**
  ```bash
  => fdt list /soc
  ```

### 2.3 修改和操作设备树 (fdt set / fdt rm / fdt mknode)

如果需要临时修改某个属性（例如关闭某个设备、修改compatible以测试不同驱动），可以直接在U-Boot中修改：

- **设置/修改属性：**
  ```bash
  # 将 status 属性修改为 "okay"
  => fdt set /soc/leds status "okay"
  
  # 修改包含整数数组的属性（如reg），需要使用尖括号
  => fdt set /soc/i2c@21a0000 clock-frequency <400000>
  ```

- **删除属性或节点：**
  ```bash
  => fdt rm /soc/leds status
  ```

- **创建新节点：**
  ```bash
  => fdt mknode /soc new_device
  ```

> [!note]
> 修改设备树时，要注意当前内存中的dtb是否还有足够的空间（padding）来容纳新增加的字符串或节点。如果没有，可能需要先通过 `fdt resize` 命令扩展设备树大小。

## 3. Linux用户空间的设备树调试

一旦内核成功解析了dtb并启动完毕，内核会将设备树的层级结构以文件系统的形式导出到用户空间（sysfs）。开发者可以直接在Linux命令行中查看这些节点。

### 3.1 Sysfs与Proc目录结构

导出的设备树目录通常位于：
- `/sys/firmware/devicetree/base/`
- `/proc/device-tree/` (这是一个指向 `/sys/firmware/devicetree/base/` 的符号链接)

在这个目录下，每一个目录对应设备树中的一个节点（Node），每一个文件对应一个属性（Property）。

### 3.2 查看文本类型属性

对于字符串类型的属性（如 `compatible`, `status`），可以直接使用 `cat` 命令查看：

```bash
# 查看根节点的compatible属性
root@imx6ull:~# cat /proc/device-tree/compatible
fsl,imx6ull-14x14-evbfsl,imx6ull

# 提示：字符串常常没有换行符，输出可能与提示符粘连
```

### 3.3 查看二进制与数值属性 (hexdump / xxd)

对于数值型（`<u32>`）、数组或是二进制数据，直接使用 `cat` 会输出乱码。我们需要借助十六进制查看工具如 `hexdump`（通常随BusyBox提供）。

```bash
# 查看节点的 reg 属性（通常包含地址和长度）
root@imx6ull:~# hexdump -C /proc/device-tree/soc/aips-bus@2000000/gpio@209c000/reg
00000000  02 09 c0 00 00 00 40 00                           |......@.|
00000008

# 查看中断属性 interrupts
root@imx6ull:~# hexdump -C /proc/device-tree/soc/aips-bus@2000000/gpio@209c000/interrupts
00000000  00 00 00 00 00 00 00 42  00 00 00 04 00 00 00 00  |.......B........|
00000010  00 00 00 43 00 00 00 04                           |...C....|
00000018
```

通过将 `hexdump -C` 的输出与对应的绑定文档（Binding Doc）结合分析，可以有效地验证内核接收到的最终设备树属性值是否正确无误，这在排查驱动探针（probe）失败等问题时是极为关键的手段。