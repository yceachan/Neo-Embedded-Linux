CC = gcc
CFLAGS = -I./lib -O0 -g
LDFLAGS_SHARED = -L./output -Wl,-rpath,./output -lmylib
LDFLAGS_STATIC = ./output/libmylib.a

all: output app_static app_shared

output:
	mkdir -p output

# Build static library
libmylib.a: lib/mylib.c lib/mylib.h
	$(CC) $(CFLAGS) -c lib/mylib.c -o output/mylib_static.o
	ar rcs output/libmylib.a output/mylib_static.o

# Build shared library
libmylib.so: lib/mylib.c lib/mylib.h
	$(CC) $(CFLAGS) -fPIC -shared lib/mylib.c -o output/libmylib.so

app_static: src/main.c libmylib.a
	$(CC) $(CFLAGS) src/main.c $(LDFLAGS_STATIC) -o output/app_static

app_shared: src/main.c libmylib.so
	$(CC) $(CFLAGS) src/main.c $(LDFLAGS_SHARED) -o output/app_shared

clean:
	rm -rf output
