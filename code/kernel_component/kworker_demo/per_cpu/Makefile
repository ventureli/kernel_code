MNAM :=per_cpu_queue_demo
obj-m :=$(MNAM).o
KERNELDIR ?=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)

all:
	make -C $(KERNELDIR) M=$(PWD) modules
clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c modules.order Module.symvers
install:
	sudo insmod $(MNAM).ko
uninstall:
	sudo rmmod $(MNAM)
log:
	sudo dmesg | tail -120
test:
	sudo dmesg -C 
	sudo insmod $(MNAM).ko
	sudo dmesg 
	sudo rmmod $(MNAM)
	sudo dmesg  
