#include <linux/mm.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mempool.h>

static struct kmem_cache *pkmem = NULL;
static mempool_t *pmp = NULL;
static void *pobj = NULL;


//这是一个内存池使用slab对象作为alloc和freee函数的
static int __init my_init(void) {
    pkmem = kmem_cache_create("myslabbuffer", 500000, 0, SLAB_HWCACHE_ALIGN, NULL);
    if (NULL == pkmem) {
        pr_info("call kmem_cache_create slab 缓存失败");
    }
    pr_info("调用kmem_cache_creae(...)函数创建slab缓存成功");
    pmp = mempool_create(10, mempool_alloc_slab, mempool_free_slab, pkmem);  //这两个函数是固定的
    if (NULL == pmp) {

        pr_info("call mempool_create(...)函数创建内存池失败");
        return 0;
    }
    pr_info("call mempool_create(...)函数创建内存池成功");
        //开始进行分配neicun 
    pobj = mempool_alloc(pmp, GFP_KERNEL);
    //
    pr_info("调用mempool_alloc()函数分配内存对象成功，对应地址为:%p",pobj);

    return 0;
}

static void __exit my_exit(void) {
    if (NULL != pobj) {
        mempool_free(pobj,pmp); //释放分配的对象
        pr_info("调用mempool_alloc()函数分配内存对象成功，对应地址为");
    }
    if (NULL != pmp) {
        mempool_destroy(pmp); //释放内存池子
        pr_info("mempool_destroy 释放内存池对象成功");
    }
    if (NULL != pkmem) {
        kmem_cache_destroy(pkmem); 
        pr_info("kmem_cache_destroy kmemCache成功");
    }
    printk(KERN_INFO "Goodbyd  kernel world");
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");


module_init(my_init);
module_exit(my_exit);
