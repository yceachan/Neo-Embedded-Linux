---
title: README - muxIO Demo
tags: [demo, io, multiplexing]
desc: I/O 多路复用演示项目的说明文档。
update: 2026-04-09
---

# muxIO: 多路复用对比演示

本项目旨在演示和对比 Linux 下三种主流的 I/O 多路复用系统调用：`select`, `poll`, `epoll`。

## 项目结构

```text
prj/04-muxIO/
├── README.md          # 部署和运行指南
├── Design-muxIO.md    # 架构、时序图与源码导读
├── Consult.md         # 现象分析与实际运行输出
├── src/               
│   └── main.c         # 全部 C 语言源码
├── Makefile           # 编译脚本
├── output/            # 编译及运行产物输出目录
│   ├── bin/           
│   ├── obj/           
│   └── log/           
└── test.sh            # 一键编译、运行与测试脚本
```

## 部署与环境准备

- **依赖**: Linux 操作系统（提供了 `epoll`），GCC 编译器，Make。
- **环境搭建**: 此项目使用标准 C 和 POSIX API，无需安装第三方库。

## 一键测试

进入 `prj/04-muxIO` 目录，执行以下命令即可触发“清理 → 构建 → 执行并测试（三种机制）”的全链路操作：

```bash
./test.sh
```

**期望的运行输出示例 (Exit 0):**

```text
Cleaning...
rm -rf output/bin output/obj output/log
Building...
gcc -c -o output/obj/main.o src/main.c
gcc -o output/bin/muxio output/obj/main.o
Running select test...
--- Using select ---
[select] Read from fd1: MSG1
...
[select] grep_marker_success
Running poll test...
--- Using poll ---
...
[poll] grep_marker_success
Running epoll test...
--- Using epoll ---
...
[epoll] grep_marker_success
All tests passed!
```

## 清理工作

执行以下命令可以清除所有的二进制文件和运行日志：

```bash
make clean
```
