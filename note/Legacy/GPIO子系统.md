# 100ask-imx开发板gpio

```shell
[root@imx6ull:~]# cat /sys/kernel/debug/gpio
gpiochip0: GPIOs 0-31, parent: platform/209c000.gpio, 209c000.gpio:
 gpio-5   (                    |goodix_ts_int       ) in  hi IRQ
 gpio-19  (                    |cd                  ) in  hi IRQ
 gpio-20  (                    |spi_imx             ) out hi    

gpiochip1: GPIOs 32-63, parent: platform/20a0000.gpio, 20a0000.gpio:

gpiochip2: GPIOs 64-95, parent: platform/20a4000.gpio, 20a4000.gpio:
 gpio-68  (                    |lcdif_rst           ) out hi    

gpiochip3: GPIOs 96-127, parent: platform/20a8000.gpio, 20a8000.gpio:
 gpio-110 (                    |User2 Button        ) in  hi IRQ
 gpio-120 (                    |spi_imx             ) in  lo    
 gpio-122 (                    |spi_imx             ) in  lo    

gpiochip4: GPIOs 128-159, parent: platform/20ac000.gpio, 20ac000.gpio:
 gpio-129 (                    |User1 Button        ) in  hi IRQ
 gpio-130 (                    |goodix_ts_rst       ) out hi    
 gpio-134 (                    |phy-reset           ) out hi    
 gpio-135 (                    |spi32766.0          ) out hi    
 gpio-136 (                    |?                   ) out lo    
 gpio-137 (                    |phy-reset           ) out hi    
 gpio-138 (                    |spi4                ) out hi    
 gpio-139 (                    |spi4                ) out lo    

gpiochip5: GPIOs 504-511, parent: spi/spi32766.0, 74hc595, can sleep:
 gpio-505 (                    |?                   ) out hi    
```

# 用户空间接口-sysfs

通过操作`/sys/class/gpio` 目录下的文件来实现对gpio的配置

通过向`export` 和`unexport` 写入`x` ，来将`gpiox` 导入/导出到用户空间

`export` 会在gpio文件夹下生成`gpiox`目录，包含以下文件，写值即可。

- **direction**：表示GPIO引脚的方向，它的可取值如下：
  1. in：引脚为输入模式。
  2. out：引脚为输出模式，且默认输出电平为低。
  3. low：引脚为输出模式，且默认输出电平为低
  4. high：引脚为输出模式，且默认输出电平为高
- **value**：表示GPIO的电平，1表示高电平，0表示低电平。GPIO被配置为输出模式， 那么修改该文件的内容可以改变引脚的电平。
- **edge**：用于配置GPIO的中断触发方式，当GPIO被配置为中断时，可以通过系统 的poll函数监听。edge文件可取如下的属性值：
  1. none：没有使用中断模式。
  2. rising：表示引脚为中断输入模式，上升沿触发。
  3. falling：表示引脚为中断输入模式，下降沿触发。
  4. both：表示引脚为中断输入模式，边沿触发。
- **`active_low`**
  - **作用：** 反转 GPIO 引脚的**逻辑电平**解释。用于处理“低电平有效”的硬件信号（如 LED 共阳极接法、按键按下拉低等）。这是软件层面的反转，不改变实际物理电平。
  - **读写：** 可读可写。
  - **参数/值：**
    - **读取 (`cat active_low`)：**
      - `0`: **默认值**。表示 `value` 文件直接反映物理电平（高=1, 低=0）。
      - `1`: 表示 `value` 文件的值是物理电平的反转（物理高=0, 物理低=1）。
- **`device`**
  - **作用：** 这是一个**符号链接 (symlink)**，指向 `/sys/devices/` 目录下实际控制这个 GPIO 引脚的**硬件设备 （gpio_chip）**

* SHEEL:cmd

```shell
#以下所有操作均需要打开管理者权限使用
#使能引脚PC3
echo 67 > /sys/class/gpio/export

#设置引脚为输入模式
echo in > /sys/class/gpio/gpio67/direction
#读取引脚的值
cat /sys/class/gpio/gpio67/value

#设置引脚为输出模式
echo out > /sys/class/gpio/gpio67/direction
#设置引脚为低电平
echo 0 > /sys/class/gpio/gpio67/value
#设置引脚为高电平
echo 1 > /sys/class/gpio/gpio67/value

#复位引脚
echo 67 > /sys/class/gpio/unexport
```

* Code:file-io

```c
//*类似地写值就可以。
int gpio_export(char *name)
{
    int fd;
   
  fd = open("/sys/class/gpio/export", O_WRONLY);
 
  write(fd, name, strlen(name));
  close(fd);
   
    return 0;
}
```

# 内核驱动接口-gpiochip

## Strcut 

![image-20250805221653320](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508052216436.png)

```c
//e.g.drivers/gpio/gpiolib.h 实现了 desc描述符 (api入口)  、 dev设备（驱动逻辑）  、  chip控制器(硬件控制) 的三层分层设计。

/ ***************struct gpio_desc ***************/
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

/ ***************struct gpio_device ***************/
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

/ ***************struct gpio_chip ***************/
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

1. `gpio_desc` → `gpio_device`：每个GPIO引脚知道它属于哪个设备。
2. `gpio_device` → `gpio_desc`：设备知道它管理的所有引脚。
3. `gpio_device` → `gpio_chip`：设备知道它对应的硬件控制器。
4. `gpio_chip` → `gpio_device`：硬件控制器知道它对应的设备。

## API

gpiolib有新旧两套api，新的基于描述符desc，旧的基于gpio号。

### gpio_request

通过gpio号申请gpio资源

![image-20250805221850909](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508052218976.png)

### gpio_to_desc

通过gpio号获得描述符，后续通过描述符号

 ![image-20250805221950104](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508052219159.png)

### 基于desc的标准GPIO API

```c
//*declared at <linux/gpio/consumer.h> 
int gpiod_get_direction(struct gpio_desc *desc)
    
int gpiod_get_value(const struct gpio_desc *desc);
void gpiod_set_value(struct gpio_desc *desc, int value);
void gpiod_set_array_value(unsigned int array_size,
			   struct gpio_desc **desc_array, int *value_array);

int gpiod_direction_input(struct gpio_desc *desc);
int gpiod_direction_output(struct gpio_desc *desc, int value);
int gpiod_direction_output_raw(struct gpio_desc *desc, int value);


//通过desc获得irq
int gpiod_to_irq(const struct gpio_desc *desc);
```

### 基于gpio号的旧版API

> 已改成对基于desc的封装。
>
> `arch/arm/include/asm/gpio.h`

![image-20250805222657879](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508052226984.png)

### 新旧api的适用范围

 关键点是，要使用`gpiod_get`系列函数，你需要一个有效的`struct device`指针。这个指针通常嵌入在设备的总线特定结构中（如`struct platform_device`，`struct pci_dev`，`struct i2c_client`等），并且可以通过`&pdev->dev`、`&client->dev`等方式获取。

 然而，**如果你正在编写一个不关联于任何具体设备的模块**（例如，一个在系统初始化时运行的模块，或者一个测试模块），**你可能没有这样一个`struct device`。在这种情况下，可以使用legacy GPIO API**

# code -User_button

>  hw:![image-20250806165848000](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508061658168.png)

这里原理图描述的gpio组号是从1开始的，kernel中chip的编号是从0开始的

即，user_button 的两个按键，在设备树上定义是`gpiochip4_1` 和`gpio3_14`,即`129` 和 `110`

![image-20250806170038548](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508061700626.png)
