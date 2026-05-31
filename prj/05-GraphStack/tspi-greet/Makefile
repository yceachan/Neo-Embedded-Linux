# 默认值用 := （强赋值），独立交叉编译走这里的默认路径。
# Buildroot 集成时从 .mk 的 BUILD_CMDS 显式 command-line 传 CC/CFLAGS/LDFLAGS/SCANNER/PROTO —
# Make 优先级：command-line > := > environment > ?=，所以 buildroot 注入仍能覆盖。
TSPI_SDK := $(HOME)/imx/sdk/tspi-rk3566-sdk/buildroot/output/rockchip_rk3566_taishanpi_1m_v10/rockchip_rk3566
SYSROOT  := $(TSPI_SDK)/host/aarch64-buildroot-linux-gnu/sysroot
TC       := $(TSPI_SDK)/host/bin/aarch64-buildroot-linux-gnu-
SCANNER  := $(TSPI_SDK)/host/bin/wayland-scanner
PROTO    := $(SYSROOT)/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

CC      := $(TC)gcc
CFLAGS  := --sysroot=$(SYSROOT) -O2 -Wall -Wno-unused-parameter -Isrc -I$(SYSROOT)/usr/include/cairo
LDFLAGS := --sysroot=$(SYSROOT) -lwayland-client -lcairo -lrt -lm

BIN_DIR := output/bin
TARGET  := $(BIN_DIR)/tspi-greet
SRCS    := src/main.c src/xdg-shell-protocol.c
GEN_H   := src/xdg-shell-client.h
GEN_C   := src/xdg-shell-protocol.c

.PHONY: all clean
all: $(TARGET)

$(GEN_H):
	$(SCANNER) client-header $(PROTO) $@

$(GEN_C):
	$(SCANNER) private-code $(PROTO) $@

$(TARGET): $(GEN_H) $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(GEN_H) $(GEN_C)
