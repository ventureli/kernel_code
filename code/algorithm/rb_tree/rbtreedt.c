#define pr_fmt(fmt) "WQTK: " fmt
// #include <stdio.h>
// #include <stdlib.h>
#include <linux/init.h>
#include <linux/list.h> 
#include <linux/module.h>
#include <linux/kernel.h> 
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/slab.h>

struct mytypes{
    int key;
    struct rb_node node;   
};

struct rb_root mytree = RB_ROOT;  //整课和红黑树的根节点

//设计一个函数，根据Key来查询节点的数据信息。
struct mytypes *my_searchs(struct rb_root *root, int new) {
    struct rb_node *node = root->rb_node;
    while (node) {
        struct mytypes *data = container_of(node, struct  mytypes, node); 
        if (data->key > new) {
            node = node->rb_left;
        } else if (data->key < new) {
            node  = node->rb_right;
        }else{
            return data;
        }
    }
    return NULL;
};

//设计一个函数，将一个元素插入到红黑树中
int my_inserts(struct rb_root *root, struct mytypes *data) {
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    //查找到符合条件，则可添加新节点到红黑树中的位置
    while (*new) {
            struct mytypes *this = container_of(*new, struct mytypes, node);
            parent = *new;
            if (this->key > data->key) {
                new  = &((*new)->rb_left);
            } else if (this->key < data->key) {
                new  = &((*new)->rb_right);
            }else{
                return -1;
            }
    }
    //添加新街道到树中
    rb_link_node(&(data->node), parent, new);
    //new 代表的是指针的指针，就是父节点的 left还是right指针的地址，
    //对新插入的节点进行着色，修正红黑树，让他达到平衡
    rb_insert_color(&data->node, root);
    return 0;
}



static int __init my_initFunc(void) {
    int i;
    struct mytypes *data;
    struct rb_node *node;
    //插入对应数据结构
    for (i=0; i < 20; i = i+2) {
        data = kmalloc(sizeof(struct mytypes), GFP_KERNEL);
        data->key  = i;
        my_inserts(&mytree, data);
    }
    //打印输出
    for (node = rb_first(&mytree); node; node=rb_next(node)) {
        printk("WQTK rb_tree key = %d\n", rb_entry(node, struct mytypes, node)->key);
    }
    
    return 0;
}

static void __exit my_exitFunc(void) {

    struct mytypes *data;
    struct rb_node *node, *next;

    for (node = rb_first(&mytree); node; node = next) {
        next  = rb_next(node);
        data = rb_entry(node, struct mytypes, node);
        if (data) {
            rb_erase(&data->node, &mytree);
            kfree(data);
        }
    }

}

MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("rbtree");
MODULE_LICENSE("GPL");

module_init(my_initFunc);
module_exit(my_exitFunc);




