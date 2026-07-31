#include <linux/wait.h>
#include <linux/sched.h> 
#include <linux/sched/signal.h> 
#include <linux/module.h>  
#include <linux/pid.h>  
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/list.h>  

#define MAX_DATA_LENGTH 10  //最多生产这么多
wait_queue_head_t my_queue;
int data_ready = 0;



DEFINE_MUTEX(my_mutex);

//一个生产者，一个消费者
int producer(void *data)
{
    // 数据就绪，继续执行
    while(!kthread_should_stop()) { 
        wait_event_interruptible(my_queue, data_ready < MAX_DATA_LENGTH || kthread_should_stop());
        mutex_lock(&my_mutex);
        smp_rmb();
        if(data_ready < MAX_DATA_LENGTH){  //这里判断下主要是因为有可能是其他元素引起的唤醒
            data_ready += 1;
            smp_wmb();
            pr_info("模拟生产一个元素，并设置data——ready = %d",data_ready);
            wake_up(&my_queue);
        }
        mutex_unlock(&my_mutex);
        msleep(10);
    }   
    pr_info("producer exit 0");
    return 0;
}
int consumer(void *data)
{
    while(!kthread_should_stop()) { 
        wait_event_interruptible(my_queue, data_ready > 0 ||  kthread_should_stop());
        mutex_lock(&my_mutex);
        smp_rmb();
        if(data_ready > 0 ){  //这里判断下主要是因为有可能是其他元素引起的唤醒
            data_ready -= 1;
            smp_wmb();
            pr_info("模拟消费一个元素，并设置data——ready = %d",data_ready);
            //
            wake_up(&my_queue);
        }
        mutex_unlock(&my_mutex);
        msleep(10);
    }  
    pr_info("producer exit 1");
    return 0;
}

static struct task_struct *thread_producer, *thread_consumer;

//init
static int __init my_module_init(void) {
    pr_info("Mutex module loaded\n");
    init_waitqueue_head(&my_queue); //初始化等待队列元素
    thread_producer = kthread_run(producer,(void *)(long)1, "produce_thread1");
    thread_consumer = kthread_run(consumer,(void *)(long)2, "consumer_thread2");
    return 0;
}
static void __exit my_module_exit(void)
{
    kthread_stop(thread_producer);
    kthread_stop(thread_consumer);
    pr_info("Mutex moudule unloaded\n");
    // 清理
}
MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");

module_init(my_module_init);
module_exit(my_module_exit);
