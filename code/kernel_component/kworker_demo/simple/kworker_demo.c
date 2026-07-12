#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>

static struct delayed_work kworker_demo_work;

static void kworker_demo_func(struct work_struct *work) {
    static int count = 0;
    pr_info("kworker demo:run count = %d\n", count++);
    //循环五次停止
    if(count < 5) {
        schedule_delayed_work(&kworker_demo_work, msecs_to_jiffies(1000));  //再次提交一个worker,延迟执行 ,相当于每次启动后又进行了一次加入，
    } else {
        pr_info("kwork_demo :worker down exit\n");
    }
}

static int __init kworker_demo_init(void) {
    pr_info("kworker demo:init \n");
    INIT_DELAYED_WORK(&kworker_demo_work, kworker_demo_func);
    schedule_delayed_work(&kworker_demo_work, msecs_to_jiffies(0));  //再次提交一个worker,延迟执行 ,相当于每次启动后又进行了一次加入，
    return 0;  //立刻执行一次
}

static void __exit kworker_demo_exit(void) {
    //确保推出前一ing取消了所有的worker

    cancel_delayed_work_sync(&kworker_demo_work);
    pr_info("kworker demo :exit");
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");


module_init(kworker_demo_init);
module_exit(kworker_demo_exit);
