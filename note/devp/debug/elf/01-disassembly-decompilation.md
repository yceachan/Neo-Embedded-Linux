---
title: 从二进制到源代码：反汇编与反编译流程
tags: [Reverse Engineering, Disassembly, Decompilation, Ghidra]
desc: 详解从 ELF 机器码恢复为汇编指令以及进一步反编译为 C 伪代码的技术路径。
update: 2026-04-07
---


# 从二进制到源代码：反汇编与反编译流程

> [!note]
> **Ref:** [Ghidra Guide](https://ghidra-re.org/), [Reverse Engineering: Art of Exploitation]

逆向工程的过程是一个**信息熵减少**的过程。我们从高度压缩的二进制指令中，通过启发式算法和符号映射，还原人类可读的逻辑。

## 1. 反汇编 (Disassembly)
**目标**：将机器码（Opcode）转换为汇编助记符。

- **工具**：`objdump`, `GDB`, `Capstone Engine`.
- **关键技术**：
  - **线性扫描 (Linear Sweep)**：从头开始逐字节解析。
  - **递归下降 (Recursive Descent)**：跟随跳转指令解析，能有效处理代码中的数据空隙。
- **局限性**：汇编指令依然是低级的，难以看出复杂的业务逻辑。

## 2. 反编译 (Decompilation)
**目标**：将汇编逻辑抽象为高层次的伪 C 代码。

### 核心步骤：
1. **控制流分析 (CFG Recovery)**：
   - 识别基本的代码块（Basic Blocks）。
   - 将 `jump` 关系转化为 `if/else`, `switch`, `while`, `for` 结构。
2. **数据流分析 (Data Flow Analysis)**：
   - 追踪寄存器和栈空间的使用。
   - 消除临时寄存器变量，合并为高级语言变量。
3. **类型传播 (Type Propagation)**：
   - 根据指令（如 `addpd` 处理双精度浮点）推断数据类型。
   - 识别结构体内存布局。

## 3. 符号的作用：逆向的“作弊码”

| 状态 | 反汇编效果 | 反编译效果 |
| :--- | :--- | :--- |
| **Debug Build (-g)** | 完美显示函数名、行号、变量名 | 几乎能 1:1 还原原始 C 代码 |
| **Release Build** | 仅显示导出的函数名 (如 main) | 变量名丢失 (v1, v2)，逻辑基本清晰 |
| **Stripped (剥离符号)** | 只有地址，无任何名字 | 仅剩逻辑，极其难以阅读 |

## 4. 实战建议
- **初学者**：使用 `objdump -S` 对比自己写的代码。
- **进阶者**：使用 **Ghidra** 的 Decompiler 面板。它能展示从机器码到伪 C 代码的实时转换。
- **内核分析**：关注内核符号表 `/proc/kallsyms`，这是内核世界的符号来源。
