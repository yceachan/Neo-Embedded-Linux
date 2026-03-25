#include <stdio.h>

/* 
 * 这是一个参数查看器。
 * 它会打印出内核在启动它时压入栈中的所有 argv 和前两个 envp。
 */
int main(int argc, char *argv[], char *envp[]) {
    printf("\n[Child Process: my_viewer]\n");
    printf("I am running! My PID is %d\n", getpid());
    
    printf("\n--- Arguments Received (argc = %d) ---\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    printf("\n--- Environment (Top 2) ---\n");
    for (int i = 0; i < 2 && envp[i] != NULL; i++) {
        printf("envp[%d]: %s\n", i, envp[i]);
    }

    return 0;
}
