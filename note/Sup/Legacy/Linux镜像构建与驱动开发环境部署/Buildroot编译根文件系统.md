**注意：buildroot 除了支持编译根文件系统，还能编译出uboot，kernel。但他所使用的源码来自公网仓库，未必包含开发板的驱动，我们更倾向于单独编译uboot、KERNEL和rootfs**

# 根文件系统的定义

根文件系统（root filesystem，简称 rootfs）是 Linux 系统启动后挂载的第一个文件系统，它包含系统运行所必需的最基本文件和目录结构。

内核启动后，会挂载 rootfs，然后执行 `/sbin/init`（或 systemd），开始用户空间。

**一个标准的根文件系统包含：**

✔ 必要的目录结构（FHS 标准）

| 目录    | 作用                               |
| ------- | ---------------------------------- |
| `/bin`  | 基本用户命令（ls、cp、sh 等）      |
| `/sbin` | 基本系统管理命令（init、mount 等） |
| `/etc`  | 系统配置文件                       |
| `/lib`  | 基本共享库、动态链接器             |
| `/dev`  | 设备文件（如 /dev/console）        |
| `/proc` | 内核提供的虚拟文件系统             |
| `/sys`  | 系统信息虚拟文件系统               |
| `/root` | root 用户的家目录                  |

**✔ 启动系统所需的最小程序**

例如：

- `/sbin/init` 或 systemd
- `/bin/sh`
- `/sbin/mount`
- `/sbin/udevd`

✔ **必要的库文件**

如 glibc、libc.so 等。

# 制作工具

- busybox：

  一套linux的超精简命令合集，可以用他来手动构建完整系统。常用于教学，如NJU 操作系统课。

- Buildroot：

  它是一个自动化程序很高的系统，可以以`make menuconfig`的图形化方式配置编译**内核、u-boot、根文件系统**，**规模较为精简**，常用于嵌入式Linux教学领域

- Yocto：

  **工业主流和前沿的构建方案**，复杂，大型。

在本札记中，基于Buildroot构建镜像。

# make menuconfig配置

对buildroot 的menuconfig 配置可以参考野火文档

[12. Buildroot根文件系统的构建 — [野火\]Linux镜像构建与部署——基于i.MX6ULL开发板Buildroot系统 文档](https://doc.embedfire.com/lubancat/build_and_deploy_buildroot/zh/latest/doc/buildroot_build/buildroot_build.html#id5)

有诸多目标平台，编译工具链，软件包等选项

> `make menuconfig`
>
> ![image-20251225115523897](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/image-20251225115523897.png)



对于100ask_imx6ull开发板，厂家SDK中给出了了一份适用于开发版的config配置，位于/SDK/buildroot/config 里面。

![image-20251225115632711](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/image-20251225115632711.png)

# 编译过程

![image-20251225115729617](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/image-20251225115729617.png)

# 输出文件

![image-20251225113756095](https://ali-oss-yceachan.oss-cn-chengdu.aliyuncs.com/img-bed-typora/image-20251225113756095.png)

