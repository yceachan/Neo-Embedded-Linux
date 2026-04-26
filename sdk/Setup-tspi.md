# TSPI RK3566 SDK

This directory contains the SDK and resources for the TSPI RK3566 development board.

All Ref Lckfb wikil.

## Directory Structure

- `app/` - Applications.
- `buildroot/` - Buildroot root filesystem generation.
- `debian/` - Debian OS build configurations.
- `device/` - Device specific configs.
- `docs/` - SDK Documentation.
- `external/` - External packages and libraries.
- `hal/` - Hardware Abstraction Layer.
- `kernel/` & `kernel-6.1/` - Linux Kernel source trees (6.1 is default).
- `prebuilts/` - Prebuilt toolchains and binaries (contains GCC for Aarch64).
- `rkbin/` - Rockchip binary blobs.
- `rtos/` - RTOS source code.
- `tools/` - SDK utility tools.
- `u-boot/` - U-Boot bootloader source.
- `yocto/` - Yocto project support.
- `build.sh` - Main build script.
- `Makefile` - Main SDK Makefile.
- `rkflash.sh` - Flashing tool script.

## Environment Variables and Toolchain

To develop for the TSPI RK3566 board, a specific cross-compilation environment must be set up.

- **Architecture:** `arm64` (Aarch64)
- **Toolchain:** `gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu`
- **Compiler Prefix:** `aarch64-none-linux-gnu-`
- **Kernel Directory:** `kernel-6.1`

### Activating the Environment

You can easily set up the necessary environment variables by sourcing the top-level script:

```bash
source ~/imx/tspi_env.sh
```

This will automatically set `ARCH`, `CROSS_COMPILE`, and include the toolchain in your `PATH`.
