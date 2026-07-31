#define pr_fmt(fmt) "WQTK: " fmt
#include <linux/init.h>
#include <linux/list.h> 
#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/sched.h> 


static int pid = 0;


struct task_struct * get_task_safe(pid_t pid)
{
    struct pid *pid_ptr;
    struct task_struct *task = NULL;
    rcu_read_lock();
    pid_ptr = find_vpid(pid);
    if (pid_ptr) {
        task = pid_task(pid_ptr, PIDTYPE_PID);
        if(task){
            get_task_struct(task); //持有
        }
    }
    rcu_read_unlock();
    return task;
}

void put_task_safe(struct task_struct *task)
{
    if (task) {
        put_task_struct(task);
    }
}

int force_stop_or_resume_process(pid_t pid, int stop) 
{
    struct task_struct *task  = get_task_safe(pid);
    struct kernel_siginfo info;
    int ret;
    int sig =  (stop == 1 ? SIGSTOP : SIGCONT);
    if (!task) {
        pr_err("pid %ld not found\n");
        return -ENOENT;
    }
    //内核线程无mm，不接信号
    if (!task->mm) {
        pr_err("pid %u is kernel tread, sigstop invalid", pid);
        put_task_safe(task);
        return -EINVAL;
    }
    memset(&info, 0, sizeof(info));
    info.si_code = SI_KERNEL; 
    ret = send_sig_info(sig, &info, task);
    pr_info("send SIGSTOP to pid:%d ret:%d", pid, ret);
    put_task_safe(task);
    return ret;
}
 
 
static int set_my_pid(const char *val, const struct kernel_param *kp)
{
    int old_val = pid;
    int ret; 
    ret = param_set_int(val, kp);
  
    if (ret ==0) {
        pr_info("Param pid changed !\n");
        pr_info("Old=%d\n",old_val);
        pr_info("New=%d\n",pid);
        //set_wpid
        if (pid != old_val) {  //先设置的0，那么就把原来的恢复
            force_stop_or_resume_process(old_val, 0);
            force_stop_or_resume_process(pid, 1);
             
        }

    }
    return ret; 
}

static const struct kernel_param_ops pid_ops = {
    .set = set_my_pid,
    .get = param_get_int,       //使用默认的
};
//这里的param_get_int 定义在哪里？

//注册回调用
module_param_cb(pid, &pid_ops, &pid, 0664);

static int __init demo_init(void)
{
    printk(KERN_INFO "WQTK Hello, kernel world\n");
    pr_info("Hello, kernel world\n"); 
    return 0;
}

static void __exit demo_exit(void)
{
    printk(KERN_INFO "WQTK Goodbyd  kernel world");
    pr_info("Goodbyd  kernel world\n"); 
}

MODULE_AUTHOR("Ventrueli");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");

module_init(demo_init);
module_exit(demo_exit);
