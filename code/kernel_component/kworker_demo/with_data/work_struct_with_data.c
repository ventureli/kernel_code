#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/slab.h>  //kmalloc
#include <linux/init.h>

//自定义结构体
struct my_data {
    int id;
    char msg[32];
    struct work_struct work;
};

static void my_work_handler(struct work_struct *work) {
    //1.反向拿到数据
    
    int cpu = smp_processor_id();
    struct my_data *data = container_of(work, struct my_data, work);
    pr_info("working struct runing current->pid:%d, cpu:%d(id=%d, msg=%s)", current->pid, cpu, data->id, data->msg);
}


static struct my_data * g_data;
static int __init demo_init(void) {
    pr_info("work_struct with data init\n");
    g_data = kmalloc(sizeof(*g_data), GFP_KERNEL);
    if(!g_data){
        return -ENOMEM;
    }
    g_data->id = 10086;
    snprintf(g_data->msg, sizeof(g_data->msg),"hello workqueue");
    INIT_WORK(&(g_data->work),  my_work_handler);
    schedule_work(&(g_data->work));
    return 0;
}

static  void __exit demo_exit(void) {
    //1.先把队列的里的所有的work都执行完
    //flush    
    pr_info("work_struct with data unloaded\n");
    flush_scheduled_work();
    kfree(g_data);
    g_data = NULL;
    //释放
}


MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("hello_kworker_demo_exit");
MODULE_LICENSE("GPL");

module_init(demo_init);
module_exit(demo_exit);
