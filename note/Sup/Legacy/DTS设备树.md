# Concept

 ![image-20250805195221939](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508051952055.png)

 设备树用于提供平台总线下的device 的静态定义。**通过树状的数据结构，解析并在内核空间定义`struct platform_device` 设备对象**。从而把内核源码中，关于设备资源之类的垃圾代码剔除掉。

---



- **`.dts` (Device Tree Source)：** 设备树源文件，人类可读（文本）。
- **`.dtsi` (Device Tree Source Include)：** 类似 C 的头文件，包含公共部分（如 SoC 定义），被 `.dts` 包含。一般由MPU厂家提供，是该平台共用的设备树文件
- **`.dtb` (Device Tree Blob / Binary)：** 编译后的设备树二进制文件，由 Bootloader 加载并传给内核。
- **`dtc` (Device Tree Compiler)：** 编译 `.dts` -> `.dtb` 的工具。也支持反编译 `.dtb` -> `.dts`。
- **`/proc/device-tree`：** 系统启动后，内核会将解析后的设备树以目录结构的形式导出到这个虚拟文件系统下，方便查看。里面的目录对应节点，文件对应属性。
- **`fdtdump`：** 工具，用于查看 `.dtb` 文件的内容（比直接看二进制直观）。
- **绑定文档 (`Documentation/devicetree/bindings/`)：** Linux 内核源码中极其重要的文档！它规定了特定设备类型或总线（如 `Documentation/devicetree/bindings/i2c/i2c-imx.yaml`）的节点**必须包含哪些属性**、**可选哪些属性**、**属性的格式和含义**。编写或修改设备树节点时，必须参考对应的绑定文档。

# Struct

设备树是一个树状结构的数据结构，由节点（`Node`）和属性（`Property`）组成：

1. **节点 (`Node`)：**
   - 代表系统中的**一个设备或一个总线/桥**。
   - 节点有一个**名字**（例如 `uart1`, `i2c0`, `ethernet`）。
   - 节点可以包含**子节点**（代表挂载在其上的设备）和**属性**。
   - 根节点是 `/`。
   - **常见节点类型：**
     - **CPU 节点：** 描述 CPU 核心。
     - **内存节点：** 描述物理内存的大小和地址范围。
     - **总线节点：** 如 `soc` (表示片上系统，包含许多外设)、`amba`, `axi` (AMBA/AXI 总线)、`pci` (PCI/PCIe 总线)、`i2c`, `spi`, `usb` 等。总线节点下挂载连接到该总线的设备节点。
     - **外设节点：** 如 `uart`, `ethernet`, `mmc` (SD/MMC 控制器), `gpio`, `pwm`, `watchdog`, `rtc`, `adc`, `dma`, `clocks`, `interrupt-controller` 等。
2. **属性 (`Property`)：**
   - 依附于节点，描述该节点的具体**特征和配置信息**。
   - 属性由**键值对**组成。
   - **键 (`name`)** 是一个字符串。
   - **值 (`value`)** 可以是：
     - 空（仅表示存在该属性本身就有意义，例如 `status = "disabled";` 中的 `disabled` 是值）。
     - 字符串 (`string`)： 例如 `compatible = "fsl,imx6ul-uart", "fsl,imx6q-uart";`。
     - 字符串列表 (`list of strings`)： 如上例的 `compatible`。
     - 单元格 (`u32` 数值)： 例如 `reg = <0x02020000 0x4000>;` (单个地址范围)。
     - 单元格列表 (`list of cells`)： 例如 `interrupts = <0 66 IRQ_TYPE_LEVEL_HIGH>;` (中断信息)。
     - 字节序列 (`bytestring`)： 较少见。
     - **对另一个节点的引用 (`phandle`)：** 用于建立节点间关系，例如 `interrupt-parent = <&intc>;` 表示这个设备的中断控制器是 `intc` 节点。
     - **节点路径引用：** 例如 `target = &i2c1;`。
   - **关键属性：**
     - **`compatible` (最重要的属性！):** 字符串列表。它告诉内核**这个设备是什么**，以及**使用哪个驱动程序**来驱动它。内核驱动程序会声明自己兼容哪些字符串，匹配成功则绑定驱动。格式通常是 `"制造商,具体型号"`, `"制造商,兼容系列"`。例如 `compatible = "ti,omap3-i2c", "ti,omap-i2c";`。
     - **`reg`:** 描述设备在**父总线地址空间**内的寄存器资源（地址和长度）。对于内存映射设备，就是物理地址和大小。对于 I2C/SPI 设备，通常是寄存器地址或片选偏移量。可以有多个 `reg` 条目。
     - **`#address-cells`, `#size-cells`:** 位于总线节点上。定义其**子节点**的 `reg` 属性中 `< >` 内每个 `address` 和 `size` 字段分别占用多少个 `u32` 单元格。这是解析 `reg` 的关键。
     - **`interrupt-parent`:** 指向该设备所连接的中断控制器节点（通常通过 `&` 引用或 `phandle`）。
     - **`interrupts`:** 描述该设备产生的中断信号线信息（通常是中断号、触发类型等）。具体格式依赖于 `interrupt-parent` 指向的中断控制器。
     - **`clocks`, `clock-names`:** 指定设备使用的时钟源。
     - **`pinctrl-0`, `pinctrl-names`:** 指定设备使用的管脚复用配置组（通常引用 Pin Controller 节点）。
     - **`status`:** 表示设备状态，常用值 `"okay"` (启用), `"disabled"` (禁用)。
     - **`model`:** 板卡型号描述。
     - **`device_type`:** 较老或特定类型设备使用（如 `"cpu"`, `"memory"`）。

