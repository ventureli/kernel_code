#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>

#define ARCH_PFH_OFFSET (0UL) //起始空间的物理地址对应的页帧号
#define PRT(a,b) pr_info("%-15s=%10d %10ld %8ld", a, b, PAGE_SIZE * b / 1024, (PAGE_SIZE *b)/1024/1024 );

static int __init my_init(void) {
    struct page *p;//内核物理页面；
    unsigned long i, pfn, valid = 0;
    int free = 0;
    int locked = 0;
    int reserved = 0;
    int swapcache = 0;
    int reference = 0;
    int slab  = 0 ;
    int private = 0;
    int uptodate = 0;
    int dirty = 0;
    int active = 0;
    int writeback = 0;
    int mappedtodisk = 0;
    
    unsigned long num_physpages;
    num_physpages = get_num_physpages();
    for(i = 0; i < num_physpages; i++) {
        pfn = ARCH_PFH_OFFSET + i;
        if (!pfn_valid(pfn)) continue;//看是否有效
        valid++;

        p = pfn_to_page(pfn); //拿到了page
        if (!p) continue;
        if (!page_count(p)) {
            free++;
            continue;
        }
        if (PageLocked(p)) {
            locked++;
             
        }
        if (PageReserved(p)) {
            reserved++;
             
        }
        if (PageSwapCache(p)) {
            swapcache++;
             
        }
        if (PageReferenced(p)) { //最近是否被使用过
            reference++;
        }
        if (PageSlab(p)) {
            slab++;
             
        }
        if(PagePrivate(p)){
            private++;
        }
        if (PageDirty(p)) {
            dirty++;
        }
        if(PageUptodate(p)){
            uptodate++;
        }

        if(PageActive(p)){
            active++;
        }

        if(PageWriteback(p)){
            writeback++;
        }

        if(PageMappedToDisk(p)){
            mappedtodisk++;
        }         
    }

    pr_info("\nExaming %ld pages(num_phys_pages)=%ld",num_physpages * PAGE_SIZE, num_physpages);
    pr_info("\nPages whith valid PFN %ld=%ld MB",valid, valid * PAGE_SIZE /1024/1024);
    pr_info("\%-15s %10s %10s %8s\n\n", "Name","Pages","KB","MB");
    pr_info("-----------------------------------------------");
    PRT("free", free);
    PRT("locked", locked);
    PRT("rseserved", reserved);
    PRT("reference", reference);
    PRT("slab", slab);
    PRT("private", private);
    PRT("uptodate", uptodate);
    PRT("dirty", dirty);
    PRT("active", active);
    PRT("writeback", writeback);
    PRT("mappedtodisk", mappedtodisk);
  
    return 0;
}

static void __exit my_exit(void) {
    pr_info("Module Exit OK.");

    // printk(KERN_INFO "Module Exit OK.");
}


MODULE_AUTHOR("FatBoyLi");
MODULE_DESCRIPTION("getphysinfo");
MODULE_LICENSE("GPL");

module_init(my_init);
module_exit(my_exit);
