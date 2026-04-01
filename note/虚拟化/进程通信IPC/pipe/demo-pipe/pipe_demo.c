/**
 * pipe_demo.c
 * 演示匿名管道的三个核心场景：
 *   1. 父写子读（基础用法）
 *   2. 非阻塞模式 + EAGAIN 处理
 *   3. SIGPIPE 触发（读端关闭后写入）
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

/* ── 场景 1：父进程写，子进程读 ─────────────────────────────── */
void demo_basic_pipe(void)
{
    puts("\n=== Demo 1: 基础父写子读 ===");

    int fd[2];
    if (pipe2(fd, O_CLOEXEC) == -1) {
        perror("pipe2");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); exit(1); }

    if (pid == 0) {
        /* 子进程：读端 */
        close(fd[1]);                   /* 关闭写端，防止永久阻塞 */
        char buf[128] = {0};
        ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
        printf("[Child  pid=%d] read %zd bytes: \"%s\"\n", getpid(), n, buf);
        close(fd[0]);
        exit(0);
    } else {
        /* 父进程：写端 */
        close(fd[0]);                   /* 关闭读端 */
        const char *msg = "Hello from parent!";
        write(fd[1], msg, strlen(msg));
        printf("[Parent pid=%d] wrote: \"%s\"\n", getpid(), msg);
        close(fd[1]);                   /* 关闭写端，子进程 read 返回 EOF */
        wait(NULL);
    }
}

/* ── 场景 2：非阻塞模式，缓冲区空时返回 EAGAIN ──────────────── */
void demo_nonblock_pipe(void)
{
    puts("\n=== Demo 2: 非阻塞模式 (O_NONBLOCK) ===");

    int fd[2];
    pipe2(fd, O_NONBLOCK | O_CLOEXEC);

    /* 不写入任何数据，直接尝试读取 */
    char buf[64];
    ssize_t n = read(fd[0], buf, sizeof(buf));
    if (n == -1 && errno == EAGAIN)
        puts("[NonBlock] read on empty pipe → EAGAIN (正确行为，非阻塞返回)");
    else
        printf("[NonBlock] unexpected: n=%zd errno=%d\n", n, errno);

    /* 写入数据后再读 */
    write(fd[1], "data", 4);
    n = read(fd[0], buf, sizeof(buf));
    printf("[NonBlock] after write, read %zd bytes: \"%.*s\"\n", n, (int)n, buf);

    close(fd[0]);
    close(fd[1]);
}

/* ── 场景 3：SIGPIPE — 读端关闭后写入 ──────────────────────── */
static volatile int sigpipe_received = 0;
static void sigpipe_handler(int sig) { (void)sig; sigpipe_received = 1; }

void demo_sigpipe(void)
{
    puts("\n=== Demo 3: SIGPIPE (读端关闭后写入) ===");

    /***********************
     * @notes:
     * 不捕获时默认行为 (SIG_DFL)：
     * 内核在 pipe_write() 检测到 readers==0 后投递 SIGPIPE，
     * 进程在 write() 的返回路径上被直接终止，write() 永不返回。
     * 退出码 141 (128+13，13 为 SIGPIPE 编号)。
     * 亦可用 signal(SIGPIPE, SIG_IGN) 忽略，使 write() 返回 -1/EPIPE。
     ***********************/
    signal(SIGPIPE, sigpipe_handler);

    int fd[2];
    pipe2(fd, O_CLOEXEC);

    close(fd[0]);                       /* 主动关闭读端 */

    ssize_t n = write(fd[1], "oops", 4);
    if (n == -1 && errno == EPIPE)
        printf("[SIGPIPE] write returned -1 (EPIPE), sigpipe_received=%d\n",
               sigpipe_received);

    close(fd[1]);
}

int main(void)
{
    demo_basic_pipe();
    demo_nonblock_pipe();
    demo_sigpipe();
    return 0;
}
