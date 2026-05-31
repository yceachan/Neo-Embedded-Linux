---
title: IMX6ULL 常见外设 DTS 绑定范例
tags: [DTS, Bindings, IMX6ULL, fec, i2c, sai, lcdif]
desc: 以 100ask EVB 为例,汇总 fec/uart/i2c/sai/lcdif 等常用外设的 DTS 绑定写法与必备属性。
update: 2026-04-10

---


# IMX6ULL 常见外设 DTS 绑定范例

> [!note]
> **Ref:**
> - `sdk/.../Linux-4.9.88/Documentation/devicetree/bindings/net/fsl-fec.txt`
> - `sdk/.../Linux-4.9.88/Documentation/devicetree/bindings/i2c/i2c-imx.txt`
> - `sdk/.../Linux-4.9.88/Documentation/devicetree/bindings/sound/fsl-sai.txt`
> - `sdk/.../Linux-4.9.88/Documentation/devicetree/bindings/display/mxsfb.txt`
> - `sdk/.../Linux-4.9.88/arch/arm/boot/dts/100ask_imx6ull-14x14.dts`

本文给出 100ask 板卡上**已经验证可工作**的外设 DTS 绑定模板,作为新写驱动或裁剪 DTS 时的参考。

## 1. UART (`fsl,imx6ul-uart`)

```dts
&uart1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_uart1>;
    status = "okay";
};
```

必备:`pinctrl-0`、`status`。其它如 `fsl,uart-has-rtscts`、`uart-has-rtscts`、`dmas` 按需添加。

## 2. I2C (`fsl,imx6ul-i2c`) 与子设备

```dts
&i2c2 {
    clock-frequency = <100000>;
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_i2c2>;
    status = "okay";

    sii902x: sii902x@39 {
        compatible = "SiI,sii902x";
        reg = <0x39>;
        interrupt-parent = <&gpio5>;
        interrupts = <2 IRQ_TYPE_LEVEL_LOW>;
        reset-gpios = <&gpio5 2 GPIO_ACTIVE_LOW>;
    };

    gt9xx: gt9xx@5d {
        compatible = "goodix,gt9xx";
        reg = <0x5d>;
        interrupt-parent = <&gpio1>;
        interrupts = <5 IRQ_TYPE_EDGE_FALLING>;
        irq-gpios = <&gpio1 5 0>;
        reset-gpios = <&gpio5 2 0>;
    };
};
```

要点:
- `clock-frequency` 设置 SCL 速率(标准 100 kHz / 快速 400 kHz)。
- 子设备 `reg = <addr>`(7-bit)直接作为 unit-address。
- 中断脚需明确 `interrupt-parent`(GPIO controller)+ trigger 类型。

## 3. FEC 以太网 (`fsl,imx6ul-fec`)

```dts
&fec1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_enet1>;
    phy-mode = "rmii";
    phy-handle = <&ethphy0>;
    phy-reset-gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;
    phy-reset-duration = <26>;
    status = "okay";
};

&fec2 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_enet2>;
    phy-mode = "rmii";
    phy-handle = <&ethphy1>;
    phy-reset-gpios = <&gpio5 6 GPIO_ACTIVE_LOW>;
    status = "okay";

    mdio {
        #address-cells = <1>;
        #size-cells = <0>;

        ethphy0: ethernet-phy@0 { reg = <0>; };
        ethphy1: ethernet-phy@1 { reg = <1>; };
    };
};
```

要点:
- IMX6ULL 双 FEC 共用一条 MDIO 总线,**只在一个 FEC 节点下定义** `mdio { ... }`,另一边用 `phy-handle` 引用。
- `phy-mode` 必须与硬件一致(rmii / rgmii)。
- `phy-reset-gpios` + `phy-reset-duration` 让 fec 驱动负责复位 PHY,无需驱动外加。

## 4. SAI 音频 (`fsl,imx6ul-sai`) + WM8960

```dts
&sai2 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_sai2>;
    assigned-clocks = <&clks IMX6UL_CLK_SAI2_SEL>,
                      <&clks IMX6UL_CLK_SAI2>;
    assigned-clock-parents = <&clks IMX6UL_CLK_PLL4_AUDIO_DIV>;
    assigned-clock-rates = <0>, <12288000>;
    status = "okay";
};

sound {
    compatible = "fsl,imx-audio-wm8960";
    model = "wm8960-audio";
    cpu-dai = <&sai2>;
    audio-codec = <&codec>;
    audio-routing =
        "Headphone Jack", "HP_L",
        "Headphone Jack", "HP_R",
        "Ext Spk",        "SPK_LP",
        "Ext Spk",        "SPK_LN",
        "LINPUT1",        "Mic Jack",
        "Mic Jack",       "MICB";
    /* hp-det = <3 0>; */
};
```

要点:
- `assigned-clocks` 锁定 PLL4 → SAI MCLK 频率(WM8960 需要 12.288 MHz)。
- `sound` 顶层节点是 ASoC machine driver,通过 `cpu-dai` + `audio-codec` 把 SAI 与 codec 拼接。

## 5. LCDIF 显示 (`fsl,imx6ul-lcdif`) + RGB 屏

```dts
&lcdif {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_lcdif_dat
                 &pinctrl_lcdif_ctrl>;
    display = <&display0>;
    status = "okay";

    display0: display@0 {
        bits-per-pixel = <16>;
        bus-width = <24>;

        display-timings {
            native-mode = <&timing0>;
            timing0: timing0 {
                clock-frequency = <51200000>;
                hactive = <1024>;
                vactive = <600>;
                hfront-porch = <160>;
                hback-porch  = <140>;
                hsync-len    = <20>;
                vfront-porch = <12>;
                vback-porch  = <20>;
                vsync-len    = <3>;
                hsync-active = <0>;
                vsync-active = <0>;
                de-active    = <1>;
                pixelclk-active = <0>;
            };
        };
    };
};
```

要点:
- `bits-per-pixel` 是帧缓冲深度,`bus-width` 是物理 RGB 总线宽度,二者可不同(如 BPP=16 / bus=24)。
- `display-timings` 必须严格匹配面板手册,porch/sync 错误会黑屏或撕裂。
- `pinctrl_lcdif_dat` 与 `pinctrl_lcdif_ctrl` 拆分两组,便于复用同一份 dat 给不同 ctrl 配置。

## 6. PWM 背光

```dts
&pwm1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_pwm1>;
    status = "okay";
};

backlight {
    compatible = "pwm-backlight";
    pwms = <&pwm1 0 1000>;     /* channel=0, period=1000 ns(1 MHz) */
    brightness-levels = <0 1 2 3 4 5 6 8 10>;
    default-brightness-level = <8>;
};
```

## 7. 通用模板要点

- **必带四件套**:`compatible`、`reg`、`interrupts`、`clocks`(从 .dtsi 继承,板卡层一般只需 `pinctrl-*` + `status`)。
- 板卡层应避免重复定义 .dtsi 已有属性,只通过 `&phandle` **追加或覆盖**。
- 引用外部资源(GPIO / clock / regulator / phy)一律用 phandle,保持单一真源。
