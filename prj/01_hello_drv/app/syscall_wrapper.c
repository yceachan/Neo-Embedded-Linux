#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "syscall_wrapper.h"

int my_driver_open(void) {
    int fd = open("/dev/hello_drv", O_RDWR);
    if (fd == -1) {
        perror("open /dev/hello_drv failed");
    }
    return fd;
}

void my_driver_write(int fd, const char *msg) {
    write(fd, msg, strlen(msg) + 1);
}

void my_driver_read(int fd) {
    char buf[1024];
    read(fd, buf, 1024);
    printf("APP Read: %s\n", buf);
}

void my_driver_close(int fd) {
    close(fd);
}

