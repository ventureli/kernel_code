NAME := mutex_demo
obj-m :=$(NAME).o
KERNELDIR ?=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)

all:
	make -C $(KERNELDIR) M=$(PWD) modules
clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c modules.order Module.symvers
install:
	sudo insmod $(NAME).ko
uninstall:
	sudo rmmod $(NAME)
log:
	sudo dmesg | tail -20
show:
	sudo lsmod | grep $(NAME)
test:
	sudo dmesg -C 
	sudo insmod $(NAME).ko
	sudo dmesg 
	sudo rmmod $(NAME)
	sudo dmesg  
