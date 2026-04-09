---
title: 进程生命周期管理：fork、exec 与 wait 族详解
tags: [Process, fork, exec, wait, SystemCall]
desc: 详细讲解 Linux 进程管理的核心 API 族及其实战组合模式
update: 2026-03-25

---


# 进程生命周期管理：fork、exec 与 wait 族详解

在 Windows 中，创建一个进程通常只用一个庞大的 API（`CreateProcess`）。而 Linux 采用了充满哲学意味的**分离设计**：将进程的创建（`fork`）、程序的加载（`exec`）和状态的同步（`wait`）完全解耦。

这种设计使得 Linux 终端（Shell）和重定向机制的实现变得极其优雅。

## 1. 核心铁三角：Linux 进程的生命流转

Linux 进程的一生，通常围绕着这三个系统调用展开：

1.  **`fork()`**: 创造新生命（复制当前状态）。
2.  **`exec()` 族**: 赋予新灵魂（加载新程序替换旧程序）。
3.  **`wait()` 族**: 父辈的凝视与收割（等待子进程结束并回收资源）。

## 2. 分身：fork() 及其返回值

正如前一篇关于 COW 的笔记所述，`fork()` 的代价极小。从开发者的角度看，`fork()` 最神奇的地方在于：**调用一次，返回两次**。

```c
pid_t pid = fork();

if (pid < 0) {
    // 失败（如内存不足，或达到最大进程数限制）
} else if (pid == 0) {
    // 在【子进程】中，pid 为 0
    // 这里是子进程的逻辑
} else {
    // 在【父进程】中，pid 大于 0，表示子进程的真实的 PID
    // 这里是父进程的逻辑
}
```

## 3. 夺舍：exec 族函数

当子进程被 `fork` 出来后，如果只想让它做与父进程不一样的事，用 `if (pid == 0)` 分支即可。但如果想让它运行一个**全新的程序**（比如 `ls` 或 `hello_world`），就需要用 `exec` 族函数。

**核心机制：** `exec` 会清空当前进程的代码段、数据段、堆和栈，然后从磁盘加载新的可执行文件。**但是，进程的 PID 不会变，打开的文件描述符（通常）也不会关。**

### 记忆法则 (6个前端包装 + 1个底层调用)
最底层的系统调用只有 `execve()`，其他都是 glibc 提供的包装函数。它们的后缀有特定含义：

- **`l` (List):** 参数以列表形式传入（可变参数），以 `NULL` 结尾。如 `execl("/bin/ls", "ls", "-l", NULL);`
- **`v` (Vector):** 参数以字符串数组（指针数组）形式传入。如 `execv("/bin/ls", argv_array);`
- **`p` (Path):** 自动在环境变量 `$PATH` 中寻找可执行文件。无需写全路径。如 `execlp("ls", "ls", "-l", NULL);`
- **`e` (Environment):** 允许传入一个自定义的环境变量数组。如 `execve(path, argv, envp);`

## 4. 善后与收割：wait() 与 waitpid()

当子进程调用 `exit()` 退出时，它并没有完全消失，而是变成了一个**僵尸进程 (Zombie, `EXIT_ZOMBIE`)**。它释放了内存，但还在内核的进程表里占着一个 PID，保留着退出码供父进程查阅。

如果父进程不“收割”它，久而久之 PID 就会耗尽。

### wait()
- **阻塞调用：** `pid_t wait(int *wstatus);`
- **行为：** 父进程会一直阻塞（睡眠），直到它的**任意一个**子进程结束。它返回结束的子进程的 PID，并将退出状态码填入 `wstatus`。

### waitpid() (更强大的升级版)
- **原型：** `pid_t waitpid(pid_t pid, int *wstatus, int options);`
- **精准狙击：** 可以指定等待某个具体的 PID（`pid > 0`），而不是任意一个。
- **非阻塞模式：** 传入 `options = WNOHANG`，如果有子进程退出就收割并返回 PID；如果没有，**立即返回 0**，父进程可以继续干别的事（常结合信号 `SIGCHLD` 使用）。

## 5. 终结：exit() vs _exit()

- **`_exit(int status)`:** 系统调用。直接陷入内核，无情抹杀进程。
- **`exit(int status)`:** C 标准库函数。在调用 `_exit` 之前，会**先刷新标准 I/O 缓冲区**（比如把你 `printf` 还没输出的内容写到终端），并调用由 `atexit()` 注册的清理函数。
- **最佳实践:** 父进程通常用 `exit()`，而由 `fork()` 产生的**子进程如果遇到错误需要中途退出，强烈建议使用 `_exit()`**，以防它不小心刷新了从父进程继承来的缓冲数据。

## 6. 标准实战模板

下面是 Linux 系统编程中最经典的“执行外部命令”的组合拳：

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // --- 子进程上下文 ---
        printf("Child (PID: %d) executing 'ls -l'...\n", getpid());
        
        // 使用 execlp 自动搜索 PATH，并传入参数列表
        execlp("ls", "ls", "-l", NULL);
        
        // 【注意】如果 exec 成功，程序就变成 ls 了，下面的代码永远不会执行
        // 如果执行到这里，说明 exec 失败了！
        perror("exec failed");
        _exit(EXIT_FAILURE); // 使用 _exit 安全退出子进程
    } else {
        // --- 父进程上下文 ---
        printf("Parent (PID: %d) waiting for child...\n", getpid());
        
        int status;
        pid_t wpid = waitpid(pid, &status, 0); // 阻塞等待特定子进程
        
        if (wpid == -1) {
            perror("waitpid failed");
        } else if (WIFEXITED(status)) { // 宏：判断是否正常退出
            printf("Child %d exited normally with status %d\n", wpid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) { // 宏：判断是否被信号杀掉（如段错误、kill）
            printf("Child %d killed by signal %d\n", wpid, WTERMSIG(status));
        }
    }

    return 0;
}
```

> [!note]
> **Ref:** 
> - 《UNIX环境高级编程 (APUE)》第8章 进程控制
> - Linux `man 2 fork`, `man 3 exec`, `man 2 wait`
