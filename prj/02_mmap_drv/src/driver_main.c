#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/mm.h>       /* __get_free_page, SetPageReserved */
#include "driver_fops.h"

/* One page of physically-contiguous kernel buffer exposed via mmap */
void *g_kbuf = NULL;

static dev_t   dev_nums;
static struct cdev  mmap_cdev;
static struct class *mmap_class;

static int mmap_drv_init(void)
{
    int ret;
    struct page *page;

    /* 1. Allocate one page (page-aligned, physically contiguous) */
    g_kbuf = (void *)__get_free_page(GFP_KERNEL);
    if (!g_kbuf) return -ENOMEM;

    /*
     * Mark the page reserved so the kernel won't swap/reclaim it
     * while it is mapped into userspace via remap_pfn_range.
     */
    page = virt_to_page(g_kbuf);
    SetPageReserved(page);
    memset(g_kbuf, 0, PAGE_SIZE);

    /* 2. Allocate device number */
    ret = alloc_chrdev_region(&dev_nums, 0, 1, "mmap_drv");
    if (ret < 0) goto err_alloc;
    printk(KERN_INFO "mmap_drv: major=%d\n", MAJOR(dev_nums));

    /* 3. Register cdev */
    cdev_init(&mmap_cdev, &mmap_fops);
    ret = cdev_add(&mmap_cdev, dev_nums, 1);
    if (ret) goto err_cdev;

    /* 4. Create /dev/mmap_drv via udev */
    mmap_class = class_create(THIS_MODULE, "mmap_class");
    if (IS_ERR(mmap_class)) { ret = PTR_ERR(mmap_class); goto err_class; }

    device_create(mmap_class, NULL, dev_nums, NULL, "mmap_drv");
    printk(KERN_INFO "mmap_drv: /dev/mmap_drv created, kbuf phys=0x%lx\n",
           virt_to_phys(g_kbuf));
    return 0;

err_class:
    cdev_del(&mmap_cdev);
err_cdev:
    unregister_chrdev_region(dev_nums, 1);
err_alloc:
    ClearPageReserved(virt_to_page(g_kbuf));
    free_page((unsigned long)g_kbuf);
    return ret;
}

static void mmap_drv_exit(void)
{
    device_destroy(mmap_class, dev_nums);
    class_destroy(mmap_class);
    cdev_del(&mmap_cdev);
    unregister_chrdev_region(dev_nums, 1);

    ClearPageReserved(virt_to_page(g_kbuf));
    free_page((unsigned long)g_kbuf);
    printk(KERN_INFO "mmap_drv: exit\n");
}

module_init(mmap_drv_init);
module_exit(mmap_drv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("yceachan");
MODULE_DESCRIPTION("mmap IPC demo driver for i.MX6ULL");
