---
title: 原子操作与内存屏障 (ARM Cortex-A7 视角)
tags: [kernel, sync, atomic, memory-barrier, arm, dmb, lock-free]
desc: atomic_t API、ARM dmb/dsb/isb、smp_mb/rmb/wmb、ACQUIRE/RELEASE 与典型 lock-free 模式
update: 2026-04-07

---


# 原子操作与内存屏障 (ARM Cortex-A7 视角)

> [!note]
> **Ref:**
> - [`include/linux/atomic.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/atomic.h)
> - [`arch/arm/include/asm/atomic.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/arch/arm/include/asm/atomic.h)
> - [`arch/arm/include/asm/barrier.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/arch/arm/include/asm/barrier.h)
> - [`include/asm-generic/barrier.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/asm-generic/barrier.h)
> - [`Documentation/memory-barriers.txt`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/Documentation/memory-barriers.txt)
> - [`include/linux/compiler.h`](../../../sdk/100ask_imx6ull-sdk/Linux-4.9.88/include/linux/compiler.h) `READ_ONCE/WRITE_ONCE`


## 1. 三类"乱序"

写并发代码必须区分三种独立的乱序：

1. **编译器乱序**：优化器为了寄存器分配、CSE，把内存访问指令重排或合并。`barrier()`（`asm volatile("" ::: "memory")`）和 `READ_ONCE/WRITE_ONCE` 阻止它。
2. **CPU 乱序执行**：现代 CPU 流水线允许同一 CPU 后发的 load/store **早于** 前发的完成。Cortex-A7 是 in-order 双发射，但仍然有 store buffer 与 load 缓冲。
3. **多核内存模型可见性**：CPU0 写出的数据何时被 CPU1 看到，取决于缓存一致性协议 + 内存屏障的传播。

ARM 是 **弱内存模型**：不同地址的 load/store 几乎可以任意重排，除非你显式插屏障。x86 相对强（除 store-load 重排外大都保序）。**为 ARM 写的代码若假设 x86 强模型，几乎一定有 bug。**


## 2. ARM 屏障指令三件套

`arch/arm/include/asm/barrier.h`：

```c
#define isb(option) __asm__ __volatile__ ("isb " #option : : : "memory")
#define dsb(option) __asm__ __volatile__ ("dsb " #option : : : "memory")
#define dmb(option) __asm__ __volatile__ ("dmb " #option : : : "memory")
```

| 指令 | 含义                                | 用途                                 |
| ---- | ----------------------------------- | ------------------------------------ |
| `dmb`| Data Memory Barrier                 | 保证屏障前后的内存访问顺序           |
| `dsb`| Data Synchronization Barrier        | dmb + 等所有 in-flight 内存事务完成  |
| `isb`| Instruction Synchronization Barrier | 冲刷流水线，保证后续指令重新取指     |

`dmb` 还可以加 **共享域参数**：

- `ish` / `ishst`：Inner Shareable，多核 SMP 内（内核 SMP 屏障用这个）
- `osh` / `oshst`：Outer Shareable，含外设的更大域（DMA 屏障）
- `sy`：System，最强（最慢）

对应到 Linux 通用屏障（同文件 49-63 行）：

```c
#define rmb()       dsb()
#define wmb()       dsb(st)
#define dma_rmb()   dmb(osh)
#define dma_wmb()   dmb(oshst)
#define __smp_mb()  dmb(ish)
#define __smp_rmb() __smp_mb()
#define __smp_wmb() dmb(ishst)
```

注意 ARM 上 `rmb/wmb` 用 `dsb`（强保证含 IO），而 SMP 屏障用 `dmb(ish)`（只跨 CPU）。MMIO 寄存器访问应使用 `wmb()/rmb()` 或 `readl/writel`（内置屏障），**不要**用 `smp_*`。


## 3. Linux 屏障 API 速查

