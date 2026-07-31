#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/kthread.h>

struct task_struct *pts = NULL;

int MyFunc_ThreadFunc(void *args) {
    printk("\t---调用线程函数 MyFunc_ThreadFunc(...)\n");
    printk("\t---打印输出当前的进程PID:%d", current->pid);
    printk("\t---退出调用线程函数 MyFunc_ThreadFunc(...)\n");
    return 0;
}

static int __init KthreadCreateOnNode_Func(void) {
  
    printk("调用函数KthreadCreateOnNode_Func(....)函数");
     
    pts = kthread_create_on_node(MyFunc_ThreadFunc,NULL,-1,"ktconnode.c");
    printk("打印新线程PID的数值为:%d\n", pts->pid);

    wake_up_process(pts); 
    printk("打印当前线程PID的数值为:%d\n", current->pid);
    
    printk("退出函数KthreadCreateOnNode_Func(....)函数");

    return 0;
}
// wake_up_process(pts); //唤醒新创建的内核线程。 //哟关于把睡眠态变成就绪态度。

static void __exit kThreadExitOnNode_Func(void) {
    // kthread_stop(pts);
    printk("正常退出内核：函数kthread_create_on_node(....)函数");
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("test  kthread_create_on_node");
MODULE_LICENSE("GPL");
module_init(KthreadCreateOnNode_Func);
module_exit(kThreadExitOnNode_Func);
