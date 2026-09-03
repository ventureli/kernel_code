- [如何在虚拟里面可以调试kprobe](#如何在虚拟里面可以调试kprobe)
  - [虚拟机配置](#虚拟机配置)
  - [机器的配置](#机器的配置)

# 如何在虚拟里面可以调试kprobe

## 虚拟机配置

- 硬件-处理器-开启虚拟化IntelVT-x/EPT或AMD-V/RVI
- 选项-高级-打开UEFI（但是不打开启动安全引导）

## 机器的配置

可能是因为模块签名出现了错误，看dmesg会出现下面的问题

```sh
[   34.429005] input: VMware DnD UInput pointer as /devices/virtual/input/input7
[  349.396416] kprobe_demo: loading out-of-tree module taints kernel.
[  349.396420] kprobe_demo: module verification failed: signature and/or required key missing - tainting kernel
[  349.398359] register_kprobe failed
[  353.292808] register_kprobe failed
[  354.145855] register_kprobe failed
```

需要编辑，/etc/default/grub 增加 module.sig_enforce=0 这个设置。

```C
xuan@xuan-u22:~/Desktop/share_dir/article/kernel/code/kprobe$ sudo nano /etc/default/grub

/*

GRUB_CMDLINE_LINUX_DEFAULT="quiet splash module.sig_enforce=0"
GRUB_CMDLINE_LINUX="find_preseed=/preseed.cfg auto noprompt priority=critical locale=en_US module.sig_enforce=0"
*/
xuan@xuan-u22:~/Desktop/share_dir/article/kernel/code/kprobe$ sudo update-grub
Sourcing file `/etc/default/grub'
Sourcing file `/etc/default/grub.d/init-select.cfg'
Generating grub configuration file ...
Found linux image: /boot/vmlinuz-6.5.0-28-generic
Found initrd image: /boot/initrd.img-6.5.0-28-generic
Found linux image: /boot/vmlinuz-5.19.0-32-generic
Found initrd image: /boot/initrd.img-5.19.0-32-generic
Memtest86+ needs a 16-bit boot, that is not available on EFI, exiting
Warning: os-prober will not be executed to detect other bootable partitions.
Systems on them will not be added to the GRUB boot configuration.
Check GRUB_DISABLE_OS_PROBER documentation entry.
Adding boot menu entry for UEFI Firmware Settings ...
done
xuan@xua
```

最后重启`sudo reboot`
