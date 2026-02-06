#include <stdio.h>
#include "syscall_wrapper.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <string>\n", argv[0]);
        return -1;
    }

    int fd = my_driver_open();
    if (fd < 0) return -1;

    my_driver_write(fd, argv[1]);
    my_driver_read(fd);
    
    my_driver_close(fd);
    return 0;
}

