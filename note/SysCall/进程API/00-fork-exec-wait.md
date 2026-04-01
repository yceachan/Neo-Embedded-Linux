---
title: 进程 API 详解：fork / exec / wait / exit
tags: [SysCall, Process, fork, exec, wait, exit, Linux-Kernel]
desc: 深入剖析 fork 的内核复制机制、exec 族的地址空间替换、wait 族的回收语义、exit 清理流程及组合模式。
update: 2026-03-26

---


# 进程 API 详解：fork / exec / wait / exit

> [!note]
> **Ref:** `note/虚拟化/程序和进程/03-进程创建的艺术：fork-与写时拷贝-(COW).md` · `note/虚拟化/程序和进程/04-进程生命周期管理` · `fork(2)` · `execve(2)` · `wait(2)`

---

## 1. `fork()` — 进程复制

### 1.1 函数原型

```c
#include <unistd.h>
pid_t fork(void);
```

| 返回值 | 含义 |
|--------|------|
| `> 0` | 父进程，返回值为**子进程 PID** |
| `== 0` | 子进程，从 `fork()` 返回点继续执行 |
| `== -1` | 失败，`errno` 为 `EAGAIN`（进程数超限）或 `ENOMEM` |

### 1.2 内核做了什么

```
父进程调用 fork() 陷入内核
  │
  ├─ 复制 task_struct（含寄存器快照）
  ├─ 复制页表，所有页标记为 CoW（写时拷贝）
  ├─ 复制 fd 表（两个进程共享同一组 file 结构体）
  ├─ 复制信号处理表、信号掩码
  │
  └─ 子进程 task_struct 的 PC 指针 = fork() 系统调用的返回指令
       子进程"醒来"时就在 fork() 返回处，返回值为 0
```

父子进程的分叉点：

```c
pid_t pid = fork();
// ← 父子进程都从这一行之后继续执行
//   区别仅是 pid 的值

if (pid == 0) {
    /* 子进程：pid == 0 */
} else if (pid > 0) {
    /* 父进程：pid == 子进程的 PID */
} else {
    /* fork 失败 */
    perror("fork");
}
```

### 1.3 fd 继承与 CoW 的关键细节

- **fd 表**：子进程继承父进程所有打开的 fd，并共享底层 `file` 结构体（共享文件偏移）。
  - 实践：fork 后应关闭子进程不需要的 fd，防止管道 EOF 延迟（参见 `pipe/03-shell-pipe-sequence.md`）。
- **CoW 内存**：fork 后父子进程共享物理页，任意一方写入时内核才真正复制该页。
  - 实践：fork 后立即 exec，则几乎没有内存拷贝开销（exec 会直接替换地址空间）。
- **stdio 缓冲区**：`printf` 等库函数有用户态缓冲区，fork 时会被一起复制。
  - 实践：fork 之前调用 `fflush(NULL)` 清空缓冲区，避免数据被父子进程各输出一次。

---

## 2. `exec` 族 — 地址空间替换

exec 族函数用新程序**完全替换**当前进程的地址空间，PID 不变。

### 2.1 函数族全览

```c
#include <unistd.h>

// ── 基础：路径 + argv 数组 ──────────────────────────────────────
int execv (const char *path, char *const argv[]);

// ── 带环境变量 ────────────────────────────────────────────────
int execve(const char *path, char *const argv[], char *const envp[]);

// ── 可变参数列表（以 NULL 结尾）──────────────────────────────
int execl (const char *path, const char *arg0, ... /*, NULL */);
int execle(const char *path, const char *arg0, ... /*, NULL, char *const envp[]*/);

// ── 在 PATH 中搜索程序名 ─────────────────────────────────────
int execlp(const char *file, const char *arg0, ... /*, NULL */);
int execvp(const char *file, char *const argv[]);

// ── Linux 扩展：相对于 dirfd 的路径（glibc 2.11+）────────────
int execveat(int dirfd, const char *path, char *const argv[],
             char *const envp[], int flags);
```

**命名规律：**

| 后缀字母 | 含义 |
|---------|------|
| `l` (list) | 参数以可变参数列表传递，末尾用 `NULL` 终止 |
| `v` (vector) | 参数以 `char *argv[]` 数组传递 |
| `p` (path) | 在 `$PATH` 环境变量中搜索可执行文件 |
| `e` (env) | 显式传递 `envp[]`，否则继承当前环境变量 |

### 2.2 execve 是唯一的系统调用

`execl`、`execv`、`execlp`、`execvp`、`execle` 均为 libc 封装，最终都调用 `execve()`。

```c
// 最常用的三种调用方式对比
execv("/bin/ls", (char *[]){"/bin/ls", "-l", NULL});     // 绝对路径 + 数组
execl("/bin/ls", "/bin/ls", "-l", NULL);                  // 绝对路径 + 列表
execvp("ls",     (char *[]){"ls",     "-l", NULL});       // PATH 搜索 + 数组
```

