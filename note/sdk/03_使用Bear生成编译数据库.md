# 使用 Bear 生成内核编译数据库 (compile_commands.json)

为了获得最佳的代码补全和跳转体验，建议使用 `bear` 录制完整的构建过程。

## 综合单行指令（Linaro 工具链）

在内核源码根目录 `sdk/100ask_imx6ull-sdk/Linux-4.9.88` 下执行：

```bash
make distclean 

make ARCH=arm CROSS_COMPILE=/home/pi/imx/sdk/100ask_imx6ull-sdk/ToolChain/gcc-linaro-6.2.1-2016.11-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf- 100ask_imx6ull_defconfig 

bear -- make ARCH=arm CROSS_COMPILE=/home/pi/imx/sdk/100ask_imx6ull-sdk/ToolChain/gcc-linaro-6.2.1-2016.11-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf- HOSTCFLAGS="-fcommon" zImage dtbs modules -j12

```

## 指令解析
1. **`make distclean`**: 彻底清理，确保 `bear` 捕获到所有文件的编译动作。
2. **`make ... <board>_defconfig`**: 重新根据当前工具链生成配置。
3. **`bear -- make ...`**: 核心录制指令。
   - **`ARCH=arm`**: 必须指定，决定了头文件索引路径。
   - **`CROSS_COMPILE=...`**: 推荐使用绝对路径，避免环境变量失效。
   - **`-j$(nproc)`**: 利用多核并行编译，显著提升录制速度。
4. **`zImage dtbs modules`**: 编译目标全集，确保驱动代码也被索引。
