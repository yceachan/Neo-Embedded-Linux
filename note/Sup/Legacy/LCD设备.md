# LCD 接口 种类

![image-20250808010917347](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508080109407.png)

对于STM32 ，本身没有LCD控制器外设。固需要使用集成了控制器的模组屏，通信接口可以是IIC、SPI、8080等。

其中8**080是一种仿MCU访问外部存储器的并行接口形**式。STM32上有**FSMC外设**访问8080接口。



对于imx6 等MPU，ARM**内核 集成了 LCD Controller** ； **板载 DDR，可划分一部分区域 直接作为显存。**

# MCU-8080接口

其信号时序和控制逻辑与早期 **Intel 8080**/8085 **微处理器访问外部存储器的总线时序**非常相似。

这种接口本身用于访问外部存储器，对于那些**集成了控制器和裸屏的模组**，也**很适合引出并行的8080接口供MCU访问**，例如**STM32上有FSMC接口来兼容8080。**

**特点**

1. **并行传输：** 一次传输多位数据（通常是 8位、9位、16位或 18位）。这是它与 SPI、I2C 等串行接口最主要的区别。
2. **控制信号丰富：** 使用多个独立的控制信号线来精确管理读写操作和时序。
3. **相对高速（相比早期串行接口）：** 在相同时钟频率下，并行传输理论上比串行传输快得多（位数倍），尤其适合驱动需要快速更新大量像素数据的显示器。
4. **主从结构：** MCU 作为**主设备**，主动发起读写操作；连接的存储器或 LCD 控制器等作为**从设备**，响应主设备的命令。
5. **异步：** 没有共享的同步时钟信号。数据传输的时序由主设备通过控制信号（`RD`/`WR`）的边沿变化来指示。

**信号**

- **信号线:**
  - **数据线 (D0-D15/D7-D0):** 8位或16位宽。
  - **片选 (CSX / CS):** 选择目标LCD（当系统中有多个外设时）。
  - **写使能 (WRX / WR / nWR):** 低电平有效，表示写入操作。
  - **读使能 (RDX / RD / nRD):** 低电平有效，表示读取操作（读取状态或GRAM数据，不常用）。
  - **数据/命令选择 (D/CX / RS / A0):** 区分写入的是命令还是数据。高电平通常表示数据，低电平表示命令。
  - **复位 (RESX / RST):** 复位LCD。

# MPU的显示控制器外设

以RK系列的`VOP Vedio Operation Peripheral`为例。VOP需要访问显存`framerbuff`，并处理LCD的渲染、刷新逻辑。

- Concepts
  - encoder ： 将framebuff 编码为 RGB、LVDS 、 DSI 、EDP 、DP 、HDMI 等显示接口信号
  - Connector : 负责硬件的接入
  - Brige ：桥接设备，负责转换接口e.g. RGB 转 HDMI
  - Panel : 具体的屏幕面板
  - GEM `Graphics Execution Manager` 
  - DRM `Direct Rendering Manager`

  ![image-20250808012211577](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508080122663.png)

# Freambuff

## concept

FrameBuffer中文译名为**帧缓冲驱动**，它是出现在2.2.xx**内核中的一种驱动程序接口**。 主设备号为29，次设备号递增。

**Linux抽象出FrameBuffer这个设备来供用户态进程实现直接写屏**。 FrameBuffer机制模仿显卡的功能，将显卡硬件结构抽象掉， 可以通过FrameBuffer的读写直接对显存进行操作。 **用户可以将FrameBuffer看成是显示内存的一个映像， 将其映射到进程地址空间之后，就可以直接进行读写操作**， 而写操作可以立即反应在屏幕上。这种操作是抽象的，统一的。

用户**不必关心物理显存的位置、换页机制等等具体细节**， 这些都是由FrameBuffer设备驱动来完成的。

