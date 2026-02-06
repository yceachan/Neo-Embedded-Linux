```c
//* template
#include <linux/miscdevice.h>
#include <linux/fs.h>

static const struct file_operations my_fops = {
    .owner = THIS_MODULE,
    .read = my_read,
    .write = my_write,
};

static struct miscdevice my_miscdev = {
    .minor = MISC_DYNAMIC_MINOR, // 动态分配次设备号
    .name = "mydevice",          // 设备节点名
    .fops = &my_fops,            // 文件操作集
    .mode = 0666,                // 设备权限
};

static int __init my_init(void)
{
    return misc_register(&my_miscdev);
}

static void __exit my_exit(void)
{
    misc_deregister(&my_miscdev);
}

module_init(my_init);
module_exit(my_exit);
```

## 一、杂项设备的核心优势

1. **共享主设备号**：所有杂项设备共享主设备号 10
2. **自动节点创建**：无需手动 `mknod`，自动在 `/dev` 生成设备节点
3. **简化注册**：相比标准字符设备注册更简单
4. **统一管理**：内核提供统一的管理接口

* struct miscdevice` (include/linux/miscdevice.h)

```c
struct miscdevice {
    int minor;                  // 次设备号
    const char *name;           // 设备名称（决定节点名）
    const struct file_operations *fops; // 文件操作集
    struct list_head list;      // 链表节点
    struct device *parent;      // 父设备
    struct device *this_device; // 自动创建的设备对象
    const struct attribute_group **groups;
    const char *nodename;       // 自定义节点名（可选）
    umode_t mode;               // 设备权限（可选）
};
```

## . 核心函数：`misc_register`

定义miscdevice对象，提供fops，name等属性，直接调用系统api初始化即可。