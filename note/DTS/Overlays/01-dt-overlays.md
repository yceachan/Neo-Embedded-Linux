---
title: Device Tree Overlays 详解
tags: [DTS, Linux Kernel, Device Tree, DTBO, Overlay]
desc: 详细介绍设备树插件(Device Tree Overlays)的概念、语法及内核动态解析机制
update: 2024-05-24
---

# Device Tree Overlays 详解

> [!note]
> **Ref:** 
> - `/home/pi/imx/note/DTS/docs/overlay-notes.html`
> - `/home/pi/imx/note/DTS/docs/dynamic-resolution-notes.html`

## 什么是 Device Tree Overlay (DTBO)?

Device Tree Overlay (设备树插件，源码文件通常扩展名为 `.dtso`，编译后扩展名为 `.dtbo`) 是一种无需重新编译整个内核设备树就能在系统运行时动态修改（添加、禁用、移除）实时设备树状态的机制。

通常，基础设备树(Base Device Tree)包含了平台上所有固定外设的信息。但是有很多设备是在系统启动后动态接入的（例如可插拔的扩展板、外接模块等）。此时可以通过加载一个相对独立的 DTBO，将其叠加到现有的内核实时运行的设备树（"Live Tree"）上。被叠加进来的新节点会如同原本就在基础设备树中一样触发内核动作（如动态注册新的 platform device，匹配并加载驱动等）。

## 语法与目标挂载点

编写 Overlay DTS 时的语法有以下关键特征：

1. 必须使用 `/dts-v1/;` 声明版本。
2. 必须包含 `/plugin/;` 标签来指明这是一个 Overlay 插件文件。
3. 目标挂载点：可以指定将其修改叠加到基础设备树的哪一个节点下。最常用的是 `&label` 形式，也可以使用明确的绝对路径 `&{/path/to/node}`。

下面是一个基础的 `.dtso` 示例：

```dts
// bar.dtso - 使用 label 引用目标节点
/dts-v1/;
/plugin/;

&ocp {
    /* 将被叠加的新设备节点 */
    bar {
        compatible = "corp,bar";
        /* 其他属性及子节点 */
    };
};
```

或者使用绝对路径：

```dts
// bar.dtso - 使用绝对路径引用目标节点
/dts-v1/;
/plugin/;

&{/ocp} {
    bar {
        compatible = "corp,bar";
    };
};
```

在经过 Device Tree Compiler (DTC) 编译后，目标节点中的新增内容会被自动组织打包进一个特殊的内部结构（`__overlay__` 节点），以便内核进行解析和应用。

## `-@` 编译器标志

为了让 Device Tree Overlay 中的 `&label` (如上述的 `&ocp`) 能够正确映射到基础设备树中对应的节点上，**基础设备树的源文件在编译时必须加上 `-@` 选项**。

使用 `dtc -@` 编译后，生成的 `dtb` 文件内部会多出一个 `__symbols__` 节点。这个节点保存了基础设备树中所有带 label 节点的完整路径映射表（例如 `ocp = "/ocp"` ）。
如果在编译基础 DTB 时没有带上 `-@` 标志，系统将无法在运行时解析 Overlay 中的 `&label` 目标位置，这时只能使用显式的绝对路径语法 `&{/path}` 来定位挂载点。由于依赖绝对路径的方式可移植性差，官方更推荐使用 label 和带有 `-@` 编译的基础设备树配合使用。

## 动态解析与应用机制

### 1. 编译期机制：`__fixups__` 与 `__local_fixups__`

当带有 `/plugin/;` 标志的文件被编译为 `.dtbo` 时，由于它经常引用在自身文件中不存在的外部 label（比如 `&ocp`，它的定义在基础设备树中），DTC 编译器会生成两个特殊的节点来保留重定位信息：
- `__fixups__`: 记录了所有未解析的外部引用符号（即它所依赖的外部 label）。
- `__local_fixups__`: 记录了文件内部各种 `phandle` 相互引用的位置，用于修正文件自身的内部引用偏移。

### 2. 运行期解析 (Dynamic Resolver)

在内核态，当接收到一个 DTBO 并尝试叠加时，Resolver (`drivers/of/resolver.c`) 的工作流程如下：
1. **获取最大 phandle 值**：从当前内核的 Live Tree 中找出最大的 phandle 值并加 1 作为基准。
2. **重分配本地 phandle**：利用 `__local_fixups__` 信息，将 DTBO 内部的所有 phandle 加上上一步算出的基准值，防止和系统现有的 phandle 产生冲突。
3. **解析外部引用**：遍历 `__fixups__` 里的每个外部符号。在 Live Tree（主要借助 `__symbols__` 节点）中查找这些 label 对应的目标节点，并获取目标节点在系统中的 phandle。
4. **填补外部 phandle**：将找到的外部目标 phandle 回填到 DTBO 期望这些引用的偏移位置，完成完整的连接过程。

### 3. 内核 API 应用 (Overlay In-Kernel API)

解析完毕后，内核即可通过 `drivers/of/overlay.c` 提供的 API 将数据真正合并到系统树中：
- `of_overlay_fdt_apply()`: 验证并应用一个解析好的 overlay 更改集。内核会根据增量实时修改设备树的结构，进而触发设备注册，使得符合条件的设备驱动被 probe。该调用会返回一个标识此 Overlay 的 cookie。
- `of_overlay_remove()`: 传入对应的 cookie 可以移除对应的 overlay 更改集，从而触发设备卸载动作并清理内存（注意：如果由于依赖关系被其他 overlay 栈顶遮挡，将不被允许移除）。
- `of_overlay_remove_all()`: 按照正确的顺序全部移除所有的 overlay 更改集。

> **警告：** 内核提供了相关的 Notifier 机制（如 `OF_OVERLAY_PRE_APPLY` / `OF_OVERLAY_POST_REMOVE` 等）以供特定子系统监控 overlay 行为。但开发者须注意，在 `OF_OVERLAY_POST_REMOVE` 被调用之后，相关 overlay 的内存将被彻底释放，保存指向已释放的 overlay 节点或属性数据的指针将会导致内核崩溃。