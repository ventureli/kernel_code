#define pr_fmt(fmt) "WQTK: " fmt
#include <linux/init.h>
#include <linux/list.h> 
#include <linux/module.h>
#include <linux/kernel.h> 



static int __init hello_init(void)
{
    printk(KERN_INFO "WQTK Hello, kernel world\n");
    pr_info("Hello, kernel world\n");
    return 0;
}

static void __exit hello_exit(void)
{
    printk(KERN_INFO "WQTK Goodbyd  kernel world");
    pr_info("Goodbyd  kernel world\n");

}

MODULE_AUTHOR("Ventrueli");
MODULE_DESCRIPTION("helloworld");
MODULE_LICENSE("GPL");

module_init(hello_init);
module_exit(hello_exit);
