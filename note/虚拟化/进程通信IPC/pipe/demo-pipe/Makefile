CC      = gcc
CFLAGS  = -Wall -Wextra -g

OUTDIR  = output
TARGET  = $(OUTDIR)/pipe_demo

all: $(OUTDIR) $(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(TARGET): pipe_demo.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf $(OUTDIR)

run: all
	./$(TARGET)

.PHONY: all clean run
