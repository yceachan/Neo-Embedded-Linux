---
title: SysV 信号量 API 矩阵
tags: [semaphore, SysV, IPC, semget, semop, semctl]
desc: SysV 信号量完整 API 速查，信号量数组、semop 原子操作及与 POSIX 的对比
update: 2026-04-01

---


# SysV 信号量 API 矩阵

> [!note]
> **Ref:** `man semget(2)`, `man semop(2)`, `man semctl(2)`, `man ipcs(1)`
> 头文件：`<sys/types.h>`, `<sys/ipc.h>`, `<sys/sem.h>`

---

## 1. SysV IPC 三件套回顾

| 机制 | 创建 | 操作 | 控制 |
|------|------|------|------|
| 消息队列 | msgget | msgsnd / msgrcv | msgctl |
| 共享内存 | shmget | shmat / shmdt | shmctl |
| **信号量** | **semget** | **semop** | **semctl** |

所有 SysV IPC 对象都是**内核持久**的，进程退出后仍存在，需显式删除（`IPC_RMID`）或重启系统。

---

## 2. API 详解

### 2.1 semget — 创建/获取信号量集

```c
int semget(key_t key, int nsems, int semflg);
```

| 参数 | 说明 |
|------|------|
| `key` | IPC_PRIVATE（私有）或 ftok() 生成的键值 |
| `nsems` | 信号量**数组**的大小（SysV 特色：可同时操作多个） |
| `semflg` | IPC_CREAT \| IPC_EXCL \| 权限位（如 0666） |

返回：`semid`（信号量集描述符），失败返回 -1。

```c
// 示例：创建含 2 个信号量的集合
key_t key = ftok("/tmp/myapp", 'S');
int semid = semget(key, 2, IPC_CREAT | IPC_EXCL | 0666);
```

### 2.2 semctl — 控制与初始化

```c
int semctl(int semid, int semnum, int cmd, .../*union semun arg*/);
```

常用 `cmd`：

| cmd | semnum | 说明 |
|-----|--------|------|
| `SETVAL` | 目标下标 | 设置单个信号量初值（需传 `union semun`） |
| `SETALL` | 忽略 | 批量设置所有信号量值（传 `unsigned short[]`） |
| `GETVAL` | 目标下标 | 读取当前值 |
| `GETALL` | 忽略 | 批量读取 |
| `IPC_RMID` | 忽略 | 删除整个信号量集 |
| `IPC_STAT` | 忽略 | 读取 `struct semid_ds` 元数据 |

```c
// 必须自行定义 union semun（Linux 不在头文件中提供）
union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

// 初始化第 0 个信号量为 1（互斥锁）
union semun arg = { .val = 1 };
semctl(semid, 0, SETVAL, arg);
```

### 2.3 semop — 原子 P/V 操作（核心）

```c
int semop(int semid, struct sembuf *sops, size_t nsops);
```

`struct sembuf` 描述单次操作：

```c
struct sembuf {
    unsigned short sem_num;   // 信号量下标（0 起）
    short          sem_op;    // 操作值：负=P，正=V，0=等待归零
    short          sem_flg;   // IPC_NOWAIT | SEM_UNDO
};
```

| sem_op | 行为 |
|--------|------|
| `< 0` | P 操作：`S += sem_op`（减少），S<0 时阻塞 |
| `> 0` | V 操作：`S += sem_op`（增加），唤醒等待者 |
| `= 0` | 等待信号量归零（用于屏障同步） |

**SysV 信号量的核心优势**：`nsops > 1` 时，对**多个信号量的操作是原子的**，要么全部成功要么全部阻塞 — 这是 POSIX sem 做不到的。

```c
// 原子 P 两个信号量（避免死锁的经典手法）
struct sembuf ops[2] = {
    { .sem_num = 0, .sem_op = -1, .sem_flg = 0 },
    { .sem_num = 1, .sem_op = -1, .sem_flg = 0 },
};
semop(semid, ops, 2);   // 要么同时获得两个，要么等待
```

---

## 3. SEM_UNDO — 进程崩溃自动恢复

```c
struct sembuf op = { 0, -1, SEM_UNDO };
semop(semid, &op, 1);
```

设置 `SEM_UNDO` 后，内核为进程记录**撤销调整量**。进程异常退出时，内核自动执行反向操作，防止信号量被永久锁死。

嵌入式场景中非常有用：守护进程崩溃后系统仍可自愈。

---

## 4. 完整示例：父子进程互斥访问共享内存

```c
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdio.h>
#include <unistd.h>

union semun { int val; struct semid_ds *buf; unsigned short *array; };

static void sem_p(int semid) {
    struct sembuf op = { 0, -1, SEM_UNDO };
    semop(semid, &op, 1);
}

static void sem_v(int semid) {
    struct sembuf op = { 0, +1, SEM_UNDO };
    semop(semid, &op, 1);
}

int main(void) {
    // 创建共享内存
    int shmid = shmget(IPC_PRIVATE, 4096, IPC_CREAT | 0600);
    int *shared = shmat(shmid, NULL, 0);
    *shared = 0;

    // 创建信号量，初值 1
    int semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    union semun arg = { .val = 1 };
    semctl(semid, 0, SETVAL, arg);

    pid_t pid = fork();
    if (pid == 0) {
        // 子进程
        for (int i = 0; i < 10000; i++) {
            sem_p(semid);
            (*shared)++;
            sem_v(semid);
        }
        shmdt(shared);
        return 0;
    }

    // 父进程
    for (int i = 0; i < 10000; i++) {
        sem_p(semid);
        (*shared)++;
        sem_v(semid);
    }
    wait(NULL);
    printf("shared = %d\n", *shared);  // 期望 20000

    // 清理
    shmdt(shared);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}
```

---

## 5. 系统工具

```bash
ipcs -s           # 查看所有 SysV 信号量集
ipcs -s -i <id>   # 查看指定 semid 的详细信息
ipcrm -s <semid>  # 手动删除泄漏的信号量集
```

---

## 6. POSIX vs SysV 决策树

```
需要操作多个信号量的原子性？
    └─ 是 → SysV semop(nsops > 1)

只需要简单 P/V？
    ├─ 线程间 → POSIX sem_init(pshared=0)
    └─ 进程间 → POSIX sem_open("/name")

需要进程崩溃自动释放锁？
    └─ SysV SEM_UNDO 或 POSIX robust mutex
```

---

## 小结

- SysV 信号量的本质是**信号量数组** + **原子 semop**
- `SEM_UNDO` 是嵌入式场景防死锁的重要机制
- 必须显式 `semctl(IPC_RMID)` 清理，否则重启前一直存在
- 多信号量原子操作是 SysV 相对 POSIX 的唯一真实优势
