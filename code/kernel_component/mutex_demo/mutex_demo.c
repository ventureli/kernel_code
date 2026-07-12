#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>

//共享资源
static int shared_counter = 0;

//互斥锁
static struct mutex my_mutex;


//fun
static int thread_func(void *data) {
    while (!kthread_should_stop()) {
        //枷锁
        mutex_lock(&my_mutex);
        pr_info("Thread %d:Locked,shared_counter = %d\n",(int)(long)data, shared_counter);

        shared_counter ++;
        pr_info("Thread %d:Locked,Increated to :%d\n",(int)(long)data, shared_counter);

        //解锁
        mutex_unlock(&my_mutex);
        msleep(1000);  
    }
    return 0;
}
static struct task_struct *thread1, *thread2;

//init
static int __init my_init(void) {
    pr_info("Mutex module loaded\n");
    mutex_init(&my_mutex);
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