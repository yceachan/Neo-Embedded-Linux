#include <stdio.h>
#include "hello.h"

int process_data(int a, int b) {
    // 在 C 语言侧打印接收到的参数
    printf("[C Library] Received parameters: a = %d, b = %d\n", a, b);
    
    // 返回计算结果
    return a + b;
}
