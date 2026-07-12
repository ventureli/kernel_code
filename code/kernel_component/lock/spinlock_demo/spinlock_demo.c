#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>


//共享资源
static int shared_counter = 0;

//互斥锁
static spinlock_t my_spinlock;


//fun
static int thread_func(void *data) {
    
    while (!kthread_should_stop()) {
        //枷锁    
        spin_lock(&my_spinlock); // 获取自旋锁 
        pr_info("Thread %d:current->pid:%d Locked,shared_counter = %d\n",(int)(long)data, current->pid , shared_counter);

        shared_counter ++;
        pr_info("Thread %d:Locked,Increated to :%d\n",(int)(long)data, shared_counter);

        //解锁
        spin_unlock(&my_spinlock); // 释放自旋锁
        msleep(1000);  
    }
    return 0;
}
static struct task_struct *thread1, *thread2;

//init
static int __init my_init(void) {
    pr_info("Mutex module loaded\n");
    spin_lock_init(&my_spinlock);
    thread1 = kthread_run(thread_func,(void *)(long)1, "mutex_thread1");
    thread2 = kthread_run(thread_func,(void *)(long)2, "mutex_thread2");
    return 0;
}

//模块卸载
static void __exit my_exit(void) {
    kthread_stop(thread1);
    kthread_stop(thread2);
    pr_info("Mutex moudule unloaded\n");
}

MODULE_LICENSE("GPL");
module_init(my_init);
module_exit(my_exit);