### 2.3 exec 后哪些东西被保留

| 属性 | exec 后保留？ | 说明 |
|------|:---:|------|
| PID / PPID | ✅ | 进程身份不变 |
| 打开的 fd | ✅（默认） | 除非设置了 `O_CLOEXEC` |
| 信号屏蔽字 | ✅ | |
| 进程组 / 会话 | ✅ | |
| 用户态内存（代码/堆/栈） | ❌ | 全部替换为新程序 |
| 信号 handler | ❌ | 全部重置为 `SIG_DFL` |
| `atexit` 注册函数 | ❌ | |
| 线程（除调用线程外） | ❌ | 其他线程被强制终止 |

> `O_CLOEXEC` 的重要性：fork + exec 模式中，父进程打开的 fd（如管道端）若不设置此标志，会被 exec 后的子进程意外继承，造成资源泄漏或 EOF 行为异常。

### 2.4 exec 失败时的行为

exec 成功**永不返回**；失败时返回 `-1` 并设 `errno`：

```c
execv("/nonexistent", argv);
// 只有执行到这里说明 exec 失败
perror("execv");
_exit(127);   // 用 _exit 而非 exit，避免刷新父进程继承的 stdio 缓冲区
```

---

## 3. `wait` 族 — 子进程回收

父进程必须回收子进程，否则子进程变为**僵尸进程**（占用 PID 和 task_struct）。

### 3.1 函数族全览

```c
#include <sys/wait.h>

pid_t wait  (int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
int   waitid (idtype_t idtype, id_t id, siginfo_t *infop, int options);
```

### 3.2 `waitpid` 详解（最常用）

```c
pid_t waitpid(pid_t pid, int *wstatus, int options);
```

**`pid` 参数语义：**

| `pid` 值 | 等待目标 |
|---------|---------|
| `> 0` | 等待 PID 等于 `pid` 的具体子进程 |
| `== 0` | 等待与调用者**同进程组**的任意子进程 |
| `== -1` | 等待**任意**子进程（与 `wait()` 等价） |
| `< -1` | 等待进程组 ID 等于 `|pid|` 的任意子进程 |

**`options` 标志位：**

| 标志 | 含义 |
|------|------|
| `0` | 阻塞，直到子进程状态改变 |
| `WNOHANG` | 非阻塞，若无子进程退出立即返回 `0` |
| `WUNTRACED` | 也报告被信号**暂停**（`SIGSTOP`）的子进程 |
| `WCONTINUED` | 也报告被 `SIGCONT` **继续**的子进程 |

### 3.3 `wstatus` 解码宏

```c
int status;
waitpid(pid, &status, 0);

if (WIFEXITED(status)) {
    printf("正常退出，退出码: %d\n", WEXITSTATUS(status));
}
if (WIFSIGNALED(status)) {
    printf("被信号终止，信号号: %d%s\n",
           WTERMSIG(status),
           WCOREDUMP(status) ? " (core dumped)" : "");
}
if (WIFSTOPPED(status)) {
    printf("被信号暂停，信号号: %d\n", WSTOPSIG(status));
}
```

| 宏 | 含义 |
|----|------|
| `WIFEXITED(s)` | 子进程是否正常调用 `exit()` / `_exit()` 退出 |
| `WEXITSTATUS(s)` | 正常退出时的退出码（低 8 位） |
| `WIFSIGNALED(s)` | 子进程是否被信号终止 |
| `WTERMSIG(s)` | 终止子进程的信号编号 |
| `WCOREDUMP(s)` | 是否产生了 core dump |
| `WIFSTOPPED(s)` | 子进程是否被暂停（需 `WUNTRACED`） |
| `WSTOPSIG(s)` | 暂停子进程的信号编号 |

---

## 4. `exit` 族 — 进程终止

### 4.1 函数族全览

```c
#include <stdlib.h>
void exit(int status);      /* ISO C：完整清理后终止 */
void _Exit(int status);     /* ISO C99：等同 _exit，跳过清理 */

#include <unistd.h>
void _exit(int status);     /* POSIX：直接系统调用，跳过清理 */
```

三者最终都调用内核的 `exit_group` 系统调用终止进程，差别在于**用户态清理流程**是否执行。

### 4.2 清理流程对比

```
exit(status)
  │
  ├─ 1. 按注册逆序调用所有 atexit() / on_exit() 函数
  ├─ 2. fflush() 刷新所有打开的 stdio 缓冲区
  ├─ 3. fclose() 关闭所有 stdio 流
  ├─ 4. 删除 tmpfile() 创建的临时文件
  │
  └─ _exit(status)  ← 内部最终调用
        │
        └─ exit_group 系统调用
              ├─ 关闭所有 fd
              ├─ 释放内存页 / 页表
              ├─ 向父进程发 SIGCHLD
              └─ 进程状态置为 EXIT_ZOMBIE，等待 wait()
```

