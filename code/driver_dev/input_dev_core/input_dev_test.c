#define pr_fmt(fmt) "WQTK: " fmt
#include <linux/module.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/kthread.h>
static struct input_dev *sim_dev;
struct task_struct *pts = NULL;

int MyFunc_KernelThread(void *arg){ 

    allow_signal(SIGKILL);  
    while(!kthread_should_stop()) { 
        
        pr_info("report key\n");
        if(unlikely(!sim_dev)){
             pr_info("bad sim dev \n");
                msleep(1000);
                
                continue;
        }
            
        pr_info("report key down \n");
        input_report_key(sim_dev, KEY_A, 1);
        input_sync(sim_dev);
        msleep(50);
        pr_info("report key up \n");
        input_report_key(sim_dev,KEY_A, 0);
        input_sync(sim_dev);
        
        msleep(4000);

    }  
     pr_info("child exit\n");
    return 0;
}
static int __init input_sim_init(void) {
    int ret;
    sim_dev = input_allocate_device();
    if (!sim_dev) {
        return -ENOMEM;
    }
    sim_dev->name = "input-sim-test";
    sim_dev->phys = "input-sim/input0";
    sim_dev->id.bustype = BUS_VIRTUAL;
    //声明按键
    __set_bit(EV_KEY, sim_dev->evbit);
    __set_bit(KEY_A, sim_dev->keybit); //支持A键盘
    ret = input_register_device(sim_dev);
    if(ret) {
        input_free_device(sim_dev);
        return ret;
    }
    pr_info("input_sim:registerd ,will report KEY_A everty 1 secon\n");
    pts = kthread_create_on_node(MyFunc_KernelThread, NULL, -1, "send_process"); 
    wake_up_process(pts);
    
    pr_info("init exit send\n");
    return 0;
}
static void __exit input_sim_exit(void){
    if (pts) {
        kthread_stop(pts);
        pts = NULL;
    }
    if(sim_dev)
    {    //反注册
        input_unregister_device(sim_dev);
        //释放
         input_free_device(sim_dev);

    }

}


MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("input_sim_init");
MODULE_LICENSE("GPL");

module_init(input_sim_init);
module_exit(input_sim_exit);

// cat /proc/bus/input/devices