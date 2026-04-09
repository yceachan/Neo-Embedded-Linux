---
title: 内核调试速查 — printk 重定向与日志级别
tags: [debug, printk, dmesg, proc, kernel, i.MX6ULL]
desc: 将 printk 输出流到 SSH pts 的三种方法，及 console_loglevel 控制。
update: 2026-04-01

---


# 内核调试速查 — printk 重定向与日志级别

> [!note]
> **Ref:** `man 1 dmesg`, `man 2 syslog`, [`/proc/sys/kernel/printk`](https://www.kernel.org/doc/html/latest/admin-guide/sysctl/kernel.html#printk)
>
> ---
>   /proc/sys/ — sysctl 的 procfs 投影
>
>   /proc/sys/kernel/printk 是内核变量 console_loglevel 通过 sysctl
>   暴露给用户空间的文件接口：
>
>   写 /proc/sys/kernel/printk
>           │
>           ▼
>   proc_dointvec()          // kernel/sysctl.c，procfs 写回调
>           │
>           ▼
>   console_loglevel = 8     // kernel/printk/printk.c 中的全局变量
>
>   等价操作：
>   sysctl kernel.printk          # 读
>   sysctl -w kernel.printk=8     # 写，与 echo 8 > /proc/sys/... 完全等价
>
> ---

---

## 前置：检查 console_loglevel

`printk` 能否出现在终端，取决于消息级别是否低于 `console_loglevel`：

```bash
cat /proc/sys/kernel/printk
# 输出4个字段：console_loglevel  default  min  boot_default
# 示例：  4  4  1  7  → 只显示 WARNING(3) 及以上，KERN_INFO(6) 不可见

echo 8 > /proc/sys/kernel/printk   # 放开全部级别
```

| 宏 | 数值 | 含义 |
|----|------|------|
| `KERN_EMERG` | 0 | 系统崩溃 |
| `KERN_ERR` | 3 | 错误 |
| `KERN_WARNING` | 4 | 默认阈值 |
| `KERN_INFO` | 6 | 常规信息（驱动常用） |
| `KERN_DEBUG` | 7 | 调试详情 |

> 数值**小于** `console_loglevel` 的消息才会输出到 console。

---

## 三种流到 SSH pts 的方法

### 方法一：`/proc/kmsg` 直读（消耗式）

```bash
echo 8 > /proc/sys/kernel/printk && cat /proc/kmsg
```

- blocking read，新消息实时出现
- **消耗式**：只有一个进程能持有，`syslogd`/`klogd` 运行时会冲突
- 输出格式：`<级别数字>模块: 消息`

### 方法二：`dmesg -w`（非消耗式，推荐）

```bash
dmesg -w          # GNU coreutils
dmesg --follow    # 同上别名

# busybox dmesg 不支持 -w 时的兼容写法
while true; do dmesg -c; sleep 0.2; done
```

不独占 `/proc/kmsg`，可与 `syslogd` 共存。

### 方法三：`klogd -n` 前台（输出到 stderr → pts）

```bash
klogd -c 8 -n    # -n 不 daemonize，-c 8 含 DEBUG 级别
```

Ctrl-C 停止。适合需要与 syslog 格式对齐的场景。

---

## 实战：两个 SSH session 联调驱动

```
session A（日志监控）          session B（运行 app）
─────────────────────          ──────────────────────
echo 8 > /proc/sys/           insmod mmap_drv.ko
        kernel/printk
cat /proc/kmsg                 ./mmap_drv_test
  <6>mmap_drv: open(pid=312)
  <6>mmap_drv: mapped 0x88ce8000→0x76ff7000
  <6>mmap_drv: seq=5 msg='...'
```

---

## printk 格式速查

```c
printk(KERN_INFO  "drv: mapped phys=0x%lx\n", addr);   // 推荐
pr_info("drv: mapped phys=0x%lx\n", addr);              // 同上，简写宏
pr_err("drv: remap failed %d\n", ret);
pr_debug("drv: vma->vm_start=0x%lx\n", vma->vm_start); // 需 DEBUG 宏
dev_info(dev, "mapped phys=0x%lx\n", addr);             // 绑定 struct device，自动加设备名前缀
```
