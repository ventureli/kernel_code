#define pr_fmt(fmt) "WQTK: " fmt

//备注，这个方法在5.10的操作系统中没八法测试，没有maple_tree这个头我呢见
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/maple_tree.h>

#define MODULE_LICENSE_VALUE "GPL"
#define MODULE_AUTHOR_VALUE "liwenqinag"
#define MODULE_DESC_VALUE "Maple Tree Demo Module"

MODULE_LICENSE(MODULE_LICENSE_VALUE);
MODULE_AUTHOR(MODULE_AUTHOR_VALUE);
MODULE_DESCRIPTION(MODULE_DESC_VALUE);

struct demo_entry {
    unsigned long key;
    char value[32];
};

//全局Maple Tree 指针

static struct maple_tree demo_tree;

static int __init maple_demo_init(void) {
    struct demo_entry *entry;
    int i; 
    
    mt_init(&demo_tree); //默认标志位初始化

    // 
    pr_info("Maple Tree Demo Module Loaded\n");
    //插入10个测试tiaomu
    for (i = 0; i < 10; i++) {
        entry = kmalloc(sizeof(struct demo_entry), GFP_KERNEL);
        if (!entry) {
            return -ENOMEM;
        }
        entry->key = i;
        snprintf(entry->value, sizeof(entry->value), "Entry-%d",i);
        //使用matree_strore 插入数据,覆盖条目
        if (mtree_store(&demo_tree, i , entry, GFP_KERNEL)) {
            kfree(entry);
            pr_err("Failed to insert entry %d\n",i);
            return -EFAULT;
        }
        pr_debug("Inserted key %lu:%s\n", entry->key, entry->value);
    }

    unsigned long new_index;

    mt_for_each(&demo_tree, item_entry,new_index,ULONG_MAX) {
        struct demo_entry *e = item_entry;
        pr_info("Use new Index:%lu  Value:%s\n",new_index , e->value);
    }

    return 0;
}

//请函数
static void __exit maple_demo_exit(void) {
    void *item_entry;
    unsigned long new_index;
    mt_for_each(&demo_tree, item_entry,new_index,ULONG_MAX) {
        struct demo_entry *e = item_entry;
        pr_info("kfree  Index:%lu  Value:%s\n",new_index , e->value);
        kfree(e);
    }
    //销毁Maple Tree
    mtree_destroy(&demo_tree);
    pr_info("Maple Tree Destroyed\n");
}

module_init(maple_demo_init);
module_exit(maple_demo_exit);


