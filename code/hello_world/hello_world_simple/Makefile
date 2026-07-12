MNAM := hello_world

obj-m := $(MNAM).o
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
TAG_NAME := WQTK

.PHONY: all info clean test uninstall install

all:
	make -C $(KERNELDIR) M=$(PWD) modules


clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c modules.order Module.symvers *.cmd

install:
	sudo insmod $(MNAM).ko

uninstall:
	sudo rmmod $(MNAM)

SRCS := $(wildcard *.c) $(wildcard *.h)

info:
	@echo "Module:     $(MNAM)"
	@echo "Kernel:     $(KERNELDIR)"
	@echo "Build dir:  $(PWD)"
	@echo "Sources:"
	@for f in $(SRCS); do echo "  $$f"; done

log:
	sudo dmesg | grep $(TAG_NAME) | tail -20

test:
	@echo "=== clean ==="
	@make clean
	@echo "=== build ==="
	@make all
	@echo "=== install ==="
	@sudo insmod $(MNAM).ko
	@echo "=== dmesg ==="
	@sudo dmesg | tail -20
	@echo "=== uninstall ==="
	@sudo rmmod $(MNAM)