| API                              | 编译屏障 | CPU 屏障 (UP/SMP)         | 用途               |
| -------------------------------- | -------- | ------------------------- | ------------------ |
| `barrier()`                      | 是       | 无                        | 仅防编译器乱序     |
| `READ_ONCE(x) / WRITE_ONCE(x,v)` | 是       | 无                        | 防编译器折叠/撕裂  |
| `smp_mb()`                       | 是       | 全屏障 (`dmb ish`)，UP=barrier | 通用 SMP 屏障  |
| `smp_rmb()` / `smp_wmb()`        | 是       | 读/写屏障                 | 单向屏障           |
| `smp_load_acquire(p)`            | 是       | ACQUIRE 语义              | 单读 + 后续不上越  |
| `smp_store_release(p,v)`         | 是       | RELEASE 语义              | 前面不下越 + 单写  |
| `mb()` / `rmb()` / `wmb()`       | 是       | 含 IO 的强屏障 (`dsb`)    | 与 MMIO/DMA 交互   |

ACQUIRE/RELEASE 是比 `smp_mb()` 更精细的"半屏障"：

```c
/* RELEASE: 屏障之前的所有 load/store 不能下移到这次写之后 */
smp_store_release(&flag, 1);

/* ACQUIRE: 这次读之后的所有 load/store 不能上移到读之前 */
if (smp_load_acquire(&flag))
    use(data);
```

二者配对就是经典的 **publish-subscribe** 模式（publisher 用 release，subscriber 用 acquire），相当于用最小代价实现 message-passing 一致性。spinlock、mutex 的 lock/unlock 内部就是 ACQUIRE/RELEASE。


## 4. atomic_t API

`atomic_t` 是 32 位有符号原子整型，由 ARM `ldrex/strex` 实现 LL/SC（Load-Link/Store-Conditional）：

```c
// arch/arm/include/asm/atomic.h (概念)
static inline void atomic_add(int i, atomic_t *v)
{
    int result, tmp;
    __asm__ __volatile__("1:  ldrex   %0, [%3]\n"
                         "    add     %0, %0, %4\n"
                         "    strex   %1, %0, [%3]\n"
                         "    teq     %1, #0\n"
                         "    bne     1b"
                         : "=&r"(result), "=&r"(tmp), "+Qo"(v->counter)
                         : "r"(&v->counter), "Ir"(i) : "cc");
}
```

`strex` 失败（中间被别的 CPU 改过）就重试，构成无锁原子修改。

### 4.1 三类 API 对照

| 类别                 | 函数                              | 语义                          |
| -------------------- | --------------------------------- | ----------------------------- |
| 无返回值             | `atomic_set`, `atomic_add`, `atomic_inc`, `atomic_dec` | 仅修改，**不带屏障**          |
| 返回旧/新值          | `atomic_add_return`, `atomic_inc_return` | 带 **全屏障**                 |
| Test-and-modify      | `atomic_dec_and_test`, `atomic_cmpxchg`, `atomic_xchg` | 带 **全屏障**                 |
| `_relaxed` 后缀      | `atomic_inc_return_relaxed` 等   | **不带屏障**，性能极致        |
| `_acquire/_release`  | `atomic_*_acquire/_release`      | 单向屏障                      |

**铁律**：`atomic_inc()` **不保证其他 CPU 立刻看到结果**，它只保证修改是原子的。如果你需要"先发布数据再让别人看到计数变化"，必须显式 `smp_wmb()` 或使用 `atomic_inc_return()`。

### 4.2 常用 API 一览

```c
atomic_t cnt = ATOMIC_INIT(0);

atomic_set(&cnt, 5);
atomic_inc(&cnt);
atomic_dec(&cnt);
int v = atomic_read(&cnt);                    /* READ_ONCE 包装 */
int n = atomic_add_return(10, &cnt);          /* 全屏障 */
if (atomic_dec_and_test(&cnt)) { /* 归零分支 */ }
int old = atomic_cmpxchg(&cnt, exp, new);     /* CAS */

/* 64-bit */
atomic64_t big = ATOMIC64_INIT(0);
atomic64_inc(&big);

/* 位操作 */
set_bit(3, &flags); clear_bit(3, &flags);
test_and_set_bit(3, &flags);                  /* 全屏障 */
```


