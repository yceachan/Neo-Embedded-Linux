#include <stdio.h>
#include <unistd.h>

/* 
 * 这是一个参数查看器。
 * 它会打印出内核在启动它时从栈中解析出的 argv 和前几个 envp。
 */
int main(int argc, char *argv[], char *envp[]) {
    printf("\n[Child Process: my_viewer]\n");
    printf("I am running! My PID is %d\n", getpid());
    
    printf("\n--- Arguments Received (argc = %d) ---\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    printf("\n--- Environment (Top 2) ---\n");
    for (int i = 0;envp[i] != NULL; i++) {
        printf("envp[%d]: %s\n", i, envp[i]);
    }

    printf("\n[Child Process: Done]\n");
    return 0;
}