FrameBuffer实际上就是嵌入式系统中专门为GPU所保留的一块连续的物理内存， **LCD通过专门的总线从framebuffer读取数据，显示到屏幕上。**

![image-20250808010616882](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508080106962.png)

## App code

>  `mmap`（Memory-mapped files，内存映射文件）是操作系统中一种将文件或设备直接映射到进程虚拟地址空间的技术。它允许程序像访问内存一样读写文件，无需传统的 `read`/`write` 系统调用。
>
> ```c
> #include <sys/mman.h>
> 
> void *mmap(void *addr,    // 建议映射地址（通常设为NULL由内核选择）
>            size_t length, // 映射区域长度
>            int prot,      // 保护模式：PROT_READ/PROT_WRITE等
>            int flags,     // 标志：MAP_SHARED/MAP_PRIVATE等
>            int fd,        // 文件描述符（匿名映射设为-1）
>            off_t offset); // 文件偏移量（通常4K对齐）
> 
> int munmap(void *addr, size_t length); // 解除映射
> ```

```c
struct fb_var_screeninfo var;
struct fb_fix_screeninfo fix;

int main(void)
{
	unsigned int i;
	int ret;

	/*--------------第一步--------------*/
	fd = open("/dev/fb0",O_RDWR);			//打开framebuffer设备
	if(fd == -1){
		perror("Open LCD");
		return -1;
	}
	/*--------------第二步--------------*/
 
 	//获取屏幕的可变参数
	ioctl(fd, FBIOGET_VSCREENINFO, &var);
	//获取屏幕的固定参数
	ioctl(fd, FBIOGET_FSCREENINFO, &fix);
   
  	//打印分辨率
	printf("xres= %d,yres= %d \n",var.xres,var.yres);
 	//打印总字节数和每行的长度
	printf("line_length=%d,smem_len= %d \n",fix.line_length,fix.smem_len);
	printf("xpanstep=%d,ypanstep= %d \n",fix.xpanstep,fix.ypanstep);
 
	/*--------------第三步--------------*/
	
  fb_mem = (unsigned int *)mmap(NULL, var.xres*var.yres*4, 		//获取显存，映射内存
			PROT_READ |  PROT_WRITE, MAP_SHARED, fd, 0);   
								  
	if(fb_mem == MAP_FAILED){
		perror("Mmap LCD");
		return -1;	
	}

	memset(fb_mem,0xff,var.xres*var.yres*4);		//清屏
	sleep(1);
	/*--------------第四步--------------*/
	//将屏幕全部设置成蓝色
	for(i=0;i< var.xres*var.yres ;i++)
		fb_mem[i] = Blue;
	sleep(2);
	memset(fb_mem,0x00,var.xres*var.yres*4);		//清屏
	
	munmap(fb_mem,var.xres*var.yres*4); //映射后的地址，通过mmap返回的值	
	close(fd); 			//关闭fb0设备文件
	return 0;			
}
```

# LCD时序

> 核心是 HSYNC 水平同步 和 VSYNC 垂直同步

---

时序信号

* PCLK :像素时钟
  - 每个时钟边沿传输一个RGB88 pixel，包括前导消隐等
  - 对于特定分辨率与刷新率的视频流，时钟频率可计算 e.g. 1080p60 =2200*1125\*60=148500000＝148.5MHZ

* VSYNC :帧同步信号    
  - **作用：** 指示一**帧**扫描的开始位置。它告诉LCD面板：“新的一帧图像（整个屏幕）要开始发送了，请回到屏幕左上角”。
  - **信号特点：** 一个脉冲信号，其频率等于屏幕的刷新率（如60Hz）。脉冲的下降沿（或上升沿）通常标志着一帧扫描的开始。
  - **时序关系：** 在`VSYNC`脉冲有效期间，面板内部会进行垂直方向的复位操作，将扫描位置重置到屏幕的左上角原点。像素数据**不会**在`VSYNC`脉冲有效期间传输。
