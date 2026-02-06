#ifndef _SYSCALL_WRAPPER_H_
#define _SYSCALL_WRAPPER_H_

int my_driver_open(void);
void my_driver_write(int fd, const char *msg);
void my_driver_read(int fd);
void my_driver_close(int fd);

#endif
