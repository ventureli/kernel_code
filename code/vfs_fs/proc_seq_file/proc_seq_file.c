#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/slab.h> 
static LIST_HEAD(head);
struct seqdata {
    struct list_head list;
    int values_data;
};
static struct mutex lock;

struct my_data{
    int num;
    char name[10];
};



#define DATA_SIZE  5
struct my_data my_data_list[DATA_SIZE] = {
    {.num = 1,.name = "name1"},
    {.num = 10,.name = "name2"},
    {.num = 14,.name = "name3"},
    {.num = 10,.name = "name4"},
    {.num = 24,.name = "name5"},
}; //模拟数据目前已经有的
 
 //用于添加数据的
static void show_list(void){
    pr_info("输出所有链表的节点数值:\n");
    struct seqdata *seq_node;
    list_for_each_entry(seq_node, &head, list) { 
        pr_info("%5d", seq_node->values_data); 
    }
    return;
}
//用于添加数据的
static void add_to_list(int pnum){
    struct seqdata *sdata;
    pr_info("insert to list (%d)\n",(int)pnum);
    mutex_lock(&lock);
    sdata = kmalloc(sizeof(*sdata),GFP_KERNEL);
    if (sdata != NULL){
        sdata->values_data = pnum;
        list_add(&(sdata->list), &head);
    }
    mutex_unlock(&lock);
    return;
}

static void clear_list(void) {
    struct seqdata *seq_node,*tmp;
    pr_info("clear_list");
    mutex_lock(&lock);
    list_for_each_entry_safe(seq_node, tmp, &head, list) {
        pr_info("free (%-4d)|\n",seq_node->values_data);
        list_del(&(seq_node->list));
        kfree(seq_node);
    }
    mutex_unlock(&lock);
}

//start 函数，返回第一个数据
static void *my_seq_start(struct seq_file *s, loff_t *pos) {
    if (*pos > 0) { 
        pr_info("call my_seq_start set pos > 0 (%d)\n",(int)*pos);
        //删除所有数据
        clear_list();
        return NULL;
    }
    *pos = 0;
    //add first data
    add_to_list(my_data_list[0].num);
    pr_info("call my_seq_start set pos=0 and return &my_data_list[0] \n");
    return &(my_data_list[0]);
    // return NULL;//pos 表示结束
}

//next函数
static void *my_seq_next(
    struct seq_file *s,
    void *v,  //这个是上一个数据
    loff_t *pos  //这个是新增的
) {
    
    if(v) {
        pr_info("call my_sq_next param(pos=%d v.name =%s )\n", (int)*pos, ((struct my_data *)v)->name);
    }else{
        pr_info("call my_sq_next param(pos=%d) v is NULL\n", (int)*pos);
    }
    (*pos)++;
    if(*pos >= DATA_SIZE) {
        pr_info("call my_sq_next return NULL");
        return NULL;
    }
    pr_info("call my_sq_next return pos++ and &my_data_list[*pos]");
    add_to_list(my_data_list[*pos].num);
    return &my_data_list[*pos];
}

static void my_seq_stop(struct seq_file *s, void *v) {
    show_list();
    if(v) {
        pr_info("call my_seq_stop param(v.name =%s )\n", ((struct my_data *)v)->name);
    }else{
        pr_info("call my_seq_stop param v is NULL\n");
    }
}

static int my_seq_show(struct seq_file *s, void *v) {
    if (v) {
        seq_printf(s, "(v.name =%s v.num = %d )\n" ,((struct my_data *)v)->name, ((struct my_data *)v)->num);
        pr_info("call my_seq_show param(v.name =%s v.num = %d )\n", ((struct my_data *)v)->name, ((struct my_data *)v)->num);
    }
    return 0;
}

//配置操作方法
const struct seq_operations my_seqfile_ops = {
    .start = my_seq_start,
    .next = my_seq_next,
    .show = my_seq_show,
    .stop = my_seq_stop,
};

//打开的方法
static int my_seq_open(struct inode *inode, struct file *file) {
    return seq_open(file, &my_seqfile_ops);
}

//通用fileoperations
static const struct proc_ops proc_fops = {  
    .proc_open  = my_seq_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};

//模块初始化函数

static int __init my_init(void) {
    struct  proc_dir_entry *entry;
    //创建
    entry = proc_create("my_seq_file", 0444, NULL, &proc_fops);
    if(!entry) {
        pr_err("Failed to create /proc/my_seq_file\n");
        return -ENOMEM;
    }
    pr_info("Module loaded Use 'cat /proc/my_seq_file' to see");
    return 0;    
}

static void __exit my_exit(void) {
    
    clear_list();
    remove_proc_entry("my_seq_file", NULL);
    pr_info("Module unloaded ！！！！");
}
module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A seq_file demo with my_next");