# Work Flow

- **编写源码 (`\*.dts`, `\*.dtsi`):**
  - 硬件工程师或 BSP 工程师根据目标硬件，编写设备树源文件 (`board.dts`)。
  - 大量使用包含 (`#include`) 来引用 SoC 厂商提供的通用 SoC 描述文件 (`soc.dtsi`) 和硬件模块描述 (`uart.dtsi`, `i2c.dtsi` 等)。
- **编译 (`dtc`):**
  - 使用设备树编译器 (`dtc`) 将人类可读的 `.dts` (和它包含的 `.dtsi`) 编译成机器可读的二进制格式 `.dtb` (Device Tree Blob)。
  - 命令示例：`dtc -I dts -O dtb -o my-board.dtb my-board.dts`
- **Bootloader 加载:**
  - Bootloader (如 U-Boot) 启动后，根据硬件检测或用户配置，选择正确的 `.dtb` 文件（可能从 Flash、TFTP 服务器、文件系统等加载到内存中）。
- **传递给内核:**
  - Bootloader 在跳转到内核入口点之前，将内存中 `.dtb` 的**起始地址**通过约定的方式（通常是 ARM 的 `r2` 寄存器，或其他架构的特定协议）传递给 Linux 内核。
- **内核解析 (`OF` / `FDT`)：**
  - 内核在启动的早期阶段 (`setup_arch`)，启用其设备树支持 (`CONFIG_OF`)。
  - 内核解析 Bootloader 传递过来的 `.dtb` 二进制数据，在内存中构建出设备树结构（称为 Flattened Device Tree - FDT，或 Open Firmware - OF）。
  - 内核遍历设备树节点：
    - 识别根节点、CPU 节点、内存节点（初始化内存管理）。
    - **根据节点的 `compatible` 属性，在已编译进内核或加载的模块驱动程序中查找匹配的驱动。**
    - **对于匹配成功的驱动，内核调用驱动的 `probe` 函数，并将该节点作为参数传递给驱动。驱动在 `probe` 函数中：**
      - 解析节点上的属性 (`reg`, `interrupts`, `clocks` 等) 来获取硬件资源。
      - 映射内存、申请中断、获取时钟等。
      - **初始化硬件并注册为相应的 Linux 设备（如 `platform_device`, `i2c_client`, `spi_device` 等）。**

# Grammar

## Node Struct

```c
/dts-v1/;  //必须包含的版本信息`/dts-v1/;`
//*根节点描述语法 ： / { } ; 其中,`/` 是根节点的名称
/ {
    //*子节点语法 
    [label] : <node-name> [@unit-addrdess] {
        
    };
    //属性成员语法
    `foo` = `val` ;
    
    
    compatible = "fsl,imx6ull-14x14-evk", "fsl,imx6ull";
    //#不是注释，而是约定的特定属性前缀
    #address-cells = <1>;
    
};
```

* `label` :

  e.g.`uart: serial@02288000` 节点名称是`serial@02288000` ，`@`部分描述基地址， `label`是可作为引用的别名

## Value Types

### string `""`

```c
compatible = "ti,omap3-i2c", "ti,omap-i2c"; // 按顺序匹配
```

