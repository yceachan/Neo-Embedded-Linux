CC = gcc
CFLAGS = -Wall -g
TARGET = output/signal_demo
SRC = signal_demo.c

all: $(TARGET)

$(TARGET): $(SRC)
	@mkdir -p output
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf output

.PHONY: all clean
