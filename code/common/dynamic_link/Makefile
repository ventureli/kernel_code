CC := gcc
CFLAGS := -Wall -g -fPIC

.PHONY: all clean

all: libdemo.so dlopen_demo

# 编译共享库
libdemo.so: libdemo.c libdemo.h
	$(CC) $(CFLAGS) -shared -o $@ libdemo.c

# 编译 dlopen 调用者
dlopen_demo: dlopen_demo.c
	$(CC) $(CFLAGS) -o $@ dlopen_demo.c -ldl

# 运行测试
test: all
	@echo "=== readelf -h libdemo.so ==="
	readelf -h libdemo.so | head -6
	@echo ""
	@echo "=== readelf -d dlopen_demo ==="
	readelf -d dlopen_demo | grep -E "NEEDED|RUNPATH|INTERP"
	@echo ""
	@echo "=== 运行 dlopen_demo ==="
	./dlopen_demo

clean:
	rm -f libdemo.so dlopen_demo