### Cell Array `<>`

```c
interrupts = <18 IRQ_TYPE_EDGE_FALLING>;
```

### Byte Array `[]`

```c
goodix,cfg-group0 = [    6b 00 04 58 02 05 0d 00 01 0f /*etc*/   ];
```

### **Reference `&`**

```c
gpios = <&gpio5 1 GPIO_ACTIVE_LOW>;
```

### empty `;`

```json
intc: interrupt-controller@00a01000 {
	compatible = "arm,cortex-a7-gic";
	#interrupt-cells = <3>;
	interrupt-controller; //空属性，属性的存在已经传递了信息（布尔）
	reg = <0x00a01000 0x1000>,
	      <0x00a02000 0x100>;
};
```



# Compile dtc

`dtc [选项] -I <输入格式> -O <输出格式> -o <输出文件> <输入文件>`

```shell
//正编译
dtc -I dts -O dtb -o out.dtb xxx.s
//反编译
dtc -I dtb -O dts -o out.dts xxx.dtb
```

- **`-I <输入格式>` (Input):** 指定输入文件的格式。
  - `dts`: 设备树源文件 (文本格式，`.dts` 或 `.dtsi`)。
  - `dtb`: 设备树二进制文件 (`.dtb`)。
  - `fs`: `/proc/device-tree` 文件系统路径 (运行时设备树)。
  
- **`-O <输出格式>` (Output):** 指定输出文件的格式。
  - `dtb`: 设备树二进制文件 (`.dtb`) - **最常用输出**。
  - `dts`: 设备树源文件 (文本格式) - **反编译时常用**。
  - `asm`: 汇编语言格式 (较少使用)。
  - `yaml`: YAML 格式 (较新，用于绑定文档等)。

- (`-i <路径>` / `--include <路径>`):
  
   - 指定搜索 `#include` 指令中引用的 `.dtsi` 文件的目录路径。可以多次使用 `-i` 指定多个路径。
   
- **预处理控制 (`-E` / `-P`):**

   - `dtc` 内部使用 C 预处理器 (`cpp`) 来处理 `#include`, `#define`, `#ifdef` 等指令。

   - **`-E`:** 仅运行预处理器，将处理后的结果输出到标准输出 (不编译成 `.dtb` 或 `.dts`)。

   - **`-P`:** 禁止在预处理后的输出中添加 `#line` 标记 (使输出更干净)。通常与 `-E` 一起用。

# Speical Properties

1. **`compatible` (字符串列表):**
   - 等价于
   - **作用：** 告诉内核这个设备是什么，以及使用哪个驱动程序来驱动它。内核驱动程序会声明自己兼容 (`compatible`) 哪些字符串。设备树中的 `compatible` 列表会按顺序与驱动硬编码的`id_table`行匹配，匹配成功则绑定驱动。
   - **格式：** `"manufacturer,model"`, `"manufacturer,compatible-family"`。越具体、越匹配硬件的字符串放前面。
   - **示例：** `compatible = "ti,omap3430-mmc", "ti,omap3-mmc";`

2. **`#address-cells`, `#size-cells` (单元格):**

   - **作用：** 定义该节点（通常是一个**总线控制器**节点）的**子节点**的 `reg` 属性中 `address` 和 `size` 字段分别占用多少个 `u32` 单元格。
   - **位置：** 位于总线控制器节点上。

3. **`reg` (单元格列表):**
   - **作用：** 描述设备在**父总线地址空间**内的寄存器资源（地址和长度）。
   - **格式：** `<address1 length1 [address2 length2] ...>`。`address` 和 `length` 字段各自占用的单元格数量 (`u32`) 由**父节点**的 `#address-cells` 和 `#size-cells` 属性严格定义。
   - **意义：** 驱动通过 `reg` 属性获知寄存器物理地址和大小，从而进行内存映射 (`ioremap`) 并操作硬件。

   ```json
   soc {
       #address-cells = <1>; // 子节点 reg 的 address 占 1 cell
       #size-cells = <1>;    // 子节点 reg 的 size 占 1 cell
       serial@4000f000 {
           compatible = "ns16550";
           reg = <0x4000f000 0x1000>; // 地址=0x4000f000 (1 cell), 长度=0x1000 (1 cell)
       };
   };
   
   spi4 {  
       #address-cells = <1>;
       #size-cells = <0>;
       gpio_spi: gpio_spi@0 {
           #gpio-cells = <2>;
            reg = <0>;
       }；
      
   };
   ```
