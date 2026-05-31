:::note
一个基于buildroot 的最轻量级别的嵌入式Linux 内核，模块，根文件系统的构建以及驱动开发，所需要的SDK：
:::

![image-20251225124152722](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/image-20251225124152722.png)


- Uboot ： 编译uboot
- Linux-4.98 ：kernel源码，编译kernel镜像
- Buildroot :编译根文件系统。
- Busybox：一套构建根文件系统可以用到的linux命令集，buildroot依赖工具
- ToolChain：`arm-buildroot-linux-gnueabihf`交叉编译链。


在基于buildroot的轻量根文件系统，厂家的SDK中一般都会给出以下配置号的工程和工具链：

- 可编译出适用于开发板uboot 引导 的uboot项目
- 一份适合开发板平台，**设备树信息完善**的KERNEL 源码
- buildroot构建系统 与busybox工具集
- arm-linux-eabihf-gcc 交叉编译工具链。

?> 我怎么傻乎乎地从就虚拟机上sftp down一份下来，**100ask imx6ull开发板将这部分BSP 部署在git仓库<？**


获取这份buildroot 的SDK 后，配置好交叉编译链，即可将KERN源码包含到当前工作区，开始基于module 的驱动开发了


# uboot
本开发使用的 U-boot 位于 Git 仓库，地址为：
https://e.coding.net/weidongshan/imx-uboot2017.03.git
注意：我们使用的版本针对板子进行过修改，u-boot 官网下载的源码不能直接使用。

# kernel

- Git 仓库地址：
  https://e.coding.net/weidongshan/imx-linux4.9.88.git
- NXP 官方 kernel 源码 Git 仓库地址：
  https://source.codeaurora.org/external/imx/linux-imx

上述 Git 仓库是专为 100ask_imx6ull 系列开发板制定的 Linux 内核，

![Pasted image 20260123225849](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/Pasted%20image%2020260123225849.png)

# buildroot

Buildroot 是一组 Makefile 和补丁，可简化并自动化地为嵌入式系统构建完整的、可启动的 Linux 环境(包括 bootloader、Linux 内核、包含各种 APP的文件系统)。Buildroot 运行于 Linux 平台，可以使用交叉编译工具为多个目标板构建嵌入式 Linux 平台。Buildroot 可以自动构建所需的**交叉编译工具链，创建根文件系统，编译 Linux 内核映像，并生成引导加载程序**用于目标嵌入式系统，**或者它可以执行这些步骤的任何独立组合**。例如，可以单独使用已安装的交叉编译工具链，而 Buildroot 仅创建根文件系统。

（使用buildroot来进行toolchains，kernel等集成，需要很多额外的配置来适配到本地开发板，可用性不高，）

100ask基于 buildroot 官方 2020.02 长期支持版本进行适配 imx6ull 开发板，在此基础上针对 ST yocto 发行系统做了大量的裁剪，在保证最小系统的基础上增加对 qt5.12 库的支持，同时也支持 opencv3 编程 mqtt 库 swupdate ota 升级等等比较常用的应用，我们也会提供如何在 buildroot 新增自己的软件包教程，同时源码保存在 gitee 上，链接地址为

**https://gitee.com/weidongshan/Buildroot.git**