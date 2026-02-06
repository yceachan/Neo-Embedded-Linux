# Neo-Embedded-Linux

> **A modern, AI-assisted playbook for mastering Linux Kernel Driver development on NXP i.MX6ULL.**

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-NXP%20i.MX6ULL-green.svg)
![Kernel](https://img.shields.io/badge/kernel-Linux%204.9.88-orange.svg)
![Status](https://img.shields.io/badge/status-Active-brightgreen.svg)

**Neo-Embedded-Linux** is a next-generation repository dedicated to embedded Linux learning and driver development. Unlike traditional tutorials, this project leverages **AI Agents (Gemini CLI)** to govern knowledge bases, standardize code quality, and accelerate the learning curve from "Hello World" to complex subsystem drivers.

## 🚀 Key Features

- **🧠 AI-Governed Knowledge Base**: Structured, strictly governed notes on Kernel Subsystems (GPIO, I2C, DTS) and Build Systems (Kbuild).
- **📘 Deep-Dive Documentation**: "Source-Driven Analysis" method mapping hardware -> driver -> subsystem -> VFS.
- **⚡ Modern Workflow**: Best practices for developing Out-of-Tree drivers using WSL/Linux and modern CLI tools.
- **📂 Structured Engineering**: Clear separation of vendor docs, personal notes, and production-ready code.

## 📂 Project Structure

| Directory | Description |
| :--- | :--- |
| `note/` | **The Core Brain.** Deep technical notes, architectural diagrams (Mermaid), and best practices. <br> *Highlights: `note/KernelLearning最佳实践.md`, `note/DTS/`, `note/Kbuild/`* |
| `prj/` | **The Code Forge.** Source code for kernel drivers and applications. <br> *Includes: `01_hello_drv`, `gpio_drv`, and standardized Makefiles.* |
| `sdk/` | **The Foundation.** NXP/100ASK BSP, Linux 4.9 Kernel source, and Toolchains. |
| `docs/` | **The Reference.** Official datasheets (IMX6ULL RM), board schematics, and vendor manuals. |
| `GEMINI.md`| **The Agent Protocol.** Rules and conventions for AI Agents interacting with this repository. |

## 🛠️ Getting Started

### Prerequisites

- **Hardware**: NXP i.MX6ULL Development Board (e.g., 100ASK_IMX6ULL_PRO).
- **Host**: Linux (Ubuntu 20.04+) or Windows (WSL2).
- **Toolchain**: `arm-linux-gnueabihf-` (provided in SDK).

### Build Your First Driver

1.  **Setup Environment**:
    Refer to `note/Kbuild/05-external-modules.md` for setting up the environment variables.

2.  **Compile the Hello World Driver**:
    ```bash
    cd prj/01_hello_drv
    make
    # Output: hello_drv.ko
    ```

3.  **Deploy**:
    Copy `.ko` to your board (via NFS/SCP) and test:
    ```bash
    insmod hello_drv.ko
    ./hello_drv_test
    rmmod hello_drv
    ```

## 📚 Knowledge Map

Start your journey here:

1.  **Methodology**: [Kernel Learning Best Practices](note/KernelLearning最佳实践.md)
2.  **Build System**: [Kbuild Governance](note/Kbuild/00-governance-plan.md)
3.  **Device Tree**: [DTS Usage Model](note/DTS/01-usage-model.md)

## 🤝 Contributing

This project adopts a **"Governance First"** approach.
- **Notes**: Must include YAML headers and follow the `note/KernelLearning最佳实践.md` format.
- **Code**: Follows Linux Kernel Coding Style.
- **Diagrams**: Use Mermaid.js for architecture visualizations.

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

Copyright (c) 2026 @yceachan