4. **Interrupts**
   
   ```json
	intc: interrupt-controller@00a01000 {
   	compatible = "arm,cortex-a7-gic";
   	#interrupt-cells = <3>;
   	interrupt-controller; //空属性，属性的存在已经传递了信息（布尔）
   	reg = <0x00a01000 0x1000>,
   	      <0x00a02000 0x100>;
   };
   
   gpc: gpc@020dc000 {
   	interrupt-controller;
   	#interrupt-cells = <3>;
   	interrupts = <GIC_SPI 89 IRQ_TYPE_LEVEL_HIGH>;
   	interrupt-parent = <&intc>;
       //***and so many properties**/
   };
   ```

-  **`interrupt-parent` (phandle 引用):**
   - **作用：** 指向该设备所连接的中断控制器节点。通常通过 `&label` 引用。
   - **意义：** 告诉内核该设备的中断应该注册到哪个中断控制器上。如果省略，通常会继承父节点的 `interrupt-parent`。
   
-  **`interrupts` (单元格列表):**
   - **作用：** 描述该设备产生的中断信号线的具体信息。
   
   - **格式：** 完全依赖于该节点 `interrupt-parent` 指向的**中断控制器**所定义的绑定规范。**通常包含中断号 (`irq number`) 和中断触发标志 (`flags`)，**每个中断可能占用 1 个、2 个或更多单元格。
   
   - **常见格式 (如 ARM GIC):** `< IRQ_TYPE_LEVEL_HIGH>`。具体数值定义在 `include/dt-bindings/interrupt-controller/irq.h` 中。
   
   - **示例：** `interrupts = <0 66 IRQ_TYPE_LEVEL_HIGH>;` (假设该中断控制器要求每个中断 3 cells)
   
5. **`clocks`, `clock-names` (phandle 引用, 字符串列表):**
   
   - **作用：** 指定设备使用的时钟源。
   
   - **`clocks`：** 一个或多个指向时钟提供者节点 (`clock-controller`) 的 phandle 引用，通常还包含一个时钟标识符（ID 或索引）。
   
   - **`clock-names`：** (可选但强烈推荐) 为 `clocks` 列表中的每个时钟提供一个名称字符串。驱动通过这些名称来请求特定的时钟。
   
   - **示例：**
   
     ```json
     i2c2: i2c@021a4000 {
     	#address-cells = <1>;
     	#size-cells = <0>;
     	compatible = "fsl,imx6ul-i2c", "fsl,imx21-i2c";
     	reg = <0x021a4000 0x4000>;
     	interrupts = <GIC_SPI 37 IRQ_TYPE_LEVEL_HIGH>;
     	clocks = <&clks IMX6UL_CLK_I2C2>;
     	status = "disabled";
     };
     ```

6. `pinctrl`

   ```json
   &i2c1 {
       clock-frequency = <100000>;
       pinctrl-names = "default";
       pinctrl-0 = <&pinctrl_i2c1>;
       status = "okay";
   };
   
   
   pinctrl_i2c1: i2c1grp {
       fsl,pins = <
           MX6UL_PAD_UART4_TX_DATA__I2C1_SCL 0x4001b8b0
           MX6UL_PAD_UART4_RX_DATA__I2C1_SDA 0x4001b8b0
       >;
   };
   ```

   - `pinctrl-0`, `pinctrl-1`, ...约定的pin 配置enum标识，用户通过这些属性快捷的设置为某套配置
   ![image-20250805212330581](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508052123739.png)
   - `pinctrl-names`：设备树中硬编码的一套pinctrl属性集。

7. `model` :提供给bash 的可读描述信息
8. `status`
   - `"okay"`：设备已启用（默认状态，通常可省略）。
   - `"disabled"`：设备被禁用。内核将忽略此节点，不会尝试绑定驱动。
   - `"fail"` / `"fail-sss"`：表示设备存在严重问题，不应启用（较少用）。
   - `"reserved"`：设备保留给其他软件（如安全世界、协处理器），Linux 不应使用。

---

* 始终参考内核源码中的 `Documentation/devicetree/bindings/` 目录下的 YAML 或 `.txt` 文件，了解特定设备或总线**必须 (required)** 和 **可选 (optional)** 的属性及其**精确格式**。这是编写正确设备树的终极指南。

![image-20250805213033283](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508052130420.png)

