
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>


//定义并初始化定时器
static struct timer_list my_timer;

void my_timer_callback(struct timer_list *t) {
    pr_info("Timer expired at jiffies:%lu\n",jiffies);
    my_timer.expires =  jiffies + msecs_to_jiffies(2000);
    add_timer(&my_timer);
}

static int __init timer_init(void) {
    pr_info("Timer moudlue loaded\n");
    //初始化定时器
    timer_setup(&my_timer, my_timer_callback, 0);
    //设置定时器到期时间（例如5秒触发）
    my_timer.expires =  jiffies + msecs_to_jiffies(5000);
    //添加定时器到内核
    add_timer(&my_timer);
    return 0;
}

static void __exit timer_exit(void) {
    //删除定时器
    del_timer(&my_timer);
    pr_info("Timer module unloaded");
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("test  wakeup");
MODULE_LICENSE("GPL");

module_init(timer_init);
module_exit(timer_exit);