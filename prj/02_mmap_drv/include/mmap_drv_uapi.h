#ifndef MMAP_DRV_UAPI_H
#define MMAP_DRV_UAPI_H

/* Shared between kernel driver and userspace app.
 * Kernel includes <linux/ioctl.h>, userspace includes <sys/ioctl.h>.
 */
#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#define MMAP_DRV_MAGIC    'm'
#define MMAP_DRV_BUF_SIZE 4096   /* one page, covers SharedRegion + slack */

/* Dump [seq, msg] from the kernel buffer to dmesg */
#define MMAP_DRV_IOC_DUMP  _IO(MMAP_DRV_MAGIC, 0)

#endif /* MMAP_DRV_UAPI_H */
