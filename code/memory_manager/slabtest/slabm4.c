#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/console.h>

typedef struct kbuf_chain{
    char * kbuf;
    struct list_head list_node;
    int id;
}kbuf_chain_t;

static int size  = 40; //分配的对象大小

static LIST_HEAD(buf_chain_list);
static atomic_t buf_index = ATOMIC_INIT(0);

static struct kmem_cache *my_caches;

static char add_or_minus[10] = "waiting"; //动态函数
static char *add_or_minus_ptr = add_or_minus;


static int add_buf_node(void) {
   char * kbuf = kmem_cache_alloc(my_caches, GFP_ATOMIC);
    if(!kbuf) {
        pr_err("create Object failed destroy cache");
        return -1;
    }
    //success add to list
    pr_info("create object successly:%p", kbuf);  
    
    kbuf_chain_t *node = kmalloc(sizeof(kbuf_chain_t),GFP_ATOMIC);
    if (!node) {
        pr_err("create chain node  failed ");
        return -1;
    }
    node->kbuf = kbuf;
    node->id = atomic_read(&buf_index);
    
    list_add(&(node->list_node), &buf_chain_list);
    atomic_inc(&buf_index);
    return 0;
}

static int minus_buf_node(void) {
   if (list_empty(&buf_chain_list)) {
      pr_err("donot have obj remove failed");
      return -1;
   }

   kbuf_chain_t *last = list_last_entry(&buf_chain_list, kbuf_chain_t, list_node);
   //
   list_del(&(last->list_node)); //摘除钩子
   kmem_cache_free(my_caches, last->kbuf);
   pr_info("remove index %d success", last->id);  
   kfree(last);

   return 0;

}

static void show_buf_node(void) {
    if (list_empty(&buf_chain_list)) {
      pr_info("!!buf_chain_list is empty!!!");
      return;
   }
    kbuf_chain_t *chain_node;
    pr_info("------------------------------\n");
    int count = 0;
    list_for_each_entry(chain_node, &buf_chain_list, list_node) {
        pr_info("|%2d:(%-4d,%p)|\n",count++,chain_node->id, chain_node->kbuf);
    }
    pr_info("------------------------------\n");
}

static void free_all_nodes(void) {

    pr_info("---------remove all node-----\n");
    kbuf_chain_t *chain_node,*tmp;
    
    list_for_each_entry_safe(chain_node, tmp, &buf_chain_list, list_node) {
        pr_info("free (%-4d,%p)|\n",chain_node->id, chain_node->kbuf);
        list_del(&(chain_node->list_node));
        kmem_cache_free(my_caches, chain_node->kbuf);
        kfree(chain_node);
    }
    pr_info("------------------------------\n");
}

static int set_my_cmd(const char *val, const struct kernel_param *kp){

    char buf[32]; 
    strncpy(buf,val,sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    strim(buf);
    // strncpy(add_or_minus, val, sizeof(add_or_minus) - 1 );
    // add_or_minus[sizeof(add_or_minus) - 1 ] = '\0';//anqu
    if (strcmp(buf,"add") == 0) {
         pr_info("触发命令 add 不修改:%s\n",add_or_minus);
         add_buf_node();
    }else if (strcmp(buf,"minus") == 0) {
         pr_info("触发命令 minus 不修改:%s\n",add_or_minus);
         minus_buf_node();
    }else{
         pr_info("CMD触发**%s**go show ***\n",val);
         show_buf_node();
    }
    console_conditional_schedule();
    return 0;
}



static const struct kernel_param_ops add_or_minus_ops = {
    .set = set_my_cmd,
    .get = param_get_charp, //使用默认的
};

//注册回调用
module_param_cb(add_or_minus, &add_or_minus_ops, &add_or_minus_ptr, 0644);
// MODULE_PARAM_DESC(add_or_minus,"A string param with writer trigger");

//
// 
module_param(size, int, 0444);  //这是一个宏定义
 

static int __init myslab_init(void) {
    if (size > KMALLOC_MAX_SIZE) {
        pr_err("Size=%d is too big", size);
        return -1;
    }
    my_caches = kmem_cache_create("mycaches", size, 0, SLAB_HWCACHE_ALIGN, NULL );
    //SLAB_HWCACHE_ALIGN，按照硬件缓存行对齐
    //NULL 代表是的硬件构造函数
    if (!my_caches) {
        pr_err(" kmem-cache_create()failed");
        return -ENOMEM;
    }
    pr_info("Create mycache correct %p",my_caches);
    //分配一个对象
    
    return 0;

}

static void __exit myslab_exit(void) {
    // if (kbuf) {
    //     //释放一个对象
        // kmem_cache_free(my_caches,kbuf);
    free_all_nodes();
    kmem_cache_destroy(my_caches);  //销毁内存
    //销毁内存
    pr_info("destroy cache successly");
    // }
}

MODULE_AUTHOR("liwenqiang");
MODULE_DESCRIPTION("my_slab test");
MODULE_LICENSE("GPL");


module_init(myslab_init);
module_exit(myslab_exit);


/*
//我们能看到这个参数
xuan@xuan-u22:~/Desktop/share_dir/article/kernel/code/slabtest$ make install
sudo insmod slabm.ko
xuan@xuan-u22:~/Desktop/share_dir/article/kernel/code/slabtest$ cat /sys/module/slabm/parameters/size 
40
//使用的方法
882  sudo sh -c 'echo show > /sys/module/slabm4/parameters/add_or_minus'
883  sudo sh -c 'echo minus > /sys/module/slabm4/parameters/add_or_minus'
884  sudo sh -c 'echo add > /sys/module/slabm4/parameters/add_or_minus'
1004  sudo sh -c 'echo add > /sys/module/slabm4/parameters/add_or_minus'
1005  sudo sh -c 'echo show > /sys/module/slabm4/parameters/add_or_minus'
*/