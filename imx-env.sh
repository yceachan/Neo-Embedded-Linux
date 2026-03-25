# IMX6ULL Buildroot SDK Environment Variables
# Source this file to set up the toolchain and environment

export KERN_DIR="/home/pi/imx/sdk/100ask_imx6ull-sdk/Linux-4.9.88"
export ARCH=arm
export CROSS_COMPILE=arm-buildroot-linux-gnueabihf-
export PATH=$PATH:/home/pi/imx/sdk/100ask_imx6ull-sdk/ToolChain/arm-buildroot-linux-gnueabihf_sdk-buildroot/bin

function dump_armgcc () {
    echo "--- IMX6ULL Buildroot Toolchain Info ---"
    echo "[KERN_DIR]      = $KERN_DIR"
    echo "[ARCH]          = $ARCH"
    echo "[CROSS_COMPILE] = $CROSS_COMPILE"
    echo "----------------------------------------"
}
