#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include "driver_fops.h" // 引用分离出去的 fops

static int major = 0;
static dev_t dev_nums;
static struct cdev cdev;
static struct class *hello_class;

static int hello_init(void) {
    int ret;
    
    // 1. 申请设备号
    ret = alloc_chrdev_region(&dev_nums, 0, 1, "hello_drv");
    if (ret < 0) return ret;
    major = MAJOR(dev_nums);
    printk(KERN_INFO "hello_drv: allocated major %d\n", major);

    // 2. 初始化并添加 cdev (关联 fops)
    cdev_init(&cdev, &hello_fops);
    ret = cdev_add(&cdev, dev_nums, 1);
    if (ret) goto err_cdev;

    // 3. 创建类和设备节点 (Sysfs)
    hello_class = class_create(THIS_MODULE, "hello_class");
    if (IS_ERR(hello_class)) {
        ret = PTR_ERR(hello_class);
        goto err_class;
    }

    device_create(hello_class, NULL, dev_nums, NULL, "hello_drv");
    return 0;

err_class:
    cdev_del(&cdev);
err_cdev:
    unregister_chrdev_region(dev_nums, 1);
    return ret;
}

static void hello_exit(void) {
    device_destroy(hello_class, dev_nums);
    class_destroy(hello_class);
    cdev_del(&cdev);
    unregister_chrdev_region(dev_nums, 1);
    printk(KERN_INFO "hello_drv: exit\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yceahcan");