* HSYNC :行同步信号
  - **作用：** 指示一**行**扫描的开始位置。它告诉LCD面板：“新的一行数据要开始发送了，请做好准备”。
  - **信号特点：** 一个脉冲信号。脉冲的下降沿（或上升沿，取决于约定）通常标志着一行扫描的开始。
  - **时序关系：** 在`HSYNC`脉冲有效期间（低电平或高电平有效），面板内部会进行水平方向的复位操作，为接收新一行的像素数据做准备。像素数据**不会**在`HSYNC`脉冲有效期间传输。
* DE: 视频信号
  - **作用：** 电平信号，明确指示**什么时候数据线上的信号是有效的像素数据**。这是区分有效像素数据和消隐区（Blanking Period）的关键信号。
  - **信号特点：** 一个与有效像素区域完全对齐的**窗口电平信号**。在有效像素区域内为高电平（或低电平，取决于约定），在水平消隐区（HBlank）和垂直消隐区（VBlank）内为低电平（或高电平）。
  - **重要性：** 极大地简化了接收端的逻辑。接收端只需要在`DE`有效时采样数据线上的像素值即可，无需知道HFP、HBP、VFP、VBP等参数
* **RGB888 24位数据总线**
  - **作用：** 传输实际的像素颜色数据。
  - **时序关系：** 在像素时钟的有效边沿，并且`DE`信号有效时，这些数据线上的电平被锁存为当前像素的颜色值。

![image-20250808150843175](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508081508256.png)

---

时序参数

- **HSPW（水平同步脉冲宽度）**：HSync信号的脉冲宽度，通常以像素周期为单位。
- **HBP（水平后沿）**：HSync信号后，开始传输有效数据之前的无效像素数。
- **HFP（水平前沿）**：一行结束后，进入下一行之前的无效像素数。
- **VSPW（垂直同步脉冲宽度）**：VSync信号的脉冲宽度。
- **VBP（垂直后沿）**：VSync信号后，开始传输下一帧有效数据之前的无效行数。
- **VFP（垂直前沿）**：一帧结束后，进入下一帧之前的无效行数。

## RGB接口

---

> imx6ull RGB if 左：RGBif到MPUpin 右 
>
>  <img src="https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/202508081503515.png" style="zoom:50%;" /><img src="C:\Users\yceachan\AppData\Roaming\Typora\typora-user-images\image-20250808150013950.png" alt="image-20250808150013950" style="zoom:50%;" />

## DTS配置显示通路

> 显示通路：从MPU 的LCD IF 外设 ，到显示屏PANNEL 的通路 
>
> dts里配置里定义好接口设备模型，驱动里就能按照设备资源描述操控MPU的LCD IF 外设了（驱动由bsp工程师提供，用户操控drm、framebuf 即可）

### lcdif

```json
//* `imx6ull.dtsi` 芯片dtsi定义好时钟和芯片资源
//*soc->aips1->lcdif
lcdif: lcdif@021c8000 {
	compatible = "fsl,imx6ul-lcdif", "fsl,imx28-lcdif";
	reg = <0x021c8000 0x4000>;
	interrupts = <GIC_SPI 5 IRQ_TYPE_LEVEL_HIGH>;
	clocks = <&clks IMX6UL_CLK_LCDIF_PIX>,
		 <&clks IMX6UL_CLK_LCDIF_APB>,
		 <&clks IMX6UL_CLK_DUMMY>;
	clock-names = "pix", "axi", "disp_axi";
	status = "disabled";
};

//*`board.dts` boarddts定义pinctrl复用，和面板pannel时序参数
&lcdif {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_lcdif_dat
             &pinctrl_lcdif_ctrl
             &pinctrl_lcdif_reset>; 
    display = <&display0>;
    status = "okay";
    reset-gpios = <&gpio3 4 GPIO_ACTIVE_LOW>; /* 100ask */

    display0: display {
        bits-per-pixel = <24>;
        bus-width = <24>;

        display-timings {
            native-mode = <&timing0>;

             timing0: timing0_1024x768 {
             clock-frequency = <50000000>;
             hactive = <1024>;
             vactive = <600>;
             hfront-porch = <160>;
             hback-porch = <140>;
             hsync-len = <20>;
             vback-porch = <20>;
             vfront-porch = <12>;
             vsync-len = <3>;

             hsync-active = <0>;
             vsync-active = <0>;
             de-active = <1>;
             pixelclk-active = <0>;
             };

        };
    };
};
```

