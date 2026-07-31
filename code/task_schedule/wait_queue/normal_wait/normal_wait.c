#include <linux/wait.h>

#include <linux/sched.h>
#include <linux/sched/signal.h>

#include <linux/module.h>  
#include <linux/pid.h>  
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/list.h>  

static wait_queue_head_t my_queue;
static int condition = 0;

//唤醒函数
static void wake_me_up(void) {
    printk(KERN_INFO "call wake up \n");
    condition = 1;
    wake_up(&my_queue);  //直接唤醒队列
}


//等待函数
static int  wait_for_condition(void *data){
    DEFINE_WAIT(wait);
    add_wait_queue(&my_queue, &wait);//加入等待队列
    while(!condition) {   ///这里主要是方式虚假唤醒，比如被别的信号什么唤醒了。而且兼职处理了信号
        
        printk(KERN_INFO "before prepare to wait\n");
        prepare_to_wait(&my_queue, &wait, TASK_INTERRUPTIBLE);  //加入队列设置状态
        if(signal_pending(current)) {
            printk(KERN_INFO "signal received,exiting wait\n");
            break;
        }
        schedule();
    }
    pr_info("finish wait");
    finish_wait(&my_queue, &wait);
    pr_info("after_finish wait");
    return 0;
}

static int __init my_module_init(void) {
    init_waitqueue_head(&my_queue);
    condition = 0;
    //启动一个内核线程等待条件 
    kthread_run(wait_for_condition,(void *)(long)1, "wait_thread");  //自动就wake_up了
    //演示3000
    msleep(2000);
    wake_me_up();
    return 0;
}
static void __exit my_module_exit(void)
{
    // 清理
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");

module_init(my_module_init);
module_exit(my_module_exit);
