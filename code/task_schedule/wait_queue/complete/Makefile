MNAM :=complete_all1
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
	sudo rmmod -f $(MNAM)
show:
	sudo lsmod | grep $(MNAM)
log:
	sudo dmesg | tail -20
test:
	@echo ------------------------
	- make

	@echo ------------------------
	sudo dmesg -C 
	sudo insmod $(MNAM).ko
	sudo dmesg 
	sudo rmmod $(MNAM)
	sudo dmesg   

	@echo ------------------------
	make clean

