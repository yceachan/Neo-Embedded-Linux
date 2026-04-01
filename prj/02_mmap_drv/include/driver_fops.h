#ifndef DRIVER_FOPS_H
#define DRIVER_FOPS_H

#include <linux/fs.h>

extern const struct file_operations mmap_fops;

/* Kernel buffer allocated in driver_main.c, used by mmap fop */
extern void *g_kbuf;

#endif /* DRIVER_FOPS_H */
