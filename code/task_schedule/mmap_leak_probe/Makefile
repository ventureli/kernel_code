# 编译两个东西：
#   1. leak_demo          —— 用户态泄漏程序（gcc）
#   2. mmap_leak_probe.ko —— 内核模块（内核构建系统）
#
# 目标环境：ARM64 Linux（如 Ubuntu 22.04，内核 5.15）

NAME := mmap_leak_probe
obj-m := $(NAME).o
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

.PHONY: all modules demo clean install uninstall

all: modules demo

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

demo:
	gcc -O2 -Wall -o leak_demo leak_demo.c

install:
	sudo insmod $(NAME).ko target=leak_demo

uninstall:
	sudo rmmod $(NAME)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f leak_demo *.o *.ko *.mod.c modules.order Module.symvers
