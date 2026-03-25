---   
title: 环境变量的继承与传递：envp 的秘密
tags: [Process, envp, exec, EnvironmentVariables]
desc: 深入解析 exec 族函数调用时环境变量 (envp) 的继承、覆盖与传递机制
update: 2026-03-25

---


# 环境变量的继承与传递：envp 的秘密

在上一节的实战 Demo 中，我们观察到了一个有趣的现象：
我们在 `caller.c` 中只调用了 `execv("./my_viewer", args);`，**并没有传递任何环境变量数组**。然而，当 `my_viewer` 运行起来时，它依然打印出了 `PULSE_SERVER` 或 `HOSTTYPE` 等环境变量。

**这些环境变量是怎么“穿透” `execv` 传递到新程序里的？** 这涉及到 Linux 进程环境管理的底层机制。

## 1. 全局变量 environ 的隐式传递

在 C 标准库 (glibc) 中，如果你使用的是不带 `e` 后缀的 exec 函数（如 `execl`, `execv`, `execlp`, `execvp`），**它们会自动帮你把当前进程（也就是父进程）的所有环境变量，原封不动地传递给新程序**。

这背后依赖于一个在 C 库中声明为全局变量的指针数组：
```c
extern char **environ;
```

当 `caller` 进程被 Shell 启动时，Shell 通过 `execve` 将系统所有的环境变量拷贝到了 `caller` 的内存空间，并让 `environ` 指向它们。

当我们调用 `execv("./my_viewer", args)` 时，glibc 的实现大致相当于：
```c
// execv 的内部逻辑简化
int execv(const char *path, char *const argv[]) {
    // 隐式地拿走了全局的 environ
    return execve(path, argv, environ); 
}
```
**这就是为什么 `my_viewer` 能够看到 `PULSE_SERVER` 和 `LANG`：它继承了 `caller` 的环境，而 `caller` 又继承了终端 Shell 的环境。**

## 2. 显式控制：带 e 后缀的 exec 函数

如果我们要**彻底改变**新程序的环境变量（比如为了安全，在一个干净的环境中运行某个脚本），我们就必须使用带 `e` 后缀的函数，最典型的是底层的 `execve` 或包装函数 `execle` / `execvpe`。

使用这些函数，你可以强制传入自定义的 `envp` 数组，**这将完全覆盖默认的继承行为**。

### 实战代码示例 (自定义 envp)

如果在 `caller.c` 中，我们将 `execv` 替换为 `execve` 并手动传入环境数组：

```c
#include <unistd.h>
#include <stdio.h>

int main() {
    char *args[] = {"my_app", "arg1", NULL};
    
    // 构造一个全新的、极简的环境变量数组
    // 必须以 NULL 结尾！
    char *custom_env[] = {
        "MY_CUSTOM_VAR=HelloIMX6ULL",
        "PATH=/usr/local/bin:/usr/bin:/bin",
        NULL
    };

    // 使用 execve，显式传递自定义环境
    execve("./my_viewer", args, custom_env);
    
    perror("execve failed");
    return 1;
}
```

**预期输出 (`my_viewer` 看到的):**
```text
--- Environment (Top 2) ---
envp[0]: MY_CUSTOM_VAR=HelloIMX6ULL
envp[1]: PATH=/usr/local/bin:/usr/bin:/bin
```
此时，`my_viewer` 运行在一个“孤岛”中，它再也看不到宿主机默认的 `USER`、`LANG` 或 `HOSTTYPE` 等变量了。它的世界里只有你塞给它的这两个变量。

## 3. 在 main 中获取环境变量的两种方式

除了 `int main(int argc, char *argv[], char *envp[])` 中的第三个参数 `envp` 之外，更常用的获取环境变量的方法是使用 C 标准库提供的 API。

- **`getenv(const char *name)`:** 最常用的方式。
  ```c
  char *path = getenv("PATH");
  if (path != NULL) {
      printf("PATH is: %s\n", path);
  }
  ```
- **`putenv()` / `setenv()`:** 在程序运行期间动态修改环境变量。
  **注意:** 动态修改只会影响当前进程及其未来 `fork` 出的子进程，**绝对无法影响父进程（比如运行你的 Shell）**。这就是为什么你在脚本里执行 `export VAR=1`，脚本运行完后，终端里的 `VAR` 依然不存在。

## 4. 安全与隔离启示

在嵌入式开发或服务端开发中，如果你要通过 C 代码启动一个外部的未知脚本（比如 CGI 脚本），**极其强烈建议使用 `execve` 并传入一个经过清洗的、只包含必要项（如安全的 PATH）的 `envp` 数组**。

如果你图省事用 `system("script.sh")` 或 `execv`，黑客可能会在父进程中注入恶意的环境变量（如修改 `LD_PRELOAD` 或 `PATH`），导致子脚本执行被劫持的命令，造成严重的安全漏洞。

> [!note]
> **Ref:** 
> - 《UNIX环境高级编程 (APUE)》第7章 进程环境
> - Linux `man 7 environ`
