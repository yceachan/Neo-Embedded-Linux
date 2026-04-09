#include <stdio.h>
#include <unistd.h>
#include "mylib.h"

int main() {
    printf("=== VMA Linking Experiment (PID: %d) ===\n", getpid());
    lib_function("Hello from main!");
    
    printf("Sleeping for 60s. Use as_analyzer.py to observe maps.\n");
    sleep(60);
    return 0;
}
