#include <linux/init.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/smp.h>
//1.定义工作队列和工作项目
//推荐使用系统默认队列（对应ps里的events）最常用

static struct workqueue_struct *my_wq;
static struct delayed_work my_work;  //定义work

#define USE_SYS_WQ false
#define USE_BOUND true

//定义kwork func
static void demo_work_handler (struct work_struct  *work) {
    //打印当前的任务的kworker信息
    static int count = 0; 
    int cpu = smp_processor_id();
    printk(KERN_INFO "【Demo】 [count:%d] 我在 kworker里执行啦。当前进程:%s (pid:%d) cpu:%d \n", count, current->comm, current->pid, cpu);
    //岩石，重新提交自己，实现循环
    
    if(count < 50) {
        count ++;
        if(count %3 == 0)
        { 
            queue_delayed_work_on(0, my_wq, &my_work, msecs_to_jiffies(2000));  //jiffies 是启动到当前的时钟中断数量。
        } else if (count %3 == 1)
        { 
            queue_delayed_work_on(1, my_wq, &my_work, msecs_to_jiffies(2000));  //jiffies 是启动到当前的时钟中断数量。
        }else{
            //这里不指定，就会用上一次的对应的cpu的queue，这个是测试出来的。
            queue_delayed_work(my_wq, &my_work, msecs_to_jiffies(2000));  //jiffies 是启动到当前的时钟中断数量。
        }
    } else {
        printk(KERN_INFO "【Demo】 finish worker 进程:%s (pid:%d) cpu:%d\n",  current->comm, current->pid, cpu);
    }
}

//模块初始化
static int __init kworker_demo_init(void) {
    printk(KERN_INFO "【Demo】 模块加载成功\n");
    //方式A。使用系统共享队列
    //my_wq = system_wq;
    //方式B。创建自己的专属队列（对应ps里的my_wq）
    if (USE_SYS_WQ) {
        my_wq = system_wq;
    } else { 
        my_wq = alloc_workqueue("my_wq", (USE_BOUND ? 0 : WQ_UNBOUND), 1);
    }
    if (!my_wq) {
        printk(KERN_ERR "【Demo】创建工作队列失败！\n");
        return -ENOMEM;
    }
    //初始化工作项，绑定处理函数
    INIT_DELAYED_WORK(&my_work, demo_work_handler);
    //提交工作，立刻执行一次
    queue_delayed_work(my_wq, &my_work, 0);
    // queue_delayed_work_on(0,my_wq, &my_work, 0); //指定再cpu0 上完成。
    return 0;
}

//清理资源
static void __exit kworker_demo_exit(void) {
    //刷新并停驶队列，确保没有任务再跑
    flush_workqueue(my_wq);
    //销毁自己创建队列，（如果使用的system_wq,不要销毁）
    if (!USE_SYS_WQ) {
        cancel_delayed_work_sync(&my_work);
        destroy_workqueue(my_wq);     
        pr_info("my_wq destory");
    }
    printk(KERN_INFO "【Demo模块卸载成功】");
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("hello_kworker_demo_exit");
MODULE_LICENSE("GPL");

module_init(kworker_demo_init);
module_exit(kworker_demo_exit);
