---
title: dtso 运行时加载的方法
tags: [DTS, Overlay, dtso, configfs, U-Boot]
desc: 整理 dtbo 在系统运行期 apply 的三种方法(configfs / U-Boot fdt apply / driver 主动 apply),含验证与常见坑。
update: 2026-04-10

---


# dtso 运行时加载的方法

> [!note]
> **Ref:**
> - `Documentation/devicetree/overlay-notes.txt`
> - `Documentation/devicetree/dynamic-resolution-notes.txt`
> - `drivers/of/overlay.c`、`drivers/of/configfs.c`
> - `cmd/fdt.c`(U-Boot `fdt apply`)

本节聚焦 **运行期加载** 的具体操作。dtso 与 dts 的语法/语义对比见 `Overlays/02-dts-vs-dtso-overwrite.md`。

## 0. 前置条件

- 基底 dtb **必须**用 `dtc -@` 编译,带 `__symbols__` 节点(主线 `make dtbs` 已默认开启 `-@`,4.x 之前的老树需手动加)。
- kernel:
  - `CONFIG_OF_OVERLAY=y` —— overlay 核心
  - `CONFIG_OF_CONFIGFS=y` —— configfs 接口(方法 A 必备)
  - `CONFIG_OF_DYNAMIC=y` —— 动态节点支持

## 1. 编译 `.dtso` → `.dtbo`

```dts
/dts-v1/;
/plugin/;                    /* 关键:让 dtc 生成 fixups 段 */

&i2c2 {
    eeprom@50 {
        compatible = "atmel,24c32";
        reg = <0x50>;
    };
};
```

```bash
dtc -@ -I dts -O dtb -o foo.dtbo foo.dtso
# 或在 kbuild:
#   dtbo-y += foo.dtbo
#   make dtbs   # 4.17+ 支持 .dtso 后缀;旧版用 .dts 后缀 + /plugin/
```

产物 `foo.dtbo` 中的 `__fixups__` / `__local_fixups__` / `__symbols__` 三段元数据,是运行期 phandle 重定位的依据。

## 2. 方法 A:`/lib/firmware/` + configfs(主线推荐)

```bash
# 一次性挂载 configfs
mount -t configfs none /sys/kernel/config

# 投递 dtbo
cp foo.dtbo /lib/firmware/

# 创建 overlay 实例并加载
mkdir /sys/kernel/config/device-tree/overlays/foo
echo foo.dtbo > /sys/kernel/config/device-tree/overlays/foo/path

# 校验
cat /sys/kernel/config/device-tree/overlays/foo/status   # → "applied"

# 卸载
rmdir /sys/kernel/config/device-tree/overlays/foo
```

执行链路:
1. `path` 写完 → kernel 经 `request_firmware()` 从 `/lib/firmware/` 读 dtbo
2. `of_overlay_fdt_apply()` 解析 fixups
3. 在基底 `__symbols__` 中查目标 phandle 真实数值,改写 fixups 中的引用
4. 把 overlay 节点挂到 `of_root`
5. 触发对应 `platform_device` / i2c_client / spi_device 创建 → 驱动 `.probe()`

`rmdir` 反向走完整 remove 流程,触发 `.remove()`。

## 3. 方法 B:U-Boot `fdt apply`(启动期合并)

```text
=> load mmc 1:1 0x83000000 zImage
=> load mmc 1:1 0x84000000 base.dtb
=> load mmc 1:1 0x84100000 foo.dtbo
=> fdt addr 0x84000000
=> fdt resize                    # 留出 overlay 增长空间
=> fdt apply 0x84100000          # U-Boot 把 overlay 合并进基底
=> bootz 0x83000000 - 0x84000000
```

本质:**bootloader 期完成的"静态化 overlay"**。kernel 拿到的是合并后的扁平 dtb,与普通独立 dtb 加载完全一致。

| 适用 | 备注 |
|---|---|
| 启动期可拔插(开机检测到 cape 再 apply 对应 overlay) | 无需 kernel 开 `CONFIG_OF_OVERLAY` |
| Raspberry Pi `config.txt` 的 `dtoverlay=` 走的就是这条路 | Raspi bootloader 自带 fdt apply |
| 100ask 想做 cape 实验的最低成本路径 | Linux-4.9.88 较老,configfs overlay 支持不完整 |

## 4. 方法 C:driver 主动 apply

```c
int ovcs_id;
int ret = of_overlay_fdt_apply(fdt_blob, fdt_size, &ovcs_id);
if (ret)
    return ret;

/* ... 使用 ... */

of_overlay_remove(&ovcs_id);
```

适合 **总线驱动检测到子卡后,自己塞 dtbo**。典型场景:FPGA Manager 加载 bitstream 后挂上动态外设、PCIe 热插拔板卡描述自带 dtbo。

## 5. 加载后验证

```bash
# 节点是否真的进了 of_root
ls /sys/firmware/devicetree/base/<被覆盖父节点>/

# 新设备是否触发驱动 probe
dmesg | tail
ls /sys/bus/i2c/devices/      # 或对应总线

# 反汇编当前 live dt
dtc -I fs -O dts /sys/firmware/devicetree/base
```

## 6. 常见坑

| 现象 | 原因 |
|---|---|
| `failed to resolve tree` / `no symbols` | 基底 dtb 未带 `__symbols__`,需 `dtc -@` 重新编译基底 |
| `apply` 成功但驱动不 probe | overlay 节点 `compatible` 拼写错;目标 bus controller `status="disabled"`,先在 base 中点亮 |
| `rmdir` 卡住 | 对应驱动 `.remove()` 不完整,持有引用计数 |
| 改 cpu/memory/中断控制器后 oops | 这些节点在 kernel 早期已使用,**不允许运行时改**,只能走静态 dts |
| 多个 overlay 顺序冲突 | overlay 间按 apply 顺序合并,后 apply 的覆盖前者;remove 必须**逆序** |
| `path` 写入返回 `-ENOENT` | dtbo 不在 firmware 搜索路径,或文件名拼写错 |

## 7. 三种方法对比

| 维度 | A. configfs | B. U-Boot fdt apply | C. driver 主动 |
|---|---|---|---|
| 时机 | 系统运行期 | bootloader 阶段 | 驱动 probe 内 |
| kernel 配置 | `OF_OVERLAY` + `OF_CONFIGFS` | 无需 | `OF_OVERLAY` |
| 可逆性 | `rmdir` 可卸载 | 不可逆(已合并入基底 dtb) | `of_overlay_remove` |
| 用户空间介入 | 是 | 否 | 否 |
| 典型代表 | BeagleBone Cape | Raspberry Pi `dtoverlay=` | FPGA Manager |

## 8. 100ask EVB 现状与建议

- 内核 `Linux-4.9.88` 默认 **未启用** `CONFIG_OF_OVERLAY`,configfs overlay 路径不可用。
- 若要做 cape 实验,**优先选方法 B**(U-Boot `fdt apply`):
  - 不动 kernel 配置
  - 不动 100ask 现有 SD 分区布局,只需在 boot 脚本里多 load 一个 `.dtbo` 并 `fdt apply`
- 长期路线:升级到主线 5.x kernel + 启用 `OF_OVERLAY/OF_CONFIGFS`,迁到方法 A,获得运行时可拔插能力。

## 9. 关联阅读

- 静态合并机制(为什么 dts 覆盖 dtsi 不需要 fixups)→ `Overlays/02-dts-vs-dtso-overwrite.md`
- 基底 dtb 是怎么被 kernel 拿到的 → `mechanism/01-dtb-boot-delivery.md`
- overlay 概念入门 → `Overlays/01-dt-overlays.md`
