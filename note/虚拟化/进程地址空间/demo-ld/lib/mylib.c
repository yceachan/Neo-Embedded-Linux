#include <stdio.h>
#include "mylib.h"

int lib_global_var = 0x12345678;

void lib_function(const char* msg) {
    printf("[mylib] %s (lib_global_var addr: %p)\n", msg, (void*)&lib_global_var);
}