| | `exit()` | `_exit()` / `_Exit()` |
|---|:---:|:---:|
| `atexit` 回调 | ✅ 执行 | ❌ |
| stdio `fflush` | ✅ 执行 | ❌ |
| stdio `fclose` | ✅ 执行 | ❌ |
| 临时文件清理 | ✅ 执行 | ❌ |
| 内核资源回收（fd、内存）| ✅ | ✅ |
| async-signal-safe | ❌ | ✅ |

### 4.3 `atexit` — 注册退出回调

```c
#include <stdlib.h>
int atexit(void (*func)(void));
```

- 最多可注册 `ATEXIT_MAX`（至少 32）个函数。
- **逆序执行**（后注册先调用），类似析构语义。
- 只在 `exit()` 或 `main()` 正常 return 时触发，`_exit()` / 信号终止不触发。

```c
void cleanup_db(void)  { printf("closing db\n"); }
void cleanup_log(void) { printf("closing log\n"); }

int main(void) {
    atexit(cleanup_db);     /* 第 1 个注册 */
    atexit(cleanup_log);    /* 第 2 个注册 */
    exit(0);
    /* 输出顺序：closing log → closing db（逆序） */
}
```

### 4.4 退出码约定

`status` 只有低 8 位传递给父进程（`WEXITSTATUS` 取值范围 0–255）：

| 退出码 | 约定含义 |
|--------|---------|
| `0` | 成功 |
| `1` | 通用错误 |
| `2` | 参数/用法错误（Shell 惯例） |
| `127` | 命令未找到（`exec` 失败的 Shell 惯例） |
| `128+N` | 被信号 N 终止（Shell 计算方式，非内核） |

### 4.5 何时用 `_exit()` 而非 `exit()`

**fork 后的子进程（未 exec）中必须用 `_exit()`：**

```c
pid_t pid = fork();
if (pid == 0) {
    do_something();
    _exit(0);   /* ← 不能用 exit() */
}
```

原因：子进程继承了父进程的 stdio 缓冲区副本，`exit()` 会调用 `fflush()` 将缓冲区内容**再输出一次**，造成数据重复。

**signal handler 中必须用 `_exit()`：**

```
主流程: printf() → 持有 stdio 内部锁
            ↓ 信号打断
handler:  exit() → fflush() → 尝试获取同一把锁 → 死锁
```

`_exit()` 直接发起系统调用，不触碰任何用户态锁，是 async-signal-safe 的。

**规则总结：**

| 场景 | 使用 |
|------|------|
| `main()` 正常退出 | `exit()` 或 `return` |
| fork 后子进程（未 exec）| `_exit()` |
| signal handler 中退出 | `_exit()` |
| exec 失败后 | `_exit(127)` |

---

## 6. 标准组合模式

### 4.1 fork + exec（spawn 子进程执行新程序）

```c
pid_t pid = fork();
if (pid == 0) {
    /* 子进程 */
    execvp("ls", (char *[]){"ls", "-l", NULL});
    perror("execvp");   /* exec 失败才会到这里 */
    _exit(127);
} else if (pid > 0) {
    /* 父进程 */
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        printf("ls 退出码: %d\n", WEXITSTATUS(status));
} else {
    perror("fork");
}
```

### 4.2 SIGCHLD + 非阻塞 waitpid（异步回收，防僵尸）

父进程不阻塞等待，而是在 `SIGCHLD` handler 中异步回收：

```c
static void reap_children(int sig)
{
    (void)sig;
    int status;
    pid_t pid;
    /* 循环回收所有已退出的子进程，防止信号合并导致遗漏 */
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        /* 可记录 pid 和 status */
    }
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = reap_children,
        .sa_flags   = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    /* 主循环中正常 fork 子进程，无需阻塞等待 */
}
```

### 4.3 fork + exec + pipe（捕获子进程输出）

参见 `note/虚拟化/进程通信IPC/pipe/04-embedded-usecases.md` §2。

---

## 7. 常见陷阱速查

| 陷阱 | 原因 | 解决 |
|------|------|------|
| fork 后 printf 输出两遍 | stdio 缓冲区被 CoW 复制 | fork 前 `fflush(NULL)` |
| 管道读端永不 EOF | 父进程未关闭自持的写端 | fork 后父进程 `close(fd[1])` |
| exec 后 fd 泄漏 | 未设置 `O_CLOEXEC` | `pipe2(fd, O_CLOEXEC)` / `open(..., O_CLOEXEC)` |
| 僵尸进程堆积 | 父进程未 `wait()` | 注册 `SIGCHLD` 异步回收 |
| signal handler 死锁 | handler 内调用 `exit()` | 改用 `_exit()` |
| exec 后 handler 失效 | exec 重置所有 handler 为 `SIG_DFL` | exec 后重新注册（新程序 `main` 里） |
