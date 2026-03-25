# 使用本地 GCC 编译器
CC = gcc
CFLAGS = -Wall

# 定义输出目录
OUT_DIR = output

# 定义最终的可执行文件路径
TARGETS = $(OUT_DIR)/my_viewer $(OUT_DIR)/caller

# 默认规则
all: $(OUT_DIR) $(TARGETS)

# 创建输出目录
$(OUT_DIR):
	mkdir -p $(OUT_DIR)

# 编译 my_viewer
$(OUT_DIR)/my_viewer: my_viewer.c
	$(CC) $(CFLAGS) -o $@ $<

# 编译 caller
$(OUT_DIR)/caller: caller.c
	$(CC) $(CFLAGS) -o $@ $<

# 运行演示
# 注意：由于 execve 调用的是当前目录下的 ./my_viewer，
# 我们需要进入输出目录运行，或者在 caller 中修改路径。
# 这里简单处理，将 caller 在 output/ 中运行。
run: all
	cd $(OUT_DIR) && ./caller

# 清理规则
clean:
	rm -rf $(OUT_DIR)

.PHONY: all clean run
