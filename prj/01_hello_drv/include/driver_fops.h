#ifndef _DRIVER_FOPS_H_
#define _DRIVER_FOPS_H_

#include <linux/fs.h>

// 声明外部定义的 fops，
extern const struct file_operations hello_fops;

#endif
