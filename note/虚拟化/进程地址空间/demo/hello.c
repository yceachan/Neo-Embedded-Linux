#include <stdio.h>
#include <unistd.h>

int global_var = 42;
static int static_var = 100;

void test_function() {
    printf("I am a test function at %p\n", test_function);
}

int main() {
    char buf[100];
    printf("Hello, Address Space!\n");
    printf("PID: %d\n", getpid());
    printf("Global Var (Data): %p\n", &global_var);
    printf("Static Var (Data/BSS): %p\n", &static_var);
    test_function();
    
    printf("Sleeping for 30 seconds for analysis...\n");
    sleep(30);
    
    return 0;
}
