#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h> // for copy_to/from_user
#include "driver_fops.h"

static char hello_buf[1024] = {0};

static int hello_drv_open(struct inode *node, struct file *file) {
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

static int hello_drv_release(struct inode *node, struct file *file) {
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    return 0;
}

static ssize_t hello_drv_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    int ret;
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    ret = copy_from_user(hello_buf, buf, len > 1023 ? 1023 : len);
    return len;
}

static ssize_t hello_drv_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    int ret;
    printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    ret = copy_to_user(buf, hello_buf, len > 1023 ? 1023 : len);
    return len;
}

// 定义并导出 fops
const struct file_operations hello_fops = {
    .owner = THIS_MODULE,
    .read  = hello_drv_read,
    .write = hello_drv_write,
    .open  = hello_drv_open,
    .release = hello_drv_release
};
