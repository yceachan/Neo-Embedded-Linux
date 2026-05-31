# Distilled Knowledge Index

> 本文件登记所有 `references/distilled/*.md` 蒸馏产物。**每条一行**,格式:
>
> ```
> - [<title>](<filename>.md) — <模块>/<芯片> — <一句话描述, ≤120 字>
> ```
>
> 新建蒸馏文件后必须追加一行到这里。重复主题先更新已存在条目而非新建。

## 索引

- [RK356x USB Gadget UAC2 快速上手](usb-gadget-uac-rk356x.md) — USB/Gadget/UAC RK356x — OTG 口当 USB 音频设备(麦+扬声器):SDK `S50usbdevice + .usb_config` 现成方案、configfs 节点、Windows 兼容性、OTG mode 切换
- [RK356x GMAC RGMII delayline 调试](gmac-rgmii-delayline.md) — GMAC/RK3566+RK3568 — RGMII tx_delay/rx_delay 扫描流程、DTS 片段、ping/iperf 与示波器验证
- [RK3566 M.2 B-Key 5G CPE 模组接入](m2bkey-5g-modem-rk3566.md) — USB3/PCIe/MHI RK3566 — M.2 B-Key 5G 模组总线选型(USB3 vs PCIe MHI)、combphy 互斥处理、DTS 模板、ModemManager/quectel-CM 拨号、GMAC 下行 NAT 路由,以及"RK3566 LAN 上不去 2.5G"的硬件死结;含 Fibocom **FM650**(SDX62) 专章:PCIe MHI 首选、上电时序、SKU 频段坑、降速到 Gen2 ×1 是正常现象