## 5. READ_ONCE / WRITE_ONCE：最低成本的"防编译器"

```c
// include/linux/compiler.h
#define READ_ONCE(x)    (*(const volatile typeof(x) *)&(x))
#define WRITE_ONCE(x,v) (*(volatile typeof(x) *)&(x) = (v))
```

它们 **不生成任何 CPU 屏障指令**，只是阻止编译器：

- 把多次读折叠为一次（"reload optimization"）
- 把一次大写拆成多次小写（"store tearing"）
- 重排相邻的内存访问

任何**无锁读多线程共享变量**都应该用 `READ_ONCE`，否则编译器可能让你看到根本不存在的中间值。`atomic_read/atomic_set` 内部就是它们。


## 6. 典型 lock-free 模式

### 6.1 Publish a pointer (single-producer single-consumer)

```c
/* writer */
struct obj *new = kmalloc(sizeof(*new), GFP_KERNEL);
new->field = 42;
smp_store_release(&shared_ptr, new);   /* RELEASE: field=42 一定在 ptr 之前可见 */

/* reader */
struct obj *p = smp_load_acquire(&shared_ptr);  /* ACQUIRE */
if (p)
    use(p->field);                              /* 一定能看到 42 */
```

这正是 RCU 中 `rcu_assign_pointer` / `rcu_dereference` 的本质（在 ARM 上 dereference 还退化为依赖序，无需显式屏障）。

### 6.2 Producer/consumer 标志位

```c
/* producer */
buffer[i] = data;
smp_wmb();              /* data 写在 head 更新之前 */
WRITE_ONCE(head, i + 1);

/* consumer */
unsigned h = READ_ONCE(head);
if (h != tail) {
    smp_rmb();          /* head 读在 buffer 读之前 */
    use(buffer[tail]);
    tail++;
}
```

### 6.3 Double-checked init

```c
if (!READ_ONCE(initialized)) {
    mutex_lock(&init_lock);
    if (!initialized) {
        do_expensive_init();
        smp_store_release(&initialized, 1);
    }
    mutex_unlock(&init_lock);
}
```


## 7. 与 MMIO / DMA 的交互

| 场景                              | 正确做法                      |
| --------------------------------- | ----------------------------- |
| 写寄存器后立即读（同设备）        | 使用 `writel()` + `readl()`，已含 IO 屏障 |
| 准备 DMA 描述符 → 启动 DMA        | `dma_wmb()`（外部域写屏障）   |
| DMA 完成中断 → 读 DMA 写出的数据  | `dma_rmb()`                   |
| `writel` 之后想立刻让外设看到     | `writel` 自带 `wmb()`         |
| 与其他 CPU 共享内存               | `smp_mb/wmb/rmb` (ish)        |

**`smp_*` 屏障不覆盖外设域**：要让 DMA 引擎看到 CPU 的写，必须 `dma_wmb`（`dmb oshst`）或更强的 `wmb`。


## 8. 调试手段

- `lockdep` (`CONFIG_PROVE_LOCKING`) 自动检测锁/屏障误用模式。
- `KCSAN` (4.9 没有，4.19+ 才合入) 是数据竞争检测器，了解即可。
- `objtool` / `sparse __rcu` 注解用于发现少打的 `rcu_dereference`。
- 出现 "在 x86 上正常、在 ARM 上偶发崩溃" 90% 是漏屏障。


## 9. 速查口诀

```text
仅怕编译器     →  barrier() / READ_ONCE / WRITE_ONCE
跨 CPU 数据序  →  smp_mb / smp_rmb / smp_wmb
publish 一次   →  smp_store_release  ↔  smp_load_acquire
原子计数       →  atomic_inc / atomic_dec_and_test
原子 CAS       →  atomic_cmpxchg / cmpxchg
含 MMIO/DMA    →  wmb / rmb / dma_wmb / dma_rmb
极致性能       →  atomic_*_relaxed + 自己排屏障
```
