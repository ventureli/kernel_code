# 内核模块参数示例

对应文章：【内核札记-02】给内核模块加上参数：module_param 实现用户空间与内核的数据交互 (2026-06-30)

## 运行结果

```sh
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ make
make -C /lib/modules/6.0.0-xuan+/build M=/home/xuan/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param modules
make[1]: Entering directory '/home/xuan/code/6.0/out/rk3568/kernel/linux-5.10'
  CC [M]  /home/xuan/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param/hello_world_with_param.o
  MODPOST /home/xuan/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param/Module.symvers
  CC [M]  /home/xuan/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param/hello_world_with_param.mod.o
  LD [M]  /home/xuan/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param/hello_world_with_param.ko
make[1]: Leaving directory '/home/xuan/code/6.0/out/rk3568/kernel/linux-5.10'

xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ sudo insmod hello_world_with_param.ko pid=42
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ dmesg | tail -5
[ 1234.567890] hello_world_with_param: module loaded with pid = 42

xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ cat /sys/module/hello_world_with_param/parameters/pid
42
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ sudo sh -c 'echo 99 > /sys/module/hello_world_with_param/parameters/pid'
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ cat /sys/module/hello_world_with_param/parameters/pid
99

xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ sudo rmmod hello_world_with_param
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/hello_world/hello_world_param$ dmesg | tail -2
[ 1300.123456] hello_world_with_param: module unloaded, final pid = 99
```

## 知识点

- `module_param_cb` — 带回调的参数注册（比 `module_param` 更灵活）
- `kernel_param_ops` — 自定义参数的 set/get 回调
- sysfs `/sys/module/<name>/parameters/` — 运行时读写模块参数
