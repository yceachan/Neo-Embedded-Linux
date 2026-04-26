# TSPI RK3566 SDK Environment Variables
# Source this file to set up the toolchain and environment

export SDK_DIR="/home/pi/imx/sdk/tspi-rk3566-sdk"
export KERN_DIR="/home/pi/imx/sdk/tspi-rk3566-sdk/kernel-6.1"
export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-
# Sanitize PATH: remove any paths containing spaces (common in WSL or specific setups)
# This is required by Buildroot which strictly forbids spaces in PATH.
CLEAN_PATH=$(echo "$PATH" | tr ':' '\n' | grep -v ' ' | tr '\n' ':' | sed 's/:$//')
export PATH=$CLEAN_PATH

# Append cross-compiler path
export PATH=$PATH:/home/pi/imx/sdk/tspi-rk3566-sdk/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin

function dump_armgcc () {
    echo "--- TSPI RK3566 Toolchain Info ---"
    echo "[KERN_DIR]      = $KERN_DIR"
    echo "[ARCH]          = $ARCH"
    echo "[CROSS_COMPILE] = $CROSS_COMPILE"
    echo "----------------------------------------"
}

function syncImage () {
    rsync -aL $SDK_DIR/rockdev/ /mnt/c/Eachan/Workspace/MPUthings/tspi/rockdev
    echo "======sync2Wins Done========"
    ls -l /mnt/c/Eachan/Workspace/MPUthings/tspi/rockdev
}
