#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/pid.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/err.h>

struct task_struct *pts_parent = NULL;

struct task_struct *pts = NULL;

struct thread_param{
    int pid;
    char name[TASK_COMM_LEN];
};
int MyFunc_KernelThread(void *arg){


    allow_signal(SIGKILL);

    struct thread_param *param = (struct thread_param *)arg;

    printk("\t...enter MyFunc_KernelThread(...)\n");

    printk("\t...current thread static_prio:%d\n",current->static_prio);
    printk("\t...current thread nice:%d\n",task_nice(current));
    printk("\t...current thread pid:%d\n",(current->pid));
    printk("\t...wake up parent:parent->pid(%d), pts_parent->pid:%d param->pid %d name :%s \n",current->parent->pid, pts_parent->pid, param->pid, param->name);

    struct task_struct  *parent = find_task_by_vpid(param->pid);
    if (!parent) {
        printk("\t...cannot find parent\n");
        return 0;
    }
    get_task_struct(parent);
    wake_up_process(parent);
    put_task_struct(parent);
    while(!kthread_should_stop()) {

        set_current_state(TASK_INTERRUPTIBLE);
        if(kthread_should_stop()){
            break;
        }
        schedule();
    } 
    int time_out = 0;
    // int tise_timeout_uninterruptible(1000*10);

    printk("\t...some one set stop signal\n");
    set_current_state(TASK_RUNNING);
    printk("\t...exit MyFunc_KernelThread(...) time out:%d\n",time_out);
    return 0;
}

///init module

static int __init tasknice_initfunc(void) {

    printk("enter tasknice_initfunc(...)\n");
    pts_parent = current;

    int inice;
    struct thread_param param;
    param.pid = current->pid;
    
    get_task_comm(param.name ,current);
    pts = kthread_create_on_node(MyFunc_KernelThread, &param, -1, "taskniceest2");
 
    inice = task_nice(pts);
    printk("new process(%d) nice value %d\n",pts->pid, inice);
    printk("current process(%d) nice value %d\n",current->pid, task_nice(current));

    wake_up_process(pts);
    // printk("set parent to me\n");

    //
    // task_lock(pts);
    // task_lock(current);
    // // __set_task_real_parent(pts, current);
    // pts->real_parent = current;
    // pts->parent = current;
    // task_unlock(current);
    // task_unlock(pts);
    
    printk("current sleep\n");
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();   //yeld cpu
    printk("enter tasknice_initfunc aggain\n");
    printk("exit tasknice_initfunc(...)\n");
    return 0;
}
//exit module
static void __exit tasknice_exitfunc(void) {
    printk("exit module subchilds %d\n",pts->pid);
    if (pts) {
        kthread_stop(pts);
    }
    pts = NULL;
    pts_parent = NULL;
    printk("exit module tasknice_exitfptsunc\n");
}

MODULE_LICENSE("GPL");

module_init(tasknice_initfunc);
module_exit(tasknice_exitfunc);