# 设备树节点到内核对象的转换

## `struct device_node`

- 内核解析`dtb`文件，把每一个节点都转换为`device_node`结构体。

   ![image-20250813155838834](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508131558020.png)

- 对于含有compatible属性的节点，将会转化为`platform_Device`。`devices_node.properties`属性链表 将转化成`platform_devices.resource`数组

- `platform_devices`的成员对象`struct devices`内包含成员`struct device_ondoe *of_node` 的节点指针。`probe`接口可以通过该指针访问设备树中硬编码的`properties`链表
## 与platform_driver匹配

![image-20250813160754056](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508131607192.png)

## 从设备树属性到Resource数组

`resource` 数组主要描述设备的**物理地址资源(reg)和中断资源(irq),dma**等，仅设备树中特定的属性`reg` ;`intterrupts`等会被自动转换为Resource。

对于 `of_address_to_resource()` 这一系列设备树解析函数的调用位置，**内核已经在创建 `platform_device` 时`of_device_alloc()`自动完成了基础资源的转换**,

- 关键转换流程

1. **`reg` 属性 → 内存资源 (IORESOURCE_MEM)**

   - **作用**：描述设备寄存器或内存区域的物理地址范围。

   - **转换规则**：

     - `reg = <地址1 长度1 [地址2 长度2] ...>` 被解析为多个 `struct resource` 条目。
     - 每个条目类型为 `IORESOURCE_MEM`。
     - **地址/长度值受父节点的 `#address-cells` 和 `#size-cells` 约束。**

   - **示例**：

     ```json
     dev: dev@40000000 {
         reg = <0x40000000 0x1000   // 区域1: 基址0x40000000, 长度0x1000
                0x40001000 0x2000>; // 区域2: 基址0x40001000, 长度0x2000
     };
     ```

     → 生成两个 `resource`:

     - `{ .start=0x40000000, .end=0x40000FFF, .flags=IORESOURCE_MEM }`
     - `{ .start=0x40001000, .end=0x40002FFF, .flags=IORESOURCE_MEM }`

2. **`interrupts` 属性 → 中断资源 (IORESOURCE_IRQ)**

   - **作用**：描述设备使用的中断号。

   - **转换规则**：

     - `interrupts = <中断号1 触发类型1 [中断号2 触发类型2] ...>` 被解析为多个中断资源。
     - 每个条目类型为 `IORESOURCE_IRQ`。
     - 中断控制器信息由 `interrupt-parent` 指定。

   - **示例**：

     ```json
     dev {
         interrupts = <15 IRQ_TYPE_EDGE_RISING>, <16 IRQ_TYPE_LEVEL_HIGH>;
     };
     ```

     → 生成两个 `resource`:

     - `{ .start=15, .flags=IORESOURCE_IRQ | IRQ_TYPE_EDGE_RISING }`
     - `{ .start=16, .flags=IORESOURCE_IRQ | IRQ_TYPE_LEVEL_HIGH }`

3. **其他属性 (如 `dma`, `clocks` 等) → 特殊处理**

   - **DMA 资源**：通过 `dmas` 和 `dma-names` 属性解析为 `IORESOURCE_DMA`。
   - **时钟/复位/电源等**：由各自子系统处理（如 `clk_get()`），不进入 `resource` 数组。

- 非资源属性如何处理？

设备树中的通用属性（如 `compatible`, `status`, 自定义属性）**不会**放入 `resource` 数组，而是：

1. 存储在 `platform_device.dev.of_node` 指向的设备树节点结构中。

2. 驱动通过 `of_*` 系列 API 访问：

   ```c
   // 读取属性示例
   const char *name = of_get_property(dev->of_node, "property-name", NULL);
   u32 value;
   of_property_read_u32(dev->of_node, "my-value", &value);
   ```

## `of_* api`

获得`of_node`后，驱动即可以使用一套`of`API 来解析设备的属性。并在probe中申请若干资源。**这套操作是与pdev->resources 无关的**。

此时的设备是`of_devices`,挂载在`Platform`总线下，与`platform_driver`匹配。`platform_driver`无需通过遍历`platform_deivces.resourece`的方式来在`probe`中申请资源组件。

