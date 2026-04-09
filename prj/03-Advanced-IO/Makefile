# -----------------------------------------------------------------------
#  Out-of-Tree Build Wrapper — adv_io_drv (Advanced IO paradigms demo)
# -----------------------------------------------------------------------

KERN_DIR      = /home/pi/imx/sdk/100ask_imx6ull-sdk/Linux-4.9.88
CROSS_COMPILE = arm-buildroot-linux-gnueabihf-

PWD        := $(shell pwd)
BUILD_DIR  := $(PWD)/build
OUTPUT_DIR := $(PWD)/output

FILES_TO_COPY := src Kbuild

TEST_SRCS := test/test_block.c test/test_poll.c test/test_fasync.c test/test_aio.c
TEST_BINS := $(patsubst test/%.c,%,$(TEST_SRCS))

all: driver app

driver:
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR)
	@echo ">>> [Driver] Copying source..."
	@cp -rf $(FILES_TO_COPY) $(BUILD_DIR)/
	@mv $(BUILD_DIR)/Kbuild $(BUILD_DIR)/Makefile
	@echo ">>> [Driver] Building kernel module..."
	@make -C $(KERN_DIR) M=$(BUILD_DIR) modules ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE)
	@cp $(BUILD_DIR)/adv_io_drv.ko $(OUTPUT_DIR)/
	@echo ">>> [Driver] Artifact: $(OUTPUT_DIR)/adv_io_drv.ko"

app:
	@mkdir -p $(OUTPUT_DIR)
	@echo ">>> [App] Building user-space test programs..."
	$(CROSS_COMPILE)gcc -O2 -Wall -o $(OUTPUT_DIR)/test_block  test/test_block.c
	$(CROSS_COMPILE)gcc -O2 -Wall -o $(OUTPUT_DIR)/test_poll   test/test_poll.c
	$(CROSS_COMPILE)gcc -O2 -Wall -o $(OUTPUT_DIR)/test_fasync test/test_fasync.c
	$(CROSS_COMPILE)gcc -O2 -Wall -o $(OUTPUT_DIR)/test_aio      test/test_aio.c -lrt
	$(CROSS_COMPILE)gcc -O2 -Wall -o $(OUTPUT_DIR)/test_nonblock test/test_nonblock.c
	@echo ">>> [App] Artifacts in $(OUTPUT_DIR)/"

clean:
	@rm -rf $(BUILD_DIR) $(OUTPUT_DIR) compile_commands.json
	@echo ">>> Cleaned."

.PHONY: all driver app clean
