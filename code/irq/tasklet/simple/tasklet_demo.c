
#define pr_fmt(fmt) "WQTK :" fmt
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>


//定义tasklet和自定义数据
static struct tasklet_struct my_tasklet;
static unsigned long my_priv_data = 0x99887766;

//软中断上下文，和软中断完全等级
static void my_tasklet_handler(unsigned long data) {
    int cpu = smp_processor_id();
    pr_info("My softirqhandleris runing  %d cpu:%d", current->pid,cpu);
}

static int __init my_tasklet_init(void) {
    pr_info("----------------------------------------------------\n");
    pr_info("my_tasklet:loaded :注册中断 current->pid:%d\n",current->pid);
    /*
    *注册tasklet 
    * */
    tasklet_init(&my_tasklet, my_tasklet_handler, my_priv_data);
    //调用调度tasklet，触发软中断，等价于raise_softirq
    //这里可以打印tasklet的对应的data和state
    tasklet_schedule(&my_tasklet); 
    return 0;
    
}

static void __exit my_softirq_exit(void) {
    tasklet_kill(&my_tasklet);
    pr_info("my_tasklet:unloaded");
}


MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");

module_init(my_tasklet_init);
module_exit(my_softirq_exit);





/**
 * 这个代码内核编译的时候无法链接成功，因为内核没有导出这个符号。编译能通过是因为这里确实有这个定义。
 * 
 * 
 * make -C /lib/modules/6.5.0-28-generic/build M=/home/xuan/Desktop/share_dir/article/kernel/code/irq/softirqs modules
make[1]: Entering directory '/usr/src/linux-headers-6.5.0-28-generic'
warning: the compiler differs from the one used to build the kernel
  The kernel was built by: x86_64-linux-gnu-gcc-12 (Ubuntu 12.3.0-1ubuntu1~22.04) 12.3.0
  You are using:           gcc-12 (Ubuntu 12.3.0-1ubuntu1~22.04) 12.3.0
  CC [M]  /home/xuan/Desktop/share_dir/article/kernel/code/irq/softirqs/sorftirq_demo.o
  MODPOST /home/xuan/Desktop/share_dir/article/kernel/code/irq/softirqs/Module.symvers
ERROR: modpost: "open_softirq" [/home/xuan/Desktop/share_dir/article/kernel/code/irq/softirqs/sorftirq_demo.ko] undefined!
ERROR: modpost: "raise_softirq" [/home/xuan/Desktop/share_dir/article/kernel/code/irq/softirqs/sorftirq_demo.ko] undefined!
make[3]: *** [scripts/Makefile.modpost:144: /home/xuan/Desktop/share_dir/article/kernel/code/irq/softirqs/Module.symvers] Error 1
make[2]: *** [/usr/src/linux-headers-6.5.0-28-generic/Makefile:1991: modpost] Error 2
make[1]: *** [Makefile:234: __sub-make] Error 2
make[1]: Leaving directory '/usr/src/linux-headers-6.5.0-28-generic'
make: *** [Makefile:7: all] Error 2
 * 
 * 
*/