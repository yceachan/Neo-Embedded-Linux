# demo-fork — host build (x86_64), elf 输出到 output/
CC      ?= gcc
CFLAGS  ?= -O0 -g -Wall -Wextra
OUTDIR  := output

SRCS    := $(wildcard *.c)
BINS    := $(patsubst %.c,$(OUTDIR)/%,$(SRCS))

.PHONY: all clean run
all: $(BINS)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/%: %.c | $(OUTDIR)
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf $(OUTDIR)
