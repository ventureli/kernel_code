#define pr_fmt(fmt) "WQTK: " fmt
#include <linux/init.h>
#include <linux/list.h> 
#include <linux/module.h>
#include <linux/kernel.h>  
#include <linux/signal.h> 
#include <linux/sched.h> 
#include <linux/sched/signal.h> 
#include <linux/cred.h> 


static int pid = 0;


#define SIG_CUSTOM_MSG 42  //通常40~50的的信号比较安全不会和系统预定义的冲突
// #define SIG_CUSTOM_MSG (SIGRTMIN + 10)
static int set_my_pid(const char *val, const struct kernel_param *kp)
{
    int old_val = pid;
    int ret;
    struct task_struct *task = NULL;
    struct kernel_siginfo info = {
        .si_signo = SIG_CUSTOM_MSG,
        .si_errno = 0,
        .si_code = SI_KERNEL,
        .si_pid = current->pid,
        .si_uid = current->cred->uid.val,
    }; 
    ret = param_set_int(val, kp);
    if (ret ==0) {
        pr_info("Param pid changed !\n");
        pr_info("Old=%d\n",old_val);
        pr_info("New=%d\n",pid);
        //set_wpid
        task = pid_task(find_vpid(pid), PIDTYPE_PID);
        if (!task) {
            pr_err("Pid is Error");
            return -1;
        } 
       
        pr_info("Send sig =%d\n",SIG_CUSTOM_MSG);
        if (send_sig_info(SIG_CUSTOM_MSG, &info, task)) {
            return -EFAULT;
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

static int __init hello_init(void) {
    printk(KERN_INFO "WQTK Hello, kernel world\n");
    pr_info("Hello, kernel world\n");
 
    return 0;
}

static void __exit hello_exit(void) {
    printk(KERN_INFO "WQTK Goodbyd  kernel world");
    pr_info("Goodbyd  kernel world\n");
 
}

MODULE_AUTHOR("Ventrueli");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");

module_init(hello_init);
module_exit(hello_exit);
