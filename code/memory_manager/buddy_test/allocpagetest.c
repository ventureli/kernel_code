#include <linux/module.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/string.h>


#define ALLOC_ORDER 4
struct page * alloced_page = NULL;

static int __init my_init(void) {
    alloced_page =  alloc_pages(GFP_KERNEL, ALLOC_ORDER); //分配16个物理页面
    if (NULL == alloced_page) {
        return -1;
    }

    pr_info("alloc pages Allocation succeed \n");
    pr_info("pages_address(allocpages = 0x%lx)\n", (unsigned long)page_address(alloced_page));

    return 0;
}

static void __exit my_exit(void){
    if(alloced_page)
    {
        __free_pages(alloced_page, ALLOC_ORDER);
        pr_info("_free_pages success");
    }
    pr_info("Module exit successfuly");
}

MODULE_AUTHOR("liwenqiang");
MODULE_DESCRIPTION("allocpagetest");
MODULE_LICENSE("GPL");


module_init(my_init);
module_exit(my_exit);



