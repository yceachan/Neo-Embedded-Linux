---
title: GPIO 字符设备 (cdev) 用户态 C 语言编程示例
tags: [GPIO, C, User-space, Ioctl, ABI-v1]
update: 2026-02-07

---

# GPIO 字符设备 (cdev) 用户态 C 语言编程示例

在较新的内核中，推荐使用 `/dev/gpiochipN` 接口。以下示例展示了如何在用户态通过 `ioctl` 控制 GPIO（基于 ABI v1，适用于 Linux 4.9+）。

## 1. 编程流程

1. **打开设备**：打开 `/dev/gpiochip4`（对应 GPIO5）。
2. **获取芯片信息**：使用 `GPIO_GET_CHIPINFO_IOCTL` 核实信息。
3. **申请线路句柄**：使用 `GPIO_GET_LINEHANDLE_IOCTL` 请求将特定引脚配置为输出。
4. **设置电平**：对返回的句柄使用 `GPIOHANDLE_SET_LINE_VALUES_IOCTL`。
5. **释放资源**：关闭句柄文件描述符和芯片设备文件。

## 2. 示例代码 (`gpio_cdev_test.c`)

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
//**`ioctl` 这个系统调用太灵活了,因而需要引入Kernel header ，去fetch ioctl cmd的宏定义
#include <sys/ioctl.h>
#include <linux/gpio.h>
#include <string.h>

int main() {
    int chip_fd, line_fd;
    struct gpiochip_info chip_info;
    struct gpiohandle_request handle_req;
    struct gpiohandle_data handle_data;

    // 1. 打开 GPIO 控制器设备
    chip_fd = open("/dev/gpiochip4", O_RDWR);
    if (chip_fd < 0) {
        perror("Open /dev/gpiochip4 failed");
        return -1;
    }

    // 2. 获取芯片信息
    if (ioctl(chip_fd, GPIO_GET_CHIPINFO_IOCTL, &chip_info) < 0) {
        perror("Get chip info failed");
        close(chip_fd);
        return -1;
    }
    printf("Chip: %s, Label: %s, Lines: %d\n", chip_info.name, chip_info.label, chip_info.lines);

    // 3. 申请 GPIO5_IO03 (第3号引脚) 的输出句柄
    memset(&handle_req, 0, sizeof(handle_req));
    handle_req.lineoffsets[0] = 3;  // 引脚偏移量
    handle_req.lines = 1;           // 申请 1 根线
    handle_req.flags = GPIOHANDLE_REQUEST_OUTPUT;
    strcpy(handle_req.consumer_label, "my_led_test");
    handle_req.default_values[0] = 1; // 默认高电平

    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &handle_req) < 0) {
        perror("Get line handle failed");
        close(chip_fd);
        return -1;
    }
    line_fd = handle_req.fd; // 获得专门控制该引脚的 fd

    // 4. 循环切换电平
    for (int i = 0; i < 10; i++) {
        handle_data.values[0] = (i % 2); // 0 或 1
        if (ioctl(line_fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &handle_data) < 0) {
            perror("Set line values failed");
            break;
        }
        printf("Set GPIO5_IO03 to %d\n", handle_data.values[0]);
        sleep(1);
    }

    // 5. 释放资源
    close(line_fd);
    close(chip_fd);
    return 0;
}
```

## 3. 编译方法

由于程序依赖 `<linux/gpio.h>`，如果交叉编译环境的头文件版本较低，可能需要指定内核源码的头文件路径：

```bash
arm-linux-gnueabihf-gcc -I /path/to/kernel/include/uapi gpio_cdev_test.c -o gpio_cdev_test
```

## 4. 总结

使用字符设备接口的优势在于：
- **原子性**：可以一次性设置多个 GPIO 的电平。
- **稳定性**：通过文件描述符（fd）持有资源，进程退出后系统会自动回收（释放 GPIO），避免了 Sysfs 方式下 export 后忘记 unexport 的遗留问题。
