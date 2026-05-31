# tspi-greet-egl — wayland-egl + GLES2 客户端
#
# 与 tspi-greet/Makefile 同构，额外多三个 -l：EGL / GLESv2 / wayland-egl
# 所有产物落 output/，src/ 只放人手写源 + wayland-scanner 生成的 xdg-shell-*

TSPI_SDK := $(HOME)/imx/sdk/tspi-rk3566-sdk/buildroot/output/rockchip_rk3566_taishanpi_1m_v10/rockchip_rk3566
SYSROOT  := $(TSPI_SDK)/host/aarch64-buildroot-linux-gnu/sysroot
TC       := $(TSPI_SDK)/host/bin/aarch64-buildroot-linux-gnu-
SCANNER  := $(TSPI_SDK)/host/bin/wayland-scanner
PROTO    := $(SYSROOT)/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

CC      := $(TC)gcc
CFLAGS  := --sysroot=$(SYSROOT) -O2 -Wall -Wextra -Wno-unused-parameter -Isrc
LDFLAGS := --sysroot=$(SYSROOT) \
           -lwayland-client -lwayland-egl \
           -lEGL -lGLESv2 \
           -lm

BIN_DIR := output/bin
LOG_DIR := output/log
TARGET  := $(BIN_DIR)/tspi-greet-egl

SRCS    := src/main.c src/render.c src/xdg-shell-protocol.c
GEN_H   := src/xdg-shell-client.h
GEN_C   := src/xdg-shell-protocol.c

.PHONY: all clean dirs
all: dirs $(TARGET)

dirs:
	@mkdir -p $(BIN_DIR) $(LOG_DIR)

$(GEN_H):
	$(SCANNER) client-header $(PROTO) $@

$(GEN_C):
	$(SCANNER) private-code $(PROTO) $@

$(TARGET): $(GEN_H) $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(GEN_H) $(GEN_C)
	rm -rf output/bin/* output/log/*
