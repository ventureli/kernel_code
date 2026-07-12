#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <linux/smp.h>

static void my_work_handler(struct work_struct *work) {
    int cpu = smp_processor_id();
    printk(KERN_INFO "Work executed on cpu %d\n", cpu);
}

static void my_work_handler2_perwork(struct work_struct *work) {
    int cpu = smp_processor_id();
    printk(KERN_INFO "Work my_work_handler2_perwork 2 executed on cpu %d\n", cpu);
}

//定义 work
// 定义 work
static DECLARE_WORK(my_work, my_work_handler);

static DEFINE_PER_CPU(struct work_struct, my_per_cpu_work);

static struct workqueue_struct *per_cpu_wq;

//模块初始化函数
static int __init smp_example_init(void) {
    int cpu = 0;
    printk(KERN_INFO "Module loaded on CPU %d\n", cpu);
    //创建per_cpu_wq
    per_cpu_wq = alloc_workqueue("per_cpu_wq",  WQ_HIGHPRI | WQ_CPU_INTENSIVE  , 0);
    if (!per_cpu_wq) {
        printk(KERN_ERR "Faild to create per-CPU workqueue\n");
        return -ENOMEM;
    }
    //为每个cpu提交work  //普通work没办法往cpu敏感类型上进行处理
    for_each_online_cpu(cpu){
        if(!queue_work_on(cpu, per_cpu_wq, &my_work)){
            printk(KERN_ERR "Failed to queue work on CPU:%d \n", cpu);
        }
    }

    //进行调度 perCpu变量
    cpu = 0;
    for_each_possible_cpu(cpu) {
        INIT_WORK(&per_cpu(my_per_cpu_work, cpu), my_work_handler2_perwork);
    }
    for_each_online_cpu(cpu) {
        if(!queue_work_on(cpu, per_cpu_wq, &per_cpu(my_per_cpu_work, cpu))){
            printk(KERN_ERR "Per_cpu_work Failed to queue work on CPU:%d \n", cpu);
        }
    }

    printk(KERN_INFO "Queue work on all online CPUS\n");
    //指定work调度到cpu
    return 0;
}
//模块退出

static void __exit per_cpu_wq_exit(void) {
    //取消并等待所有的work完成
    printk(KERN_INFO "Cancel work\n");

    cancel_work_sync(&my_work);
    
    printk(KERN_INFO "Cancel percpu work\n");
    int cpu;
    for_each_possible_cpu(cpu) {
        cancel_work_sync(&per_cpu(my_per_cpu_work, cpu));
    }
    flush_workqueue(per_cpu_wq);
    //销毁work_queue
    destroy_workqueue(per_cpu_wq);
    printk(KERN_INFO "Module unloaded\n");
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");


module_init(smp_example_init);
module_exit(per_cpu_wq_exit);