# -----------------------------------------------------------------------
#  Out-of-Tree Build Wrapper — mmap_drv
# -----------------------------------------------------------------------

KERN_DIR     = /home/pi/imx/sdk/100ask_imx6ull-sdk/Linux-4.9.88
CROSS_COMPILE = arm-buildroot-linux-gnueabihf-
TEST_APP_BIN  = mmap_drv_test

PWD        := $(shell pwd)
BUILD_DIR  := $(PWD)/build
OUTPUT_DIR := $(PWD)/output

SRC_DIRS       := src include app
FILES_TO_COPY  := $(SRC_DIRS) Kbuild

all: driver app

driver:
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR)
	@echo ">>> [Driver] Copying source tree..."
	@cp -rf $(FILES_TO_COPY) $(BUILD_DIR)/
	@mv $(BUILD_DIR)/Kbuild $(BUILD_DIR)/Makefile
	@echo ">>> [Driver] Building kernel module..."
	@make -C $(KERN_DIR) M=$(BUILD_DIR) modules ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)
	@cp $(BUILD_DIR)/mmap_drv.ko $(OUTPUT_DIR)/
	@echo ">>> [Driver] Artifact: $(OUTPUT_DIR)/mmap_drv.ko"

app:
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR)
	@echo ">>> [App] Building test application..."
	$(CROSS_COMPILE)gcc -o $(BUILD_DIR)/$(TEST_APP_BIN) \
		app/main.c \
		-Iinclude \
		-lpthread
	@cp $(BUILD_DIR)/$(TEST_APP_BIN) $(OUTPUT_DIR)/
	@echo ">>> [App] Artifact: $(OUTPUT_DIR)/$(TEST_APP_BIN)"

clean:
	@rm -rf $(BUILD_DIR) $(OUTPUT_DIR) compile_commands.json
	@echo ">>> Cleaned."

compile_commands: clean
	@mkdir -p $(BUILD_DIR)
	@cp -rf $(FILES_TO_COPY) $(BUILD_DIR)/
	@mv $(BUILD_DIR)/Kbuild $(BUILD_DIR)/Makefile
	bear -- make -C $(KERN_DIR) M=$(BUILD_DIR) modules ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)
	@mv compile_commands.json $(PWD)/

.PHONY: all driver app clean compile_commands
