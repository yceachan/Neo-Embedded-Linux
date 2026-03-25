```
-tags   : [DTS]
-update : 2026-03-13
```

> [!note]
>
> - Ref [Linux 和设备树 — Linux 内核文档](https://docs.linuxkernel.org.cn/devicetree/usage-model.html)

DT 最初由开放固件创建，作为将数据从开放固件传递到客户端程序（如操作系统）的通信方法的一部分**。操作系统使用设备树在运行时发现硬件的拓扑结构** （映射到sys/bus），从而**在没有硬编码信息的情况下支持大多数可用硬件**（假设所有设备都有可用的驱动程序）。

在结构上，DT 是一棵树，或一个具有命名节点的**无环图**，节点可以有**任意数量的命名属性，封装任意数据**。还存在一种机制，用于在自然树结构之外创建一个节点到另一个节点的任意链接（别名，overlay的最佳实践）。

从概念上讲，定义了一组**通用的使用约定**，称为“绑定”，用于描述数据应该如何在树中出现，以描述典型的硬件特征，包括数据总线、中断线、GPIO 连接和外围设备。（节点命名与值规范）

---

最重要的是要理解 **DT 只是一个描述硬件的数据结构**。它没有什么神奇之处，也不会神奇地解决所有硬件配置问题。**它所做的是提供一种语言，将硬件配置与 Linux 内核（或任何其他操作系统）中的板卡和设备驱动程序支持分离**。使用它可以使板卡和设备支持成为**数据驱动**的；基于传递到内核中的数据而不是基于每台机器硬编码的选择来进行设置决策。（OF_API，基于`通用的使用约定`来提取设备信息）

## 平台识别

在 ARM 上，arch/arm/kernel/setup.c 中的 setup_arch() 将调用 arch/arm/kernel/devtree.c 中的 setup_machine_fdt()，该函数搜索 machine_desc 表并选择最匹配**设备树数据**的 machine_desc。它通过查看根设备树节点中的“compatible”属性，并将其与 struct machine_desc 中的 **dt_compat** 列表进行比较来确定**最佳匹配**

> [!tip]
>
> “compatible”属性包含一个排序的字符串列表，首先是机器的确切名称，然后是它兼容的可选板卡列表，从最兼容到最不兼容排序。例如，TI BeagleBoard 及其后续产品 BeagleBoard xM 板的根兼容属性可能分别如下所示
>
> ```dts
> compatible = "ti,omap3-beagleboard", "ti,omap3450", "ti,omap3";
> compatible = "ti,omap3-beagleboard-xm", "ti,omap3450", "ti,omap3";
> ```

## Choesn 

在大多数情况下，DT 将是固件向内核传递数据的唯一方法，因此也用于传入运行时和配置数据.**chosen 节点**是 Linux 设备树（DTS/DTB）中的**特殊节点**，不描述真实硬件，仅用于**固件 / 引导加载程序**向内核传递**运行时配置**，遵循 Open Firmware 规范。

```
chosen {
        bootargs = "console=ttyS0,115200 loglevel=8";
        initrd-start = <0xc8000000>;
        initrd-end = <0xc8200000>;
};
```

> [!tip]
>
> bootargs 属性包含内核参数，initrd-* 属性定义了 initrd blob 的地址和大小。请注意，initrd-end 是 initrd 镜像之后的第一个地址，因此这与 struct resource 的通常语义不匹配。chosen 节点还可以选择性地包含任意数量的附加属性，用于特定于平台的配置数据。
>
> 在早期引导期间，架构设置代码使用不同的辅助回调多次调用 of_scan_flat_dt() 以在设置分页之前解析设备树数据。of_scan_flat_dt() 代码扫描设备树，并使用辅助函数提取早期引导所需的信息。通常，**early_init_dt_scan_chosen() 辅助函数用于解析 chosen 节点**，包括内核参数，**early_init_dt_scan_root() 用于初始化 DT 地址空间模型**，early_init_dt_scan_memory() 用于确定可用 RAM 的大小和位置。

### 设备填充

在识别出板卡并解析完早期配置数据后，内核初始化可以以正常方式进行**。ARM 上的 machine_desc .init_early()、.init_irq() 和 .init_machine() 钩子会与DT频繁交互。**

在 DT 上下文中**，最有趣的钩子是 .init_machine()**，它主要负责用有关平台的数据填充 Linux 设备模型。历史上，这在嵌入式平台上是通过在板卡支持 .c 文件中**定义一组静态时钟结构、platform_devices 和其他数据**，**并在 .init_machine() 中大量注册**来实现的。当使用 DT 时，可以**从解析 DT 中获得设备列表，并动态分配设备结构，而不是为每个平台硬编码静态设备。**