#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

/*
 * 调用者程序。
 * 演示：
 * 1. fork 之后使用 execve 传递自定义 argv 和 envp。
 * 2. 验证子进程是否能获得完全独立的环境变量空间。
 */
int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // --- 子进程 ---
        printf("[Child Context] PID: %d. Preparing custom argv and envp...\n", getpid());

        // 1. 准备自定义参数数组 (argv)
        char *args[] = {
            "MY_CUSTOM_NAME", // argv[0]
            "param1",
            "param2",
            NULL
        };

        // 2. 准备自定义环境变量数组 (envp)
        // 注意：这会完全覆盖掉从父进程继承来的环境
        char *envs[] = {
            "MY_PLATFORM=IMX6ULL",
            "USER_MODE=DEVELOPER",
            "PATH=/usr/bin:/bin", // 至少保留 PATH 以防 my_viewer 内部需要
            NULL
        };

        printf("[Child Context] Calling execve('./my_viewer', ...)\n");

        // 执行当前目录下的 my_viewer
        // 显式传入 args 和 envs
        if (execve("./my_viewer", args, envs) == -1) {
            perror("execve failed");
            _exit(EXIT_FAILURE);
        }
    } else {
        // --- 父进程 ---
        int status;
        waitpid(pid, &status, 0);
        printf("\n[Parent Context] Child process has finished.\n");
    }

    return 0;
}
