---
title: pinctrl 与 IOMUXC (引脚复用与电气配置)
tags: [DTS, pinctrl, IOMUXC, IMX6ULL]
desc: 讲解 IMX6ULL iomuxc 节点结构、MX6UL_PAD_* 宏的五元组展开,以及 pad 配置码(如 0x1b0b0)的位域含义。
update: 2026-04-10

---


# pinctrl 与 IOMUXC

> [!note]
> **Ref:**
> - `sdk/.../Linux-4.9.88/arch/arm/boot/dts/imx6ull-pinfunc.h`
> - `sdk/.../Linux-4.9.88/Documentation/devicetree/bindings/pinctrl/fsl,imx-pinctrl.txt`
> - `sdk/.../Linux-4.9.88/arch/arm/boot/dts/100ask_imx6ull-14x14.dts` (使用范例)

i.MX 系列采用集中式 IOMUXC(IO Multiplexer Controller)管理引脚复用,DTS 中通过 `pinctrl-single` 风格的 group 节点描述。

## 1. iomuxc 节点结构

```dts
&iomuxc {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_hog_1>;

    imx6ul-evk {
        pinctrl_uart1: uart1grp {
            fsl,pins = <
                MX6UL_PAD_UART1_TX_DATA__UART1_DCE_TX 0x1b0b1
                MX6UL_PAD_UART1_RX_DATA__UART1_DCE_RX 0x1b0b1
            >;
        };
        /* 其余 group ... */
    };
};
```

- `pinctrl_xxx: xxxgrp` 是 **引脚组(group)**,被外设节点用 `pinctrl-0 = <&pinctrl_xxx>` 引用。
- `fsl,pins` 每行 = **一对**:`MX6UL_PAD_*` 宏 + pad 配置码。
- IMX6ULL 的 SNVS 域 pad(TAMPER 系列)必须放在 `&iomuxc_snvs { ... }` 而非 `&iomuxc`,否则寄存器地址错位。

## 2. `MX6UL_PAD_*` 宏的展开

宏定义在 `imx6ull-pinfunc.h`,例如:

```c
#define MX6UL_PAD_UART1_TX_DATA__UART1_DCE_TX  0x0084 0x0310 0x0000 0 0
/*                                              ^      ^      ^      ^ ^
                                                |      |      |      | └── input_val
                                                |      |      |      └──── mux_mode
                                                |      |      └─────────── input_reg (DAISY)
                                                |      └────────────────── conf_reg  (PAD_CTL)
                                                └───────────────────────── mux_reg   (MUX_CTL) */
```

五元组语义(`fsl,imx-pinctrl` binding):

| 字段 | 作用 |
|---|---|
| `mux_reg` | IOMUXC_SW_MUX_CTL_PAD_xxx 偏移,选择 ALT 功能 |
| `conf_reg` | IOMUXC_SW_PAD_CTL_PAD_xxx 偏移,设置电气特性 |
| `input_reg` | IOMUXC_xxx_SELECT_INPUT 偏移(daisy chain),用于多 pad 可路由到同一外设输入 |
| `mux_mode` | 写入 mux_reg 的 ALT 编号(0-8) |
| `input_val` | 写入 input_reg 的值 |

最后 dts 中那一对 `<MX6UL_PAD_xxx 0x1b0b1>` 实际进入内核驱动时,**前 5 个由宏展开**,**第 6 个**就是用户写在 `fsl,pins` 中的 **pad config**(`conf_val`),写入 `conf_reg`。

## 3. pad 配置码(conf_val)位域

i.MX6ULL `IOMUXC_SW_PAD_CTL_PAD_xxx` 寄存器,常用位域:

| Bit | 名称 | 含义 |
|---|---|---|
| 16 | HYS | 1 = 启用施密特触发(输入抗噪) |
| 15:14 | PUS | 上下拉电阻值: `00`=100k↓, `01`=47k↑, `10`=100k↑, `11`=22k↑ |
| 13 | PUE | 0 = keeper, 1 = pull |
| 12 | PKE | 0 = 禁用 keep/pull, 1 = 启用 |
| 11 | ODE | 1 = open drain |
| 7:6 | SPEED | `00`=低速, `01/10`=中速, `11`=最大速度 |
| 5:3 | DSE | 驱动能力: `000`=禁用, `001`=260Ω, `010`=130Ω, `011`=87Ω, `100`=65Ω, `101`=52Ω, `110`=43Ω, `111`=37Ω |
| 0 | SRE | 0 = slow slew, 1 = fast slew |

### 解码示例 `0x1b0b0`

```
0x1b0b0 = 0001 1011 0000 1011 0000
          ───┬──── ─┬── ──┬─ ──┬─
             │      │     │    └─ Bit 5:0 = 110000 → SPEED=00, DSE=110(43Ω), SRE=0
             │      │     └────── Bit 11:8 = 1011 → ODE=1? 实则按位:Bit11=0(no OD), Bit7:6=00 低速
             │      └──────────── Bit 15:12 = 1011 → PKE=1, PUE=0(keeper), PUS=10(100k↑) 注意此处展开按真实位
             └─────────────────── Bit 16 = 1 → HYS 启用
```

更直观的解读:`0x1b0b0` 是 i.MX 阵营约定俗成的 **以太网/通用数字信号 GPIO 默认值**:启用施密特、100k 上拉(keeper 模式)、43Ω 驱动、慢速 slew、无 open-drain。`0x1b0b1` 把 SRE 改为 1(fast slew),常见于 UART/I2C。

> 实际位映射以 `i.MX6ULL Reference Manual` 中 `IOMUXC_SW_PAD_CTL_PAD_*` 寄存器章节为准,不同信号 pad 寄存器位定义可能略有差异(例如 LCD pad 多一个 DSE 等级)。

## 4. 外设节点引用 pinctrl

```dts
&uart1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_uart1>;
    status = "okay";
};
```

- `pinctrl-names` 列出所有状态名(`default`、`sleep`...)
- `pinctrl-N` 对应第 N 个名字所引用的 group phandle 列表
- 驱动调用 `pinctrl_get_select_default()`(或 `pm_runtime` 自动切换)即写入对应寄存器

## 5. 调试

- 编译:`make dtbs` 后 `dtc -I dtb -O dts arch/arm/boot/dts/100ask_imx6ull-14x14.dtb` 反汇编验证 `fsl,pins` 数值。
- 运行时:`/sys/kernel/debug/pinctrl/20e0000.iomuxc/{pinmux-pins,pinconf-groups}` 查看实际复用与配置。
- 常见错误:SNVS pad 写在 `&iomuxc` 下 → 内核启动不报错但功能失效;`pinctrl-0` 漏引用 → 外设初始化后引脚仍是 GPIO 默认状态。
