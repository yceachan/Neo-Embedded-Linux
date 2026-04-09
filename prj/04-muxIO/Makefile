TARGET = output/bin/muxio
SRC = src/main.c
OBJDIR = output/obj

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJDIR)/main.o
	@mkdir -p output/bin
	gcc -o $@ $^

$(OBJDIR)/main.o: $(SRC)
	@mkdir -p $(OBJDIR)
	gcc -c -o $@ $<

clean:
	rm -rf output/bin output/obj output/log
