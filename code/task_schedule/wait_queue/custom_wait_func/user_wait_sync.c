#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/wait.h>

static  wait_queue_head_t head;
static struct task_struct *thread1;

static int thread_func(void *data) {
    pr_info("thread_func called  before sleep:current->pid:%d",current->pid);
    msleep(1000);   
    pr_info("thread_func called  wakeup_process:current->pid:%d",current->pid);
    wake_up(&head);  //唤醒等待队列
    pr_info("thread_func called  after wakeup_process:current->pid:%d",current->pid);
    return 0;
}
int my_wake_function(wait_queue_entry_t *entry, unsigned mode, int sync, void *key)
{
    struct task_struct *p = (struct task_struct *)(entry->private);  //这个private才是目标的
    printk(KERN_INFO "Custom wake function called for task %d current:%d\n", p->pid, current->pid);

    // 手动唤醒进程
    return wake_up_process(p);
}

void custom_wait_and_wake(wait_queue_head_t *queue)
{
    //这个代码移植是TASK_RUNNING
    wait_queue_entry_t wait;
    init_wait(&wait);  //定义一个wait
    wait.func = my_wake_function;   //这里定义了一个func，但是这个func，比不是在目标进程执行的
    printk(KERN_INFO "add currrent to wait_queue current->pid:%d", current->pid);
    set_current_state(TASK_INTERRUPTIBLE);   //这里先进行睡眠 这里由于是自己唤醒自己，不能这么设置
    add_wait_queue(queue, &wait);
    
    printk(KERN_INFO "before schedule current->pid:%d", current->pid);
    schedule(); // 让出CPU ,把自己放在尾部 
    printk(KERN_INFO "after schedule get cpu again current->pid:%d", current->pid);
    //唤醒后先回复状态在出列
    __set_current_state(TASK_RUNNING);
    remove_wait_queue(queue, &wait);   
}

static int __init my_init(void) {
    printk(KERN_INFO "Hello, kernel world\n");
    init_waitqueue_head(&head); //初始化等待队列元素
    thread1 = kthread_run(thread_func,(void *)(long)1, "testTHread");  //自动就wake_up了


    custom_wait_and_wake(&head);
    return 0;
}

static void __exit my_exit(void) {
    printk(KERN_INFO "Goodbyd  kernel world");
}



MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");


module_init(my_init);
module_exit(my_exit);
