#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/wait.h>    
#include <linux/delay.h>
#include <linux/list.h>

static struct task_struct * parent_thread;
static struct task_struct *child_task; 
static struct completion cp;
//自定义内和县城函数

int my_func_thread(void *arg) {
    printk("\t...call my_func_thread");
    printk("\t...current pid:%d",current->pid);
    printk("\t...current done:%d",cp.done);
    printk("\t...parent state :%d",parent_thread->__state);
    //调用函数，唤醒进程并且更改done字段的数值
    msleep(2000);
    complete_all(&cp);
    //|complete(completion)	    | 唤醒一个等待的线程（FIFO 顺序）|	用于单个线程完成同步|
    // |complete_all(completion)   |唤醒所有等待的线程 |	用于通知多个等待者任务完成|
    printk("\t...current done:%d",cp.done);
    printk("\t...parent state :%d",parent_thread->__state);
    printk("\t...exit my_func_thread");
    return 0;
}

static int __init completion_init_func(void) {

    long left_time;
    printk("call completion_init_func(...)");
    parent_thread = current;

    init_completion(&cp); ///init
    child_task = kthread_create_on_node(my_func_thread, NULL, -1, "complete_all_test");
    //wake_up_process
    wake_up_process(child_task);
    // wait_for_completion(&cp); 
    //使得等待队列进程不可中断的的等待状态
    left_time = wait_for_completion_timeout(&cp, msecs_to_jiffies(10000));

    printk("time out :%ld", left_time);
    printk("exit completion_init_func(...)");
    printk("current->pid:%d", current->pid);
    printk("child_task->pid:%d", child_task->pid);
    

    return 0;
}

static void __exit completion_exit_func(void) {
    printk("normal exit ");
    if (child_task &&  !IS_ERR(child_task)) {
        kthread_stop(child_task);
    }
    printk("normal exit completion_cexit_func");
}
 

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("test  completion");
MODULE_LICENSE("GPL");

module_init(completion_init_func);
module_exit(completion_exit_func);