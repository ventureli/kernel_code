
#define pr_fmt(fmt) "WQTK: " fmt
#include <linux/module.h>
#include <linux/hashtable.h>
#include <linux/list.h> 
#include <linux/hash.h> 
#include <linux/slab.h> 
#include <linux/list_bl.h>

//1024个桶,带有bit lock bl 带博爱bit-clock

#define HT_BITS 10
#define HTSIZE (1 << HT_BITS)
    
static struct hlist_bl_head my_htbl[ HTSIZE ];

struct my_obj {
    int id;
    struct hlist_bl_node node; //必须用hlist_bl_node
};

//插入几个元素
static void ht_insert(int param_id){
    //申请一个节点,这个很重要
    unsigned int hash;
    struct my_obj *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
    obj->id = param_id;

    hash  =  hash_32(param_id, HT_BITS);
    hlist_bl_lock( &my_htbl[hash]);
    hlist_bl_add_head(&obj->node, &my_htbl[hash]);
    hlist_bl_unlock( &my_htbl[hash]);
    pr_info("WQTK Added value %d to bucket %u",param_id, hash);
    
}


static int __init hlist_bl_init(void) {
    int i;
    for (i = 0; i < HTSIZE; i++ ) {
        INIT_HLIST_BL_HEAD(&my_htbl[i]);
    }
    printk(KERN_INFO "Hello, hash table demo init\n");
    struct my_obj *obj;
    struct hlist_bl_node *node, *tmp;
    unsigned  int bkt;
    ht_insert(10);
    ht_insert(20);
    ht_insert(30);
    ht_insert(21);
    ht_insert(31);
    ht_insert(30);
    ht_insert(31);
    pr_info("===========遍历整个HASH table=======");
    //安全遍历
    for(bkt = 0 ; bkt< HTSIZE; bkt++) {
        hlist_bl_lock( &my_htbl[bkt]);
        hlist_bl_for_each_entry_safe(obj, node, tmp, &my_htbl[bkt], node) {
                pr_info("桶 %u :obj id = %d\n",bkt, obj->id);
        }
        hlist_bl_unlock( &my_htbl[bkt]);
    }

 
    return 0;
}


static void __exit hlist_bl_exit(void) {
    printk(KERN_INFO "Goodbyd  hash table demo");
    struct  my_obj * obj;
    struct hlist_bl_node *node, *tmp;
    unsigned int bkt;
    //安全删除元素
    //这里 很奇快的 是要把,这个node传入两次,
     for(bkt = 0 ; bkt < HTSIZE; bkt ++) {
        hlist_bl_for_each_entry_safe(obj, node, tmp, &my_htbl[bkt], node) {
            pr_info("桶 %u : free obj id = %d\n",bkt, obj->id);
            hlist_bl_del(&obj->node);
            kfree(obj);
               
        }
    }
  
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");


module_init(hlist_bl_init);
module_exit(hlist_bl_exit);