### pinctrl

```json
//***********RBG if pin配置******************/
&iomuxc{
        pinctrl_lcdif_ctrl: lcdifctrlgrp {
        //compatible=fsl，飞思卡尔/NXP
        fsl,pins = <
            MX6UL_PAD_LCD_CLK__LCDIF_CLK            0x79
            MX6UL_PAD_LCD_ENABLE__LCDIF_ENABLE  0x79
            MX6UL_PAD_LCD_HSYNC__LCDIF_HSYNC    0x79
            MX6UL_PAD_LCD_VSYNC__LCDIF_VSYNC    0x79
        >;
    };
    pinctrl_lcdif_dat: lcdifdatgrp {
        fsl,pins = <
            MX6UL_PAD_LCD_DATA00__LCDIF_DATA00  0x79
            MX6UL_PAD_LCD_DATA01__LCDIF_DATA01  0x79
            MX6UL_PAD_LCD_DATA02__LCDIF_DATA02  0x79
            MX6UL_PAD_LCD_DATA03__LCDIF_DATA03  0x79
            MX6UL_PAD_LCD_DATA04__LCDIF_DATA04  0x79
            MX6UL_PAD_LCD_DATA05__LCDIF_DATA05  0x79
            MX6UL_PAD_LCD_DATA06__LCDIF_DATA06  0x79
            MX6UL_PAD_LCD_DATA07__LCDIF_DATA07  0x79
            MX6UL_PAD_LCD_DATA08__LCDIF_DATA08  0x79
            MX6UL_PAD_LCD_DATA09__LCDIF_DATA09  0x79
            MX6UL_PAD_LCD_DATA10__LCDIF_DATA10  0x79
            MX6UL_PAD_LCD_DATA11__LCDIF_DATA11  0x79
            MX6UL_PAD_LCD_DATA12__LCDIF_DATA12  0x79
            MX6UL_PAD_LCD_DATA13__LCDIF_DATA13  0x79
            MX6UL_PAD_LCD_DATA14__LCDIF_DATA14  0x79
            MX6UL_PAD_LCD_DATA15__LCDIF_DATA15  0x79
            MX6UL_PAD_LCD_DATA16__LCDIF_DATA16  0x79
            MX6UL_PAD_LCD_DATA17__LCDIF_DATA17  0x79
            MX6UL_PAD_LCD_DATA18__LCDIF_DATA18  0x79
            MX6UL_PAD_LCD_DATA19__LCDIF_DATA19  0x79
            MX6UL_PAD_LCD_DATA20__LCDIF_DATA20  0x79
            MX6UL_PAD_LCD_DATA21__LCDIF_DATA21  0x79
            MX6UL_PAD_LCD_DATA22__LCDIF_DATA22  0x79
            MX6UL_PAD_LCD_DATA23__LCDIF_DATA23  0x79
        >;
    };
    
    //***********pwm 背光调节******************/
    pinctrl_pwm1: pwm1grp {
        fsl,pins = <
                MX6UL_PAD_GPIO1_IO08__PWM1_OUT 0x110b0
        >;
    };
}

backlight {
    compatible = "pwm-backlight";
    pwms = <&pwm1 0 5000000>;
    brightness-levels = <0 4 8 16 32 64 128 255>;
    default-brightness-level = <6>;
    status = "okay";
};

```