| 函数                        | 作用                                |
| :-------------------------- | :---------------------------------- |
| `of_get_child_by_name()`    | 获取指定名称的子节点                |
| `for_each_child_of_node()`  | 遍历所有子节点                      |
| `of_get_named_gpio_flags()` | 解析GPIO编号和标志                  |
| `of_property_read_u32()`    | 读取32位整数属性（如`linux,code`）  |
| `of_property_read_bool()`   | 检查布尔属性（如`gpio-key,wakeup`） |
| `of_get_property()`         | 获取属性原始数据（如`label`）       |

# GPIO设备树

```json
/*
*`imx6ull.dtsi`
*soc->gpio1
*/
//*chip
gpio1: gpio@0209c000 {
	compatible = "fsl,imx6ul-gpio", "fsl,imx35-gpio";
	reg = <0x0209c000 0x4000>;
	interrupts = <GIC_SPI 66 IRQ_TYPE_LEVEL_HIGH>,
		     <GIC_SPI 67 IRQ_TYPE_LEVEL_HIGH>;
	gpio-controller;
	#gpio-cells = <2>;
	interrupt-controller;
	#interrupt-cells = <2>;
};

//*board.dts
&ecspi3 { 
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_ecspi3>;
    
    //*	#gpio-cells = <2>; so
    //* gpio1 chip ，20th pin  低电平有效 
    cs-gpios = <&gpio1 20 GPIO_ACTIVE_LOW>;
    status = "okay";
    
    
    spidev: icm20608@0{
        compatible = "invensense,icm20608";
        interrupt-parent = <&gpio1>;
        interrupts = <1 1>;
        spi-max-frequency = <8000000>; 
        reg = <0>; 
    };
};
```

![image-20250810221852491](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102218638.png)

## gpio-ranges

> 将gpio控制器上的引脚映射到全局的`GPIO line`上。

![image-20250810222303092](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102223217.png)

## pinctrl

* pinmux

芯片包含数量有限的引脚，其中大多数具有多个信号选项。这些信号到引脚和引脚到信号选项由输入输出多路复用器选择称为 IOMUX。IOMUX 还用于配置其他引脚特性，例如电压电平、驱动强度和迟滞。

e.g.:![image-20250810224438385](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102244486.png)

---

![image-20250810224836986](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102248117.png)

![image-20250810224626623](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102246754.png)

```json
//* chip.dtsi
iomuxc: iomuxc@020e0000 {
			compatible = "fsl,imx6ul-iomuxc";
			reg = <0x020e0000 0x4000>;
		};
//* board.dts
//* #define MX6UL_PAD_UART1_RTS_B__GPIO1_IO19		0x0090 0x031c 0x0000 5 0
&iomuxc {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_hog_1>;
    imx6ul-evk {
        pinctrl_hog_1: hoggrp-1 {
            fsl,pins = <
                MX6UL_PAD_UART1_RTS_B__GPIO1_IO19   0x17059 /* SD1 CD */
                MX6UL_PAD_GPIO1_IO00__ANATOP_OTG1_ID    0x17059 /* USB OTG1 ID */
                // MX6UL_PAD_CSI_DATA07__GPIO4_IO28           0x000010B0
                MX6ULL_PAD_SNVS_TAMPER5__GPIO5_IO05        0x000110A0
            >;
        };
}
```

### imx6ull pinctrl规范

> [基于 6/6ULL 的 Pinmuxing i.MX 模块 |Toradex 开发者中心 --- Pinmuxing i.MX 6/6ULL Based Modules | Toradex Developer Center](https://developer.toradex.com/software/linux-resources/device-tree/pinmuxing-guide/pinmuxing-with-imx66ull-based-modules/)
>
> 要配置引脚，需要引脚控制器节点内具有属性 **fsl，pins** 的设备树节点。需要将单元格分配给属性，**每个`pin`属性需要 5 个`cell`值。**前四个通常由预处理器宏（**参见 `arch/arm/boot/dts/imx6ull-pinfunc.h` 或 `arch/arm/boot/dts/imx6ull-pinfunc-lpsr.h` ）给出;**
>
> e.g. `#define MX6UL_PAD_UART1_RTS_B__GPIO1_IO19		0x0090 0x031c 0x0000 5 0`
>
> 通常，前四个u32表示寄存器的偏移地址、引脚功能选择（mux寄存器)、以及输入选择（input select寄存器）。
>
> 最后一个单元的，称为“pad”配置， 直接写入引脚的 **Pad Control 寄存器**（`IOMUXC_SW_PAD_CTL_PAD_xxx`）

**REF**

