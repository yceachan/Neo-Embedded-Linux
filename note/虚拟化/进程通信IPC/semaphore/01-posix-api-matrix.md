---
title: POSIX 信号量 API 矩阵
tags: [semaphore, POSIX, API, sem_init, sem_open, sem_wait, sem_post]
desc: POSIX 信号量完整 API 速查，含匿名/命名信号量的使用场景与典型代码
update: 2026-04-01

---


# POSIX 信号量 API 矩阵

> [!note]
> **Ref:** `man sem_overview(7)`, `man sem_init(3)`, `man sem_open(3)`
> 编译需链接：`-lpthread` （glibc 已内置 sem 实现）

---

## 1. 两类信号量选择

```
需要线程间同步？
    └─ sem_init(pshared=0)    匿名信号量，栈/堆上分配

需要无关进程间同步？
    ├─ sem_open("/name")      命名信号量，/dev/shm 下有对应文件
    └─ sem_init(pshared=1)    匿名 + mmap(MAP_SHARED)，适合 fork 后父子进程
```

---

## 2. API 全览

### 2.1 匿名信号量

| 函数 | 签名 | 说明 |
|------|------|------|
| `sem_init` | `int sem_init(sem_t *sem, int pshared, unsigned value)` | 初始化；pshared=0 线程共享，pshared=1 进程共享（需 shared mem） |
| `sem_destroy` | `int sem_destroy(sem_t *sem)` | 销毁，释放内核资源 |
| `sem_wait` | `int sem_wait(sem_t *sem)` | P 操作，阻塞直到 S>0，然后 S-=1 |
| `sem_trywait` | `int sem_trywait(sem_t *sem)` | 非阻塞 P，S=0 时返回 EAGAIN |
| `sem_timedwait` | `int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)` | 带超时的 P，超时返回 ETIMEDOUT |
| `sem_post` | `int sem_post(sem_t *sem)` | V 操作，S+=1 并唤醒一个等待者 |
| `sem_getvalue` | `int sem_getvalue(sem_t *sem, int *sval)` | 读取当前计数（仅作参考，读后可能已变） |

### 2.2 命名信号量

| 函数 | 签名 | 说明 |
|------|------|------|
| `sem_open` | `sem_t *sem_open(const char *name, int oflag, ...)` | 创建或打开；name 以 `/` 开头 |
| `sem_close` | `int sem_close(sem_t *sem)` | 关闭本进程的引用，不删除 |
| `sem_unlink` | `int sem_unlink(const char *name)` | 删除内核对象（类似 unlink 文件） |

> `sem_wait` / `sem_post` / `sem_getvalue` 对命名/匿名信号量通用。

---

## 3. 典型用法

### 3.1 互斥锁（binary semaphore）

```c
#include <semaphore.h>
#include <pthread.h>

sem_t mutex;
int counter = 0;

void *worker(void *arg) {
    for (int i = 0; i < 10000; i++) {
        sem_wait(&mutex);   // P：进入临界区
        counter++;
        sem_post(&mutex);   // V：离开临界区
    }
    return NULL;
}

int main(void) {
    sem_init(&mutex, 0, 1);   // 初值 1 = 互斥锁语义

    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    sem_destroy(&mutex);
    printf("counter = %d\n", counter);  // 期望 20000
}
```

### 3.2 生产者-消费者（计数信号量）

```c
#include <semaphore.h>
#include <pthread.h>
#include <stdio.h>

#define BUF_SIZE 8
int buf[BUF_SIZE];
int in = 0, out = 0;

sem_t empty;   // 空槽数量，初值 BUF_SIZE
sem_t full;    // 满槽数量，初值 0
sem_t mutex;   // 保护 buf 索引

void *producer(void *arg) {
    for (int i = 0; i < 20; i++) {
        sem_wait(&empty);          // 等待空槽
        sem_wait(&mutex);
        buf[in] = i;
        in = (in + 1) % BUF_SIZE;
        sem_post(&mutex);
        sem_post(&full);           // 通知消费者
    }
    return NULL;
}

void *consumer(void *arg) {
    for (int i = 0; i < 20; i++) {
        sem_wait(&full);           // 等待有数据
        sem_wait(&mutex);
        int val = buf[out];
        out = (out + 1) % BUF_SIZE;
        sem_post(&mutex);
        sem_post(&empty);          // 归还空槽
        printf("consumed: %d\n", val);
    }
    return NULL;
}

int main(void) {
    sem_init(&empty, 0, BUF_SIZE);
    sem_init(&full,  0, 0);
    sem_init(&mutex, 0, 1);

    pthread_t p, c;
    pthread_create(&p, NULL, producer, NULL);
    pthread_create(&c, NULL, consumer, NULL);
    pthread_join(p, NULL);
    pthread_join(c, NULL);

    sem_destroy(&empty);
    sem_destroy(&full);
    sem_destroy(&mutex);
}
```

### 3.3 命名信号量（跨进程）

```c
// 进程 A：创建并初始化
sem_t *sem = sem_open("/myapp_ready", O_CREAT | O_EXCL, 0644, 0);
// ... 做初始化工作 ...
sem_post(sem);     // 通知进程 B 可以开始
sem_close(sem);

// 进程 B：等待进程 A 就绪
sem_t *sem = sem_open("/myapp_ready", 0);
sem_wait(sem);     // 阻塞直到 A post
sem_close(sem);
sem_unlink("/myapp_ready");   // 清理
```

---

## 4. 错误处理要点

| 函数 | 常见错误 | 原因 |
|------|----------|------|
| `sem_init` | `EINVAL` | value > SEM_VALUE_MAX |
| `sem_wait` | `EINTR` | 被信号中断，需重试 |
| `sem_timedwait` | `ETIMEDOUT` | 超时未获得信号量 |
| `sem_open` | `ENOENT` | 未指定 O_CREAT 且名字不存在 |
| `sem_open` | `EEXIST` | O_CREAT\|O_EXCL 但已存在 |

```c
// sem_wait 正确的信号中断处理
while (sem_wait(&sem) == -1) {
    if (errno == EINTR) continue;   // 信号中断，重试
    perror("sem_wait"); exit(1);
}
```

---

## 5. sem_t 内存布局（glibc 实现）

```
sem_t (32 bytes on ARM):
┌────────────────────────────────┐
│ unsigned int value             │  ← 计数器（低31位）+ waiter标志位（bit31）
│ int private                    │  ← pshared 标志
│ [padding]                      │
└────────────────────────────────┘

sem_post 无竞争路径（全用户态）：
  atomic_increment(sem->value)
  if (old_value & WAITER_BIT):
    futex(FUTEX_WAKE, 1)     ← 只有存在等待者才进内核
```

---

## 小结

- **匿名 sem_init**：线程间首选，简单高效
- **命名 sem_open**：无亲缘关系的进程间同步
- **sem_wait 注意 EINTR**，循环重试是惯用法
- 生产者-消费者经典模型：`empty` + `full` + `mutex` 三信号量组合
