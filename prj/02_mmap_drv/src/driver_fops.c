#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include "driver_fops.h"
#include "mmap_drv_uapi.h"

/* ------------------------------------------------------------------ */
/*  open / release                                                      */
/* ------------------------------------------------------------------ */

static int mmap_drv_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mmap_drv: open  (pid=%d)\n", current->pid);
    return 0;
}

static int mmap_drv_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "mmap_drv: close (pid=%d)\n", current->pid);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  mmap — map g_kbuf into the calling process's VMA                   */
/* ------------------------------------------------------------------ */

static int mmap_drv_mmap(struct file *file, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn  = virt_to_phys(g_kbuf) >> PAGE_SHIFT;

    if (size > MMAP_DRV_BUF_SIZE) {
        printk(KERN_ERR "mmap_drv: requested size %lu > %d\n",
               size, MMAP_DRV_BUF_SIZE);
        return -EINVAL;
    }

    /*
     * remap_pfn_range: wire [vma->vm_start, vma->vm_end) in the
     * calling process's page table to the physical pages starting at pfn.
     *
     * After this returns, any write to the userspace pointer lands
     * directly on g_kbuf's physical page — zero kernel copies.
     */
    if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) {
        printk(KERN_ERR "mmap_drv: remap_pfn_range failed\n");
        return -EAGAIN;
    }

    printk(KERN_INFO "mmap_drv: pid=%d mapped phys=0x%lx → virt=0x%lx size=%lu\n",
           current->pid, pfn << PAGE_SHIFT, vma->vm_start, size);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  ioctl — DUMP: print [seq, msg] from g_kbuf to dmesg               */
/* ------------------------------------------------------------------ */

static long mmap_drv_ioctl(struct file *file, unsigned int cmd,
                            unsigned long arg)
{
    /*
     * Shared region layout (must match app/main.c SharedRegion):
     *   [0 ..  3]  int  seq
     *   [4 .. 259] char msg[256]
     */
    int  *seq = (int *)g_kbuf;
    char *msg = (char *)g_kbuf + 4;

    switch (cmd) {
    case MMAP_DRV_IOC_DUMP:
        printk(KERN_INFO "mmap_drv [kernel view] seq=%d msg='%.64s'\n",
               *seq, msg);
        return 0;
    default:
        return -ENOTTY;
    }
}

/* ------------------------------------------------------------------ */

const struct file_operations mmap_fops = {
    .owner          = THIS_MODULE,
    .open           = mmap_drv_open,
    .release        = mmap_drv_release,
    .mmap           = mmap_drv_mmap,
    .unlocked_ioctl = mmap_drv_ioctl,
};
