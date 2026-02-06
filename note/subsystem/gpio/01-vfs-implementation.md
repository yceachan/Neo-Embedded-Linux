---
title: GPIO 子系统 VFS 实现深度分析
tags: [Kernel, VFS, GPIO, Sysfs, Cdev]
update: 2026-02-07

---

# GPIO 子系统 VFS 实现深度分析

在 Linux 内核中，GPIO 子系统通过两种主要的 VFS 接口暴露给用户空间：**Sysfs** (传统方式) 和 **Character Device** (现代方式)。本文基于 Linux 4.9 内核源码进行分析。

## 1. Sysfs 接口实现 (`gpiolib-sysfs.c`)

Sysfs 接口利用内核的 `device_attribute` 机制，将文件操作映射为 `show` 和 `store` 回调函数。

### 1.1 核心数据结构
内核使用 `DEVICE_ATTR_RW` 等宏定义属性：
- `direction`: 对应 `direction_show` 和 `direction_store`
- `value`: 对应 `value_show` 和 `value_store`

### 1.2 读写逻辑映射
以 `value` 属性为例：
- **读取 (`value_show`)**：
    内部调用 `gpiod_get_value_cansleep(desc)`。该函数最终会调用 `gpio_chip->get()` 回调，由具体的硬件驱动提供。
- **写入 (`value_store`)**：
    内部调用 `gpiod_set_value_cansleep(desc, value)`。该函数最终调用 `gpio_chip->set()` 回调。

### 1.3 中断通知 (`sysfs_notify`)
当 GPIO 配置为中断输入时，内核通过 `gpio_sysfs_irq` 处理函数调用 `sysfs_notify_dirent()`。这允许用户空间程序通过 `poll()` 或 `select()` 系统调用监听 `value` 文件的变化。

## 2. 字符设备接口实现 (`gpiolib.c`)

字符设备 `/dev/gpiochipN` 提供了比 Sysfs 更高效、功能更全的交互方式。

### 2.1 文件操作集 (`file_operations`)
在 `drivers/gpio/gpiolib.c` 中定义了 `gpio_fileops`：
```c
static const struct file_operations gpio_fileops = {
    .release = gpio_chrdev_release,
    .open = gpio_chrdev_open,
    .unlocked_ioctl = gpio_ioctl,  /* 核心交互入口 */
    .owner = THIS_MODULE,
};
```

### 2.2 注册流程
1. **分配主次设备号**：调用 `alloc_chrdev_region` 申请名为 `"gpiochip"` 的设备号。
2. **初始化 cdev**：在 `gpiochip_setup_dev` 函数中调用 `cdev_init` 和 `cdev_add`。
3. **创建节点**：调用 `device_add`，配合 udev/mdev 在 `/dev` 下生成设备节点。

### 2.3 ioctl 交互
用户空间通过 `ioctl` 发送命令（如 `GPIO_GET_CHIPINFO_IOCTL`），内核在 `gpio_ioctl` 中根据命令类型执行相应的 `gpiolib` 函数。这种方式减少了 Sysfs 文本解析的开销。

## 3. VFS 层级对比总结

| 特性 | Sysfs (`/sys/class/gpio`) | Cdev (`/dev/gpiochipN`) |
| :--- | :--- | :--- |
| **访问路径** | 文件读写 (read/write) | 控制操作 (ioctl) |
| **交互形式** | 纯文本 (ASCII) | 结构化数据 (Binary) |
| **性能** | 较低（涉及格式转换） | 较高（直接传递结构体） |
| **功能扩展** | 有限（受限于属性定义） | 丰富（支持批量操作和高精事件） |
| **推荐场景** | 脚本调试、简单控制 | 高性能应用、复杂逻辑控制 |

## 4. 驱动开发者的视角
无论是通过 Sysfs 还是 Cdev 访问，**最终在内核驱动层都会汇聚到 `struct gpio_chip` 定义的操作集。**开发者只需实现底层的硬件寄存器读写逻辑（硬件资源在dts中被描述为device member)`gpiolib` 核心层负责将其桥接到 VFS 架构中。
