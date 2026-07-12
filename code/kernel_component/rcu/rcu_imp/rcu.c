#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/kthread.h>
#include <linux/delay.h>

struct ftestrcu {
    int a;
    struct rcu_head rcu;
};

static struct ftestrcu *g_ptr;

static int myrcu_reader_thread_1(void *data) {
    struct ftestrcu *p1 = NULL;
    while (1) {
        if(kthread_should_stop()) {  //是否需要退出 
            printk("%s: stoped go exited", __func__);
            break;
        }
        msleep(10);
        rcu_read_lock();
        // mdelay(100);
        p1 = rcu_dereference(g_ptr);  //p1指向旧的数据
        if (p1) {
            printk("%s: Read1 a = %d", __func__, p1->a);
        }
        rcu_read_unlock(); 
    }
    return 0;
}

static int myrcu_reader_thread_2(void *data) {
    struct ftestrcu *p2 = NULL;
    while (1) {
        if(kthread_should_stop()) {  //是否需要退出
            printk("%s: stoped go exited", __func__);
            break;
        }
        msleep(20);
        rcu_read_lock();
        // mdelay(200);
        p2 = rcu_dereference(g_ptr);  //p1指向旧的数据
        if (p2) {
            printk("%s: Read2 a = %d", __func__, p2->a);
        }
        rcu_read_unlock();
    } 
    return 0;
}

static void myrcu_del(struct rcu_head *rh) {
    struct ftestrcu *p = container_of(rh, struct ftestrcu, rcu);
    printk("%s: a=%d\n", __func__, p->a);
    kfree(p); //删除rcu变量
}

static int myrcu_write_thread(void *p) {
    struct ftestrcu *old;
    struct ftestrcu *new_ptr;
    int value = (unsigned long)p;
    while (1) {
        if (kthread_should_stop()) {
            printk("%s: stoped go exited", __func__);
            break;
        }
        msleep(200);
        new_ptr =  kmalloc(sizeof(struct ftestrcu), GFP_KERNEL);
        old = rcu_dereference(g_ptr);
        if (old) {
            *new_ptr = *old;
        } else {
            new_ptr->a = 0;  // 默认值
        }
        
        new_ptr->a = value;
        smp_wmb();  // 写内存屏障

        rcu_assign_pointer(g_ptr, new_ptr);
        call_rcu(&old->rcu, myrcu_del);
        printk("%s:Write to new %d\n", __func__, value);
        value ++;
    }
    return 0;
}

static struct task_struct *read_thread1;
static struct task_struct *read_thread2;
static struct task_struct *writer_thread;

static int __init myrcu_init(void) {
    int value  = 5;
    pr_info("\n\nRcu Module Init OK.\n");
    g_ptr = kzalloc(sizeof(struct ftestrcu), GFP_KERNEL);
    read_thread1 =  kthread_run(myrcu_reader_thread_1, NULL, "Rcu_Read_1");
    read_thread2 =  kthread_run(myrcu_reader_thread_2, NULL, "Rcu_Read_2");
    writer_thread  =  kthread_run(myrcu_write_thread, (void *)((unsigned long)value), "Rcu_writer");

    return 0;
}

static void __exit myrcu_exit(void) {
    pr_info("RCU Unloading Mdule exit.\n\n");
    kthread_stop(read_thread1);
    kthread_stop(read_thread2);
    kthread_stop(writer_thread);
    
    synchronize_rcu();
    if (g_ptr) {
        kfree(g_ptr);
    }
}

module_init(myrcu_init);
module_exit(myrcu_exit);

MODULE_AUTHOR("li");
MODULE_LICENSE("GPL");
