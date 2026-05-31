> [Linux platform子系统【1】-PLATFORM(平台)总线详解-CSDN博客](https://blog.csdn.net/m0_46535940/article/details/126149849)

`Linux驱动`模型为了保持完整性，将usb、i2c等总线外的设备挂在一条虚拟的总线上（`platform总线`），从而实现了驱动模型的统一。

对于任何一种`Linux`设备驱动模型下的总线都由两个部分组成：`struct device`；`struct driver`。在`platform`总线下就是`platform_device`和`platform_driver`

# STRUCT

## platform_devices

```c
struct platform_device { 
	const char	*name;    //名字标识
	int	     	id;       //ID标识，区分同名设备
	bool		id_auto;  //判断是否自动生成
	struct device	dev;  //内核设备模型成员 继承。
	u32		num_resources; //resource相关
	struct resource	*resource;

	const struct platform_device_id	*id_entry; //id table相关
	char *driver_override; /* Driver name to force a match */

	/* MFD cell pointer */
	struct mfd_cell *mfd_cell;

	/* arch specific additions */
	struct pdev_archdata	archdata;
};
```

### struct devices

especially:

   @release:  allback to free the device after all references havegone away. This should be set by the allocator of the device (i.e. the bus driver that discovered the device).

```c
/**
 * struct device - The basic device structure
 * @parent:	The device's "parent" device, the device to which it is attached.
 * 		In most cases, a parent device is some sort of bus or host
 * 		controller. If parent is NULL, the device, is a top-level device,
 * 		which is not usually what you want.
 * @p:		Holds the private data of the driver core portions of the device.
 * 		See the comment of the struct device_private for detail.
 * @kobj:	A top-level, abstract class from which other classes are derived.
 * @init_name:	Initial name of the device.
 * @type:	The type of device.
 * 		This identifies the device type and carries type-specific
 * 		information.
 * @mutex:	Mutex to synchronize calls to its driver.
 * @bus:	Type of bus device is on.
 * @driver:	Which driver has allocated this
 * @platform_data: Platform data specific to the device.
 * 		Example: For devices on custom boards, as typical of embedded
 * 		and SOC based hardware, Linux often uses platform_data to point
 * 		to board-specific structures describing devices and how they
 * 		are wired.  That can include what ports are available, chip
 * 		variants, which GPIO pins act in what additional roles, and so
 * 		on.  This shrinks the "Board Support Packages" (BSPs) and
 * 		minimizes board-specific #ifdefs in drivers.
 * @driver_data: Private pointer for driver specific info.
 * @power:	For device power management.
 * 		See Documentation/power/devices.txt for details.
 * @pm_domain:	Provide callbacks that are executed during system suspend,
 * 		hibernation, system resume and during runtime PM transitions
 * 		along with subsystem-level and driver-level callbacks.
 * @pins:	For device pin management.
 *		See Documentation/pinctrl.txt for details.
 * @msi_list:	Hosts MSI descriptors
 * @msi_domain: The generic MSI domain this device is using.
 * @numa_node:	NUMA node this device is close to.
 * @dma_mask:	Dma mask (if dma'ble device).
 * @coherent_dma_mask: Like dma_mask, but for alloc_coherent mapping as not all
 * 		hardware supports 64-bit addresses for consistent allocations
 * 		such descriptors.
 * @dma_pfn_offset: offset of DMA memory range relatively of RAM
 * @dma_parms:	A low level driver may set these to teach IOMMU code about
 * 		segment limitations.
 * @dma_pools:	Dma pools (if dma'ble device).
 * @dma_mem:	Internal for coherent mem override.
 * @cma_area:	Contiguous memory area for dma allocations
 * @archdata:	For arch-specific additions.
 * @of_node:	Associated device tree node.
 * @fwnode:	Associated device node supplied by platform firmware.
 * @devt:	For creating the sysfs "dev".
 * @id:		device instance
 * @devres_lock: Spinlock to protect the resource of the device.
 * @devres_head: The resources list of the device.
 * @knode_class: The node used to add the device to the class list.
 * @class:	The class of the device.
 * @groups:	Optional attribute groups.
 * @release:	Callback to free the device after all references have
 * 		gone away. This should be set by the allocator of the
 * 		device (i.e. the bus driver that discovered the device).
 * @iommu_group: IOMMU group the device belongs to.
 * @iommu_fwspec: IOMMU-specific properties supplied by firmware.
 *
 * @offline_disabled: If set, the device is permanently online.
 * @offline:	Set after successful invocation of bus type's .offline().
 *
 * At the lowest level, every device in a Linux system is represented by an
 * instance of struct device. The device structure contains the information
 * that the device model core needs to model the system. Most subsystems,
 * however, track additional information about the devices they host. As a
 * result, it is rare for devices to be represented by bare device structures;
 * instead, that structure, like kobject structures, is usually embedded within
 * a higher-level representation of the device.
 */
struct device {
	struct device		*parent;

	struct device_private	*p;

	struct kobject kobj;
	const char		*init_name; /* initial name of the device */
	const struct device_type *type;

	struct mutex		mutex;	/* mutex to synchronize calls to
					 * its driver.
					 */

	struct bus_type	*bus;		/* type of bus device is on */
	struct device_driver *driver;	/* which driver has allocated this
					   device */
	void		*platform_data;	/* Platform specific data, device
					   core doesn't touch it */
	void		*driver_data;	/* Driver data, set and get with
					   dev_set/get_drvdata */
	struct dev_links_info	links;
	struct dev_pm_info	power;
	struct dev_pm_domain	*pm_domain;

#ifdef CONFIG_GENERIC_MSI_IRQ_DOMAIN
	struct irq_domain	*msi_domain;
#endif
#ifdef CONFIG_PINCTRL
	struct dev_pin_info	*pins;
#endif
#ifdef CONFIG_GENERIC_MSI_IRQ
	struct list_head	msi_list;
#endif

#ifdef CONFIG_NUMA
	int		numa_node;	/* NUMA node this device is close to */
#endif
	u64		*dma_mask;	/* dma mask (if dma'able device) */
	u64		coherent_dma_mask;/* Like dma_mask, but for
					     alloc_coherent mappings as
					     not all hardware supports
					     64 bit addresses for consistent
					     allocations such descriptors. */
	unsigned long	dma_pfn_offset;

	struct device_dma_parameters *dma_parms;

	struct list_head	dma_pools;	/* dma pools (if dma'ble) */

	struct dma_coherent_mem	*dma_mem; /* internal for coherent mem
					     override */
#ifdef CONFIG_DMA_CMA
	struct cma *cma_area;		/* contiguous memory area for dma
					   allocations */
#endif
	/* arch specific additions */
	struct dev_archdata	archdata;

	struct device_node	*of_node; /* associated device tree node */
	struct fwnode_handle	*fwnode; /* firmware device node */

	dev_t			devt;	/* dev_t, creates the sysfs "dev" */
	u32			id;	/* device instance */

	spinlock_t		devres_lock;
	struct list_head	devres_head;

	struct klist_node	knode_class;
	struct class		*class;
	const struct attribute_group **groups;	/* optional groups */

	void	(*release)(struct device *dev);
	struct iommu_group	*iommu_group;
	struct iommu_fwspec	*iommu_fwspec;

	bool			offline_disabled:1;
	bool			offline:1;
};

```



## Resource

```c
struct resource {      // 资源结构体
	resource_size_t start;      // 资源的起始值，如果是地址，那么是物理地址，不是虚拟地址
	resource_size_t end;        // 资源的结束值，如果是地址，那么是物理地址，不是虚拟地址
	const char *name;           // 资源名
	unsigned long flags;        // 资源的标示，用来识别不同的资源
	struct resource *parent, *sibling, *child;   // 资源指针，可以构成链表
};
```
### flags

> include/linux/ioport.h

![image-20250804163100297](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508041631375.png)

e.g.

![image-20250804163304364](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508041633435.png)

## platform_driver

```c
struct platform_device_id {
	char name[PLATFORM_NAME_SIZE];
	kernel_ulong_t driver_data;
};

struct platform_driver {
	int (*probe)(struct platform_device *);                       //探查
	int (*remove)(struct platform_device *);     
	void (*shutdown)(struct platform_device *);                   //关闭
	int (*suspend)(struct platform_device *, pm_message_t state); //挂起
	int (*resume)(struct platform_device *);
	struct device_driver driver;                //   内置的device_driver 结构体 
	const struct platform_device_id *id_table;  // 该驱动支持的设备列表: @[id_entry]
};

```

### struct device_driver

```c
struct device_driver {
	const char		*name;
	struct bus_type		*bus;

	struct module		*owner;
	const char		*mod_name;	/* used for built-in modules */

	bool suppress_bind_attrs;	/* disables bind/unbind via sysfs */
	enum probe_type probe_type;

	const struct of_device_id	*of_match_table;
	const struct acpi_device_id	*acpi_match_table;

	int (*probe) (struct device *dev);
	int (*remove) (struct device *dev);
	void (*shutdown) (struct device *dev);
	int (*suspend) (struct device *dev, pm_message_t state);
	int (*resume) (struct device *dev);
	const struct attribute_group **groups;

	const struct dev_pm_ops *pm;

	struct driver_private *p;
};
```

## platform_device_id

```c
struct platform_device_id {
	char name[PLATFORM_NAME_SIZE];
	kernel_ulong_t driver_data;
};
```

# Work Flow

**工作流程（匹配与驱动加载）**

1. **设备注册 (`platform_device`):**
   - 内核启动早期（**解析设备树**时）或通过板级文件代码**bsp**注册 `platform_device`。
   - 该设备会被添加到 Platform 总线的设备列表 (`/sys/bus/platform/devices/`)。
2. **驱动注册 (`platform_driver`):**
   - 驱动程序模块加载时注册 `platform_driver`。
   - 该驱动会被添加到 Platform 总线的驱动列表 (`/sys/bus/platform/drivers/`)。
3. **总线匹配：**
   - Platform 总线核心 (`drivers/base/platform.c`) 负责**轮询**其设备列表和驱动列表。
   - **匹配规则 (优先级从高到低):**
     - **设备树匹配 (`of_match_table`):** 检查 `platform_device` 是否源自设备树节点。如果是，则用该节点的 `compatible` 属性值去匹配驱动 `platform_driver->driver.of_match_table` 表中定义的字符串。找到第一个匹配项即成功。
     - **ACPI 匹配：** 类似设备树，使用 ACPI ID。
     - **ID 表匹配 (`id_table`):** 比较 `platform_device->name` 和 `platform_driver->id_table->name`。支持通配符匹配（如 `"serial"` 匹配 `"serial8250"`）。
     - **名称直接匹配：** 直接比较 `platform_device->name` 和 `platform_driver->driver.name`。
4. **驱动绑定与探测：**
   - 一旦找到匹配的驱动，内核将 `platform_device` 结构体（代表设备）作为参数，调用该驱动的 `probe()` 函数。
   - `probe()` 函数执行设备初始化工作。
   - 如果 `probe()` 成功返回 (`0`)，设备与驱动成功绑定，设备即可用。
5. **设备移除或驱动卸载：**
   - 当设备断开连接（热插拔场景较少见）或驱动模块被卸载时，驱动的 `remove()` 函数会被调用来清理资源。


# Coding

## Device

```c
#include <linux/platform_device.h>
#include <linux/ioport.h> // for resource

 

// 2. 定义平台设备结构体本身
//void	(*release)(struct device *dev);
void	myrelease (struct device *dev){
    printk("myrelease");
}
static struct platform_device  pdev= {
    .name     = name", // 需要拍匹配驱动id_table
    .id       = -1, // 实例ID，-1表示只有一个实例
    .num_resources = ARRAY_SIZE(your_device_resources), // 资源数量
    .resource = your_device_resources, // 指向资源数组
    .release  = myrelease
};


// 3. platform_device_register、platform_add_devices
static int __init your_board_init(void)
{
    // ... 其他板级初始化代码 ...
    // 注册单个设备
    platform_device_register(&your_device);
    
    // 或者注册一组设备
    struct platform_device *devices[] = { &your_device, &another_device };
    platform_add_devices(devices, ARRAY_SIZE(devices));
    return 0;
}
device_initcall(your_board_init); // 或 arch_initcall, 取决于初始化级别需求
```


```c

extern int platform_device_register(struct platform_device *);

//*unregister 由 platform bus 管理，无需用户操作。
extern void platform_device_unregister(struct platform_device *);


//*注册一组设备，传入devs[] 的指针数组
int platform_add_devices(struct platform_device **devs, int num)
```
## Driver

> struct:
>
> ```c
> struct platform_device_id {
> 	char name[PLATFORM_NAME_SIZE];
> 	kernel_ulong_t driver_data;
> };
> struct platform_driver {
> 	int (*probe)(struct platform_device *);                       //探查
> 	int (*remove)(struct platform_device *);     
> 	void (*shutdown)(struct platform_device *);                   //关闭
> 	int (*suspend)(struct platform_device *, pm_message_t state); //挂起
> 	int (*resume)(struct platform_device *);
> 	struct device_driver driver;                //   内置的device_driver 结构体 
> 	const struct platform_device_id *id_table;  // 该驱动支持的设备列表: @[id_entry]
> };
> ```

1. 实现必须的probe 和 remove 方法，

   ```c
   int myprove (struct platform_device * pdev){
       //*申请资源：内存、中断、定时、work_queue等
   }    
   int myremove (struct platform_device * pdev){
       //*释放驱动内部私有资源，excpet platform_driver_unregister
   }    
   ```

2. 硬编码idtable并注册

   ```c
   const struct platform_device_id my_idtable[] ={
    {.name = "pdev1",.driver_data =arg},
    {.name = "pdev2".driver_date = arg}
   }
   MODULE_DEVICE_TABLE(platgorm,mytable);
   ```

3. 实现driver对象并注册

   ```c
    struct platform_driver Pdev_led_drv {
   	.probe = myprobe,
       .remove = myremove,
       .driver = {
           .name = "Pdev_led_drv",
           .owner = THIS_MODULE
       },
       .id_table = my_idtable
   };
   module_platform_driver(Pdev_led_drv);  //等同init hook、exit hook
   ```

e.g.

![image-20250804231400884](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508042314035.png)



## MACRO MAGIC

```c
//*Driver 注册*//
module_platform_driver(ahci_driver);
//等价于
static int __init mydev_init(void)
{
    return platform_driver_register(&mydev_driver);
}

static void __exit mydev_exit(void)
{
    //*这会调用remove方法，在remove中释放私有资源。
    platform_driver_unregister(&mydev_driver);
}
module_init(mydev_init);
module_exit(mydev_exit);


//*向platform总线写入id_table*//
MODULE_DEVICE_TABLE(platform, mydev_id_table);
```




## Device 与 Driver 代码匹配

先轮询idtable，于device.name match

```c
/********DEVICE**********/
// BSP 中定义的设备
static struct platform_device my_device = {
    .name = "my_device_name",  // 必须与 id_table 中的条目一致
    // ...
};

/********Driver**********/
static struct platform_device_id table[] = {
    { "my_device_name", 0 },   // 必须与 BSP 中 platform_device.name 一致
    { "my_device_v2",  0 },   // 支持多个设备名称
    { }  // 结束标记
};

static struct platform_driver mydev_driver = {
    .driver = {
        .name = "my_generic_driver",  // 驱动名称（sysfs 中可见）,匹配优先级低于idtable
        .owner = THIS_MODULE,
    },
    .probe    = mydev_probe,
    .remove   = mydev_remove,
    .id_table = mydev_id_table,  
};
```

# Driver.Probe

Platform 是一个用于提供设备管理统一模型的总线，**platform driver 提供的是suspend，resume等设备管理的method**，他的 method 里**并没有fops 文件集来操控硬件和提供接口。**

那么，如何通过Platform 来控制硬件呢？

在Probe 函数里， 根据 probe探测到的资源，使用`cdev_add`等api来注册设备的访问接口

**`platform_device_register`用于注册硬件设备（描述硬件资源），而`cdev_add`用于注册设备的访问接口（字符设备操作）。**



p.s ：`cdev_add` 在内核空间注册了设备访问接口对象；而`class_create` ;`devices_create`基于udev机制为cdev提供了文件接口（which so called '设备节点'）。

# Platform总线的价值

1. 提供一致的设备管理接口

![image-20250805002040897](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508050020014.png)

2. 硬件抽象与驱动分离。

3. 后续的设备树继承

4. **资源管理自动化**

- **资源声明**：

  ```c
  static struct resource gpio_resources[] = {
      DEFINE_RES_GPIO(12),
      DEFINE_RES_GPIO(13),
  };
  ```

   `<linux/ioport.h>`提供一套份标准资源宏
   ![image-20250805002445783](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508050024896.png)
- **驱动获取资源**：

  ```c
  struct gpio_desc *led0 = gpiod_get(dev, "led0", GPIOD_OUT_HIGH);
  ```

- **优势**：自动处理资源冲突和分配

## 代码分层示例

![image-20250805002716969](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508050027075.png)

这让`gpio_set` 这个函数是普适的，我们以`struct dev` 这个标准组件为参来设计hal即可。

## sysfs接口文件化地展示设备信息

![image-20250805003206389](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508050032504.png)

# 附 :xxx_desc 设计规范

在Linux内核中，以`xxx_desc`（描述符）命名的结构体是一种常见的设计模式。这种结构体通常用于描述硬件设备、软件组件或资源的特性和操作集合。

```c
//e.g.drivers/gpio/gpiolib.h
//*这是一份gpiolib对desc的标准实现，desc为 dev 封装了 name，flags 这些 标识。
//* 这份desc 是可以重定义的。
struct gpio_desc {
	struct gpio_device	*gdev; //*与dev的互指指针
	unsigned long		flags;
/* flag symbols are bit numbers */
#define FLAG_REQUESTED	0
#define FLAG_IS_OUT	1
#define FLAG_EXPORT	2	/* protected by sysfs_lock */
#define FLAG_SYSFS	3	/* exported via /sys/class/gpio/control */
#define FLAG_ACTIVE_LOW	6	/* value has active low */
#define FLAG_OPEN_DRAIN	7	/* Gpio is open drain type */
#define FLAG_OPEN_SOURCE 8	/* Gpio is open source type */
#define FLAG_USED_AS_IRQ 9	/* GPIO is connected to an IRQ */
#define FLAG_IS_HOGGED	11	/* GPIO is hogged */

	/* Connection label */
	const char		*label;
	/* Name of the GPIO */
	const char		*name;
};

struct gpio_device {
	int			id;
	struct device		dev;
	struct cdev		chrdev;
	struct device		*mockdev;
	struct module		*owner;
	struct gpio_chip	*chip;   //与dev互指
	struct gpio_desc	*descs;  //*与desc的互指指针
	int			base;
	u16			ngpio;
	char			*label;
	void			*data;
	struct list_head        list;

#ifdef CONFIG_PINCTRL
	/*
	 * If CONFIG_PINCTRL is enabled, then gpio controllers can optionally
	 * describe the actual pin range which they serve in an SoC. This
	 * information would be used by pinctrl subsystem to configure
	 * corresponding pins for gpio usage.
	 */
	struct list_head pin_ranges;
#endif
};


struct gpio_chip {
	const char		*label;
	struct gpio_device	*gpiodev;  //与dev互指
	struct device		*parent;   //*C继承 特性
	struct module		*owner;    //*THIS_MODULE 
    
	int			(*get)(struct gpio_chip *chip,
						unsigned offset);
	void			(*set)(struct gpio_chip *chip,
						unsigned offset, int value);
    //************mutis of ops.*****************/
};
```

## 互指关系：

1. `gpio_desc` → `gpio_device`：每个GPIO引脚知道它属于哪个设备。
2. `gpio_device` → `gpio_desc`：设备知道它管理的所有引脚。
3. `gpio_device` → `gpio_chip`：设备知道它对应的硬件控制器。
4. `gpio_chip` → `gpio_device`：硬件控制器知道它对应的设备。

## 双向访问能力

![image-20250805005413963](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508050054073.png)

![image-20250805005245326](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508050052456.png)

## 循环引用下的安全解除

```c
void gpiochip_remove(struct gpio_chip *chip)
{
    struct gpio_device *gdev = chip->gpiodev;
    
    // 1. 解除用户空间访问
    device_del(&gdev->dev);
    
    // 2. 解除引用关系
    for (i = 0; i < gdev->ngpio; i++) {
        gdev->descs[i].gdev = NULL;
    }
    
    // 3. 释放资源
    kfree(gdev->descs);
    chip->gpiodev = NULL;
    kfree(gdev);
}
```
