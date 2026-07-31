MNAM := ioctl_demo_rw_state
obj-m :=$(MNAM).o
KERNELDIR ?=/lib/modules/$(shell uname -r)/build
PWD :=$(shell pwd)

for_platform ?= phone
plfx ?= phone

ifeq ($(for_platform), phone) 
	plfx := phone

else

	plfx := pc
endif


#for crossfile 
KDIR ?= /home/dell/code/6.0/out/rk3568/kernel/linux-5.10
CROSS_COMPILE ?= /home/dell/code/6.0/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
DSTARCH=arm64
CFLAGS += -I/$(KDIR)/arch/arm64/include
LOG_TAG := WQTK1


#.PHONY: all info clean test uninstall install

phone_all:
	$(MAKE) ARCH=$(DSTARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

phone_clean:
	 $(MAKE) ARCH=$(DSTARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c modules.order Module.symvers *.cmd

phone_install:
	hdc.exe shell "dmesg -C"
	@echo "--------------transfer ko to data------------"
	hdc.exe shell mkdir /data/test_ko
	hdc.exe file send $(MNAM).ko /data/test_ko
	@echo "------------------------------------"
	hdc.exe shell "insmod /data/test_ko/$(MNAM).ko"
	@echo "------------------------------------"
	
phone_uninstall:
	hdc.exe shell "rmmod $(MNAM)"
phone_log:
	@echo "---------------log - begin ---------------------"
	hdc.exe shell "dmesg | grep $(LOG_TAG) | tail -100"
	@echo "---------------log - end ---------------------"
	
phone_test:
	@echo "----------clean"
	@make phone_clean
	@echo "----------uninstall"
	@make phone_uninstall
	@echo "----------make"
	@make phone_all
	@echo "----------install"
	@make phone_install
	@echo "----------uninstall"
	@make phone_uninstall
	@echo "----------log"
	@make phone_log
 
#for crossfile 


pc_all:
	make -C $(KERNELDIR) M=$(PWD) modules
pc_clean:
	make -C $(KERNELDIR) M=$(PWD) clean
	rm -f *.o *.ko *.mod.c modules.order Module.symvers
pc_install:
	sudo insmod $(MNAM).ko
pc_uninstall:
	sudo rmmod $(MNAM)
pc_log:
	sudo dmesg | tail -20
pc_show:
	sudo lsmod | grep $(MNAM)
pc_test:
	sudo dmesg -C 
	sudo insmod $(MNAM).ko
	sudo dmesg 
	sudo rmmod $(MNAM)
	sudo dmesg  

info:
	@echo "for_platform:"$(for_platform)
	@echo "plfx:"$(plfx)

all:
	@echo "for_platform::"$(plfx)
	@make $(plfx)_all
clean:
	@make $(plfx)_clean
install:
	@make $(plfx)_install
	@make $(plfx)_log
uninstall:
	@make $(plfx)_uninstall
	@make $(plfx)_log
log:
	@make $(plfx)_log
show:
	@make $(plfx)_show
test:
	@make $(plfx)_test