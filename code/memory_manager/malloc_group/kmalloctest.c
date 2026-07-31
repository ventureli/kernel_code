#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/init.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

static int __init my_init(void);
static void __exit my_exit(void);

#define MEM_MAX_SIZE 10240
char *pmem = NULL;
char *vpmem = NULL;

static int __init my_init(void) {

    pmem = (char *)kmalloc(MEM_MAX_SIZE, GFP_KERNEL);
    if (pmem == NULL) {
        pr_err("call kmalloc get 连续内存失败");
        return 0;
    }
    
    printk(KERN_INFO "call kmalloc get 连续内存成功 %p\n", pmem);
    vpmem = (char *)vmalloc(MEM_MAX_SIZE);
    if (vpmem == NULL) {
        pr_err("call vmalloc get 连续内存失败");
        return 0;
    }
    
    printk(KERN_INFO "call vmalloc get 连续内存成功 %p\n", pmem);
    return 0;
}

static void __exit my_exit(void) {
    // printk(KERN_INFO "Goodbyd  kernel world");
    if (NULL != pmem) {
        kfree(pmem);
        pr_info("调用kfree 成功");
    }
    if (NULL != vpmem) {
        vfree(vpmem);
        pr_info("调用vfree 成功");
    }
}




MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");


module_init(my_init);
module_exit(my_exit);
