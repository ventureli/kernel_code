#define pr_fmt(fmt) "WQTK: " fmt
#include <linux/module.h>
#include <linux/hashtable.h>
#include <linux/list.h> 
#include <linux/hash.h> 
#include <linux/slab.h> 
#include <linux/list.h>
 

#define HT_BITS 10
#define HTSIZE (1 << HT_BITS)
    
static struct hlist_head my_htbl[ HTSIZE ];

struct my_obj {
    int id;
    char name[32];
    struct hlist_node node; //必须用hlist_bl_node
};

//插入几个元素
static void ht_insert(int param_id, const char *name){
    //申请一个节点,这个很重要
    unsigned int hash;
    struct my_obj *obj = kmalloc(sizeof(*obj), GFP_KERNEL);
    if (!obj) {
        //pr_err
        return;
    }
    obj->id = param_id;
    // obj->name[31] = '\0';
    strscpy(obj->name, name,sizeof(obj->name));
    hash  =  hash_32(param_id, HT_BITS);
    hlist_add_head(&obj->node, &my_htbl[hash]);
    pr_info("WQTK Insert value %d %s to bucket %u",param_id, name, hash);
}

static struct my_obj *ht_look_up(int id){
    struct my_obj *data; 
    unsigned int hash;

    hash = hash_32(id, HT_BITS);
    //遍历对应的桶临边
    hlist_for_each_entry(data, &my_htbl[hash], node){
        if(data->id == id) {
            pr_info("WQTK Found:id=%d name = %s\n", data->id, data->name);
            return data;
        }
    }
    pr_info("WQTK Not found:id %d\n",id);
    return NULL;
}

//安全遍历
static void ht_traverse_safe(void) {
    unsigned int bkt;
    struct my_obj *data;
    struct hlist_node *node;
    pr_info("WQTK === Start safe traversal ===");
    //外层
    //
    for (bkt = 0; bkt < HTSIZE; bkt ++) {
        //内层安全遍历当前
        hlist_for_each_entry_safe(data, node, &my_htbl[bkt], node){
            pr_info("WQTK Bucket %u: id=%d, name=%s", bkt, data->id, data->name);
            
        }
    }
     pr_info("WQTK -------------------------");
}

static void ht_traverse_safe_delete_all(void) {
    unsigned int bkt;
    struct my_obj *data;
    struct hlist_node *node;
    pr_info("WQTK === Start safe delete ===");
    //外层
    //
    for (bkt = 0; bkt < HTSIZE; bkt ++) {
        //内层安全遍历当前
        hlist_for_each_entry_safe(data, node, &my_htbl[bkt], node){
            pr_info("WQTK delete %u: id=%d, name=%s", bkt, data->id, data->name);
            //也可以删除节点 
            hlist_del(&data->node);
            kfree(data);
        }
    }
    
     pr_info("WQTK -------------------------");
}
//删除数据
static void ht_delete_id(int id) {
    unsigned int bkt, hash;
    struct my_obj *data;
    struct hlist_node *node;
    hash = hash_32(id, HT_BITS); 
    //外层
    //
    for (bkt = 0; bkt < HTSIZE; bkt ++) {
        //内层安全遍历当前
        hlist_for_each_entry_safe(data, node, &my_htbl[bkt], node){
            if(data->id == id){
                pr_info("WQTK delete %u: id=%d, name=%s", bkt, data->id, data->name);
                //也可以删除节点 
                hlist_del(&data->node);
                kfree(data);
                return;
            }
        }
    }
    pr_warn("WQTK Delete Failed:%d",id);
}
static int __init hlist_init(void) {
    
    printk(KERN_INFO "WQTK ");
    printk(KERN_INFO "WQTK ");
    printk(KERN_INFO "WQTK ##########################################");
    printk(KERN_INFO "WQTK begin hlist_init demo");
    int i;
    for (i = 0; i < HTSIZE; i++ ) {
        INIT_HLIST_HEAD(&my_htbl[i]);
    }
    //插入

    ht_insert(1001, "李文强");
    ht_insert(1002, "沈志佳");
    ht_insert(1003, "马占山"); 
    ht_insert(1004, "haoran");
    ht_insert(1005, "xiaowei");
    ht_insert(1006, "shichang"); 

    ht_insert(2001, "Alice");
    ht_insert(2002, "Carel");
    ht_insert(3003, "Bob");
    //find
    ht_look_up(1003);

    //遍历
    ht_traverse_safe();
    //删除
    ht_delete_id(2001); 
 
    ht_traverse_safe();
 
    return 0;
}


static void __exit hlist_exit(void) {
    printk(KERN_INFO "WQTK Goodbyd  hash table demo");
    ht_traverse_safe_delete_all();
  
}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");


module_init(hlist_init);
module_exit(hlist_exit);


