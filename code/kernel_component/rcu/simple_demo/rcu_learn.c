#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>

//
struct rcu_test_data {
    int value;
    struct rcu_head rcu;   //关键：添加RCU延迟释放用的成员
};

//被RCU保护的全局指针
static struct rcu_test_data __rcu *g_data;  //__RCU标志

/**
 * 模拟读线程：RCU读端临界区
*/
static void rcu_reader(void) {
    struct rcu_test_data *p;
    //* 进入RCU读端临界区
    rcu_read_lock();
    /**
     * rcu_dereference():安全读取被RCU保护的指针
     * 告诉编译器这是RCU安全的访问
    */
   p = rcu_dereference(g_data);
   if (p) {
        pr_info("RCU reader:value =%d\n", p->value);
   } else {
        pr_info("RCU read:datais NULL");
   }
   //退出RCU 读端临界区
   rcu_read_unlock();
}

/**
 * 模拟“写线程”：更新RCU保护的数据
 * 
*/
static int rcu_writer(void  *arg) {
    struct rcu_test_data *old_data;
    struct rcu_test_data *new_data;
    /*分配新数据*/
    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
    if (!new_data) {
        return -ENOMEM;
    }
    new_data->value = 890;
    pr_info("RCU writeer:create new data, value =%d\n", new_data->value);
    /**
     * rcu_assign_pointer()原子更新指针，保证其他CPU可见顺序
    */
    old_data = rcu_replace_pointer(g_data, new_data, rcu_read_lock_held());
    pr_info("RCU writer: wait all reades leae");
    synchronize_rcu();
    /**
    * synchrnoze_rcu():等待所有已存在的RCU读端临界区结束
    * 之后释放old_data;
    */
    if (old_data) {
        pr_info("RCU writer:free old data\n");
        kfree(old_data);
    }

    return 0;
        
}

/**
 * 使用kfree_rcu()例子
*/

//**使用kfree_rcu()例子（更推荐）
static int rcu_writer_kfree(void) {
    struct rcu_test_data *old_data;
    struct rcu_test_data *new_data;
    /*分配新数据*/
    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
    if (!new_data) {
        return -ENOMEM;
    }
    new_data->value = 649;
    pr_info("RCU writeer:create new data, value =%d\n", new_data->value);
    /**
     * rcu_assign_pointer()原子更新指针，保证其他CPU可见顺序
    */
    old_data = rcu_replace_pointer(g_data, new_data, rcu_read_lock_held());

    // synchronize_rcu();
    /**
    * synchrnoze_rcu():等待所有已存在的RCU读端临界区结束
    * 之后释放old_data;
    */
    if (old_data) {
        pr_info("RCU writer:use kfree_rcu to free old data\n");
        // kfree(old_data);
        kfree_rcu(old_data,rcu);
    }
    return 0;
}

static int __init rcu_learn_init(void) {
    pr_info("RCU learn module init\n");
    /**初始化数据*/
    g_data = kmalloc(sizeof(*g_data), GFP_KERNEL);
    if (g_data) {
        g_data->value =  100;
        pr_info("Init data:value = 100\n");
    }
    /*1.先读一次*/
    rcu_reader();
    /**2.写一次（同步等待）*/
    rcu_writer(NULL);
    /**3.再写一次*/
    rcu_writer_kfree();
    rcu_reader();
    return 0;
}

static void __exit rcu_learn_exit(void) {
    struct rcu_test_data *p;
    p = rcu_dereference_protected(g_data, 1);
    if (p) {
        kfree(p);
    }
    pr_info("RCU learn module exit\n");
}



// 模块卸载函数（__exit）是在进程上下文中运行的。
// 内核会保证：在调用 __exit 函数时，已经没有任务在运行该模块的代码。
// 没有新的进程会进入该模块的代码路径。
// 这是通过内核的模块卸载机制保证的，不会在模块卸载过程中发生并发访问
//如果担心的话可以使用
//static void __exit rcu_learn_exit(void) {
//     struct rcu_test_data *p;

//     p = rcu_dereference_protected(g_data, 1);
//     if (p) {
//         call_rcu(&p->rcu, kfree);
//     }

//     pr_info("RCU learn module exit\n");
// }


module_init(rcu_learn_init);
module_exit(rcu_learn_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YOUR NAME");
MODULE_DESCRIPTION("IOCTL module without modifying the kernel");

