#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

/**
 * @brief 信号处理函数
 * @param sig 信号编号
 * @param info 包含发送者信息的 siginfo_t 结构体
 * @param context 上下文信息
 */
void handle_sigusr1(int sig, siginfo_t *info, void *context) {
    printf("\n[Signal] Received SIGUSR1 (%d)\n", sig);
    printf("[Signal] Sender PID: %d\n", info->si_pid);
    printf("[Signal] Sender UID: %d\n", info->si_uid);
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    struct sigaction sa;

    // 清空结构体
    memset(&sa, 0, sizeof(sa));

    // 使用 SA_SIGINFO 标志以获取发送者信息
    sa.sa_sigaction = handle_sigusr1;
    sa.sa_flags = SA_SIGINFO;

    // 在处理 SIGUSR1 时，阻塞 SIGINT (Ctrl+C)
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);

    printf("My PID is: %d\n", getpid());
    printf("Registering SIGUSR1 handler...\n");

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for SIGUSR1. Use 'kill -USR1 %d' to trigger.\n", getpid());
    printf("Press Ctrl+C (SIGINT) to exit (it's not blocked here).\n");

    while (1) {
        pause(); // 等待任何信号
    }

    return 0;
}