- **硬件手册**：i.MX6ULL Reference Manual (IMX6ULLRM) 的 **Chapter 32: IOMUX Controller (IOMUXC)**。
- **内核源码**：Linux 内核中的 `drivers/pinctrl/freescale/pinctrl-imx6ul.c` 和寄存器定义文件。

> `MUX_CTL_PAD`寄存器在以下目录结构中被定义：
>
> ![image-20250810233053373](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102330511.png)

### pad config Reg

---

`xxx_SW_MUX_CTL_PAD_x`

复用控制

![image-20250810233229792](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102332953.png)

---

`xxxx_SW_PAD_CTL_PAD_XXX`

> 施密特触发器，上下拉电阻、开漏/CMOS输出，速度，输出电阻等电气属性配置

![image-20250810234822002](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102348130.png)

> bit 16 **Schmmit** ，施密特触发器，具备迟滞特性 **Hysteresis**
>
> bit 13-12 **Keeper** ：一种弱驱动的 **状态保持电路**（类似锁存器），当引脚处于高阻态（无驱动）时，自动维持上一次的逻辑电平。**是总线中常见的电路。可近似理解为悬空**
>
> bit 5-3 ：驱动强度 ：输出级MOSFET的等效输出电阻



![image-20250810234907864](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508102349996.png)

#### 常见值

```json
mx6ul-evk {
        pinctrl_hog_2: hoggrp-2 {
            fsl,pins = <
                MX6ULL_PAD_SNVS_TAMPER9__GPIO5_IO09     0x1b0b0 /* enet1 reset */
                MX6ULL_PAD_SNVS_TAMPER6__GPIO5_IO06     0x1b0b0 /* enet2 reset */
                MX6ULL_PAD_SNVS_TAMPER1__GPIO5_IO01     0x000110A0 /*key 1*/
            >;
        };

         pinctrl_tsc_reset: tscresetgrp  {        /*!< Function assigned for the core: Cortex-A7[ca7] */
            fsl,pins = <
                MX6ULL_PAD_SNVS_TAMPER2__GPIO5_IO02        0x000110A0
            >;
        }; pinctrl_hog_2: hoggrp-2 {
            fsl,pins = <
                MX6ULL_P

        pinctrl_spi4: spi4grp {
            fsl,pins = <
                MX6ULL_PAD_BOOT_MODE0__GPIO5_IO10        0x70a1
                MX6ULL_PAD_BOOT_MODE1__GPIO5_IO11        0x70a1
                MX6ULL_PAD_SNVS_TAMPER7__GPIO5_IO07      0x70a1
                MX6ULL_PAD_SNVS_TAMPER8__GPIO5_IO08      0x80000000
            >;
        };

        pinctrl_leds: ledgrp {
            fsl,pins = <
                  MX6ULL_PAD_SNVS_TAMPER3__GPIO5_IO03        0x000110A0
            >;
        };

        pinctrl_485_ctl: uart3_rs485 {
            fsl,pins = <
            MX6ULL_PAD_SNVS_TAMPER0__GPIO5_IO00     0x1b0b0
            >;
        };
        
    };
```

| 场景                        | 配置值     | 关键特性                                                   |
| :-------------------------- | :--------- | :--------------------------------------------------------- |
| led                         | 0x000110A0 | 施密特（迟滞），悬空，100Mhz输出速率，260/4 Ohm 输出电阻   |
| uart总线                    | 0x1b0b0    | 迟滞，100K上拉，100Mhz，260/6 Ohm                          |
| 快速输入采样（e.g.告诉spi） | 0x70a1     | 无施密特，47k上拉，100Mhz,260/4 Ohm ,  快压摆率（`SRE=1`） |


### imx开发版led设备树

```json

    leds {
        compatible = "gpio-leds";
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_leds>;
        
        status = "disabled";

        led0: cpu {
            label = "cpu";
            gpios = <&gpio5 3 GPIO_ACTIVE_LOW>;
            default-state = "on";
            linux,default-trigger = "heartbeat";
        };

    };

    pinctrl_leds: ledgrp {
            //*这是一个属性节点`fsl,pins`
            fsl,pins = <
                  MX6ULL_PAD_SNVS_TAMPER3__GPIO5_IO03        0x000110A0
            >;
        };

#define MX6ULL_PAD_SNVS_TAMPER3__GPIO5_IO03                        0x0014 0x0058 0x0000 0x5 0x0
```

# Bingings文档

![  ](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508131947086.png)
