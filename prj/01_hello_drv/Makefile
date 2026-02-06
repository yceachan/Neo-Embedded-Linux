# -----------------------------------------------------------------------
#  Out-of-Tree Build Wrapper (Multi-file + Output Flattening)
# -----------------------------------------------------------------------

# 1. Config
ARM =arm
KERN_DIR = /home/pi/imx/sdk/100ask_imx6ull-sdk/Linux-4.9.88
CROSS_COMPILE = arm-buildroot-linux-gnueabihf-
TEST_APP_BIN = hello_drv_test

# 2. Paths
PWD := $(shell pwd)
BUILD_DIR := $(PWD)/build
OUTPUT_DIR := $(PWD)/output

# 定义需要拷贝到构建目录的资源
SRC_DIRS := src include app
FILES_TO_COPY := $(SRC_DIRS) Kbuild

# 3. Targets
all: driver app

driver:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OUTPUT_DIR)
	
	@echo ">>> [Driver] Copying source tree..."
	@cp -rf $(FILES_TO_COPY) $(BUILD_DIR)/
	@mv $(BUILD_DIR)/Kbuild $(BUILD_DIR)/Makefile
	
	@echo ">>> [Driver] Building Kernel Module..."
	@make -C $(KERN_DIR) M=$(BUILD_DIR) modules ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)
	
	@echo ">>> [Driver] Installing to output..."
	@cp $(BUILD_DIR)/hello_drv.ko $(OUTPUT_DIR)/
	@echo ">>> [Driver] Artifact: $(OUTPUT_DIR)/hello_drv.ko"

app:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(OUTPUT_DIR)
	
	@echo ">>> [App] Building Application..."
	@# 编译 app/ 下的所有 .c 文件
	$(CROSS_COMPILE)gcc -o $(BUILD_DIR)/$(TEST_APP_BIN) app/main.c app/syscall_wrapper.c -Iapp
	
	@echo ">>> [App] Installing to output..."
	@cp $(BUILD_DIR)/$(TEST_APP_BIN) $(OUTPUT_DIR)/
	@echo ">>> [App] Artifact: $(OUTPUT_DIR)/$(TEST_APP_BIN)"

clean:
	@rm -rf $(BUILD_DIR) $(OUTPUT_DIR) compile_commands.json
	@echo ">>> Cleaned build and output directories."

compile_commands: clean
	@mkdir -p $(BUILD_DIR)
	@cp -rf $(FILES_TO_COPY) $(BUILD_DIR)/
	@mv $(BUILD_DIR)/Kbuild $(BUILD_DIR)/Makefile
	bear -- make -C $(KERN_DIR) M=$(BUILD_DIR) modules ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)
	@mv compile_commands.json $(PWD)/

.PHONY: all driver app clean compile_commands