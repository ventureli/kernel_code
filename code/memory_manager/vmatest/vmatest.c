#include <linux/module.h>
#include <linux/init.h>
#include <linux/path.h>
#include <linux/dcache.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>

static int pid;


// MODULE_PARAM_DESC(add_or_minus,"A string param with writer trigger");

// module_param(pid, int, S_IRUGO ); //S_IRUGO = 0444s

static const char * get_vma_elf_type(const struct vm_area_struct *vma) {
    if (!vma->vm_file) { //文件映射：获取路径
         return "anon";
    } else {
        if ((vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)) ==  (VM_READ | VM_EXEC)) {
            return ".text/code";
        }  
        if ((vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)) ==  (VM_READ)) {
            return ".rodatta/ro";
        }  
        if ((vma->vm_flags & (VM_READ | VM_EXEC | VM_WRITE)) ==  (VM_READ | VM_WRITE)) {
            return ".data/rw/.bss";
        }  
        if (vma->vm_flags & VM_GROWSDOWN) {
            return "stack";
        } 
        if (vma->vm_start == vma->vm_mm->brk) {
            return "heap";
        } 
        return "unknown";  
        
    }
}

static void get_vma_name(const struct vm_area_struct *vma, char *buf, size_t buf_len) {
    buf[0] = '\0';
    if (vma->vm_file) { //文件映射：获取路径
        struct path *path = &(vma->vm_file->f_path);
        char *ret = d_path(path, buf, buf_len - 1);
        // pr_info(" is Ffile::%s \n",ret);
        if (IS_ERR(ret)) {    
            snprintf(buf,  buf_len, "[file:unknown]");
        }else{
            strncpy(buf, ret, buf_len -1);
            buf[buf_len - 1] = '\0';
        }
    } else {
        if (vma->vm_flags & VM_STACK) {
            snprintf(buf,buf_len,"[stack]");
        } else if (vma->vm_flags & VM_MAYSHARE) {
            snprintf(buf,buf_len,"[shared]");
        } else  {
            if (vma->vm_start == vma->vm_mm->brk) {
                snprintf(buf,buf_len,"[head]");
            } else {
                snprintf(buf,buf_len,"[anon]");
            }
        }
    }
}

static void PrinfFun(struct task_struct *task) {
    struct mm_struct *mm;
    struct vm_area_struct *vma;
    int i;
    unsigned long start, end, length;
    struct vma_iterator vmi;
    char vma_name[512];
    // struct ma_state mas;//Maple Tree结构体
    mm =  get_task_mm(task);//->mm;
    pr_info(" mm=%p",mm);
    // vma = mm->mmap;
    // down_read(&mm->mmap_lock); //p操作一个信号量,只给写锁，而不给读枷锁
    mmap_read_lock(mm);
    pr_info("vmmas:vma start end\t\t lenght\n");
    // mas_init(&mas, &mm->mm_mt, 0);
    // mas_lock(&mas);
    vma_iter_init(&vmi, mm, 0);
    for_each_vma(vmi, vma){
        i++;
        start  =  vma->vm_start;
        end = vma->vm_end;
        length = end - start;
        get_vma_name(vma, vma_name, 512);
        pr_info("%d:%p---_%lx---%lx--%10ld KB %15s \t%s", i,vma, start, end,(end-start)/1024, get_vma_elf_type(vma), vma_name);
        // vma  = svma->vm_next;
    }
    // up_read(&mm->mmap_lock);
    // mas_unlock(&mas);
    mmap_read_unlock(mm);
    mmput(mm);

}


static int set_my_pid(const char *val, const struct kernel_param *kp){

    int old_val = pid;
    int ret;

    ret = param_set_int(val, kp);
    if (ret ==0) {
        pr_info("Param pid changed !\n");
        pr_info("Old=%d\n",old_val);
        pr_info("New=%d\n",pid);
        struct task_struct *task = pid_task(find_vpid(pid), PIDTYPE_PID);
    
        if (!task) {
            pr_err("Pid is Error");
            return -1;
        }
        pr_info("Exam vma's for pid = %d, command=%s\n", pid, task->comm);
        PrinfFun(task);
    }
    return ret;
}



static const struct kernel_param_ops pid_ops = {
    .set = set_my_pid,
    .get = param_get_int, //使用默认的
};


//注册回调用
module_param_cb(pid, &pid_ops, &pid, 0664);


static int __init my_init(void) {
        struct task_struct *task;
        if (pid ==0) {
            task  = current;
            pid = current->pid;
        } else {
            task = pid_task(find_vpid(pid), PIDTYPE_PID);
        }

        if (!task) {
            return -1;
        }
        pr_info("Exam vma's for pid = %d, command=%s\n", pid, task->comm);
        PrinfFun(task);
        return 0;
}

static void __exit my_exit(void) {
    pr_info("\n Module Uninstalled successfully.\n");
}


MODULE_AUTHOR("liwenqiang");
MODULE_DESCRIPTION("vmatest test");
MODULE_LICENSE("GPL");


module_init(my_init);
module_exit(my_exit);

/*


[365117.102308] Param pid changed !
[365117.102327] Old=87597
[365117.102342] New=87638
[365117.102356] Exam vma's for pid = 87638, command=bash
[365117.102372]  mm=00000000a8cd8cca
[365117.102431] vmmas:vma start end              lenght
[365117.102449] 1:00000000e5237cc0---_561fca455000---561fca484000--       188 KB     .rodatta/ro        /usr/bin/bash
[365117.102466] 2:00000000d15c2450---_561fca484000---561fca563000--       892 KB      .text/code        /usr/bin/bash
[365117.102532] 3:000000009c0e6a13---_561fca563000---561fca59d000--       232 KB     .rodatta/ro        /usr/bin/bash
[365117.102549] 4:00000000460b7969---_561fca59e000---561fca5a2000--        16 KB     .rodatta/ro        /usr/bin/bash
[365117.102565] 5:000000007e623f56---_561fca5a2000---561fca5ab000--        36 KB   .data/rw/.bss        /usr/bin/bash
[365117.102581] 6:00000000239d2e2c---_561fca5ab000---561fca5b6000--        44 KB            anon        [anon]
[365117.102597] 7:000000001e1db304---_561fca7d6000---561fca963000--      1588 KB            anon        [anon]
[365117.102613] 8:00000000042cbfcd---_7cb20c800000---7cb20d079000--      8676 KB     .rodatta/ro        /usr/lib/locale/locale-archive
[365117.102630] 9:00000000a07e4e68---_7cb20d200000---7cb20d228000--       160 KB     .rodatta/ro        /usr/lib/x86_64-linux-gnu/libc.so.6
[365117.102647] 10:00000000fb89ce1e---_7cb20d228000---7cb20d3bd000--      1620 KB      .text/code       /usr/lib/x86_64-linux-gnu/libc.so.6
[365117.102664] 11:00000000a4dd17a2---_7cb20d3bd000---7cb20d415000--       352 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/libc.so.6
[365117.102681] 12:0000000082657119---_7cb20d415000---7cb20d416000--         4 KB         unknown       /usr/lib/x86_64-linux-gnu/libc.so.6
[365117.102697] 13:0000000051c90004---_7cb20d416000---7cb20d41a000--        16 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/libc.so.6
[365117.102714] 14:00000000e0675af2---_7cb20d41a000---7cb20d41c000--         8 KB   .data/rw/.bss       /usr/lib/x86_64-linux-gnu/libc.so.6
[365117.102729] 15:00000000cac3ff72---_7cb20d41c000---7cb20d429000--        52 KB            anon       [anon]
[365117.102744] 16:00000000fde9e52e---_7cb20d53b000---7cb20d53e000--        12 KB            anon       [anon]
[365117.102761] 17:000000004c325d72---_7cb20d53e000---7cb20d54c000--        56 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/libtinfo.so.6.3
[365117.102778] 18:00000000434e3ed9---_7cb20d54c000---7cb20d55d000--        68 KB      .text/code       /usr/lib/x86_64-linux-gnu/libtinfo.so.6.3
[365117.102794] 19:0000000039941300---_7cb20d55d000---7cb20d56b000--        56 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/libtinfo.so.6.3
[365117.102810] 20:00000000e96ee303---_7cb20d56b000---7cb20d56f000--        16 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/libtinfo.so.6.3
[365117.102826] 21:00000000b8e67869---_7cb20d56f000---7cb20d570000--         4 KB   .data/rw/.bss       /usr/lib/x86_64-linux-gnu/libtinfo.so.6.3
[365117.102842] 22:00000000e34f8b90---_7cb20d577000---7cb20d57e000--        28 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/gconv/gconv-modules.cache
[365117.102857] 23:00000000a6c827e4---_7cb20d57e000---7cb20d580000--         8 KB            anon       [anon]
[365117.102873] 24:000000008c3f8498---_7cb20d580000---7cb20d582000--         8 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
[365117.102889] 25:000000009d433fd9---_7cb20d582000---7cb20d5ac000--       168 KB      .text/code       /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
[365117.102905] 26:000000005bbd1856---_7cb20d5ac000---7cb20d5b7000--        44 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
[365117.102920] 27:00000000f0086756---_7cb20d5b8000---7cb20d5ba000--         8 KB     .rodatta/ro       /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
[365117.102936] 28:00000000428d6ea8---_7cb20d5ba000---7cb20d5bc000--         8 KB   .data/rw/.bss       /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
[365117.102951] 29:000000002ed109e8---_7ffeea650000---7ffeea671000--       132 KB            anon       [stack]
[365117.102965] 30:0000000070ba7c70---_7ffeea73b000---7ffeea73f000--        16 KB            anon       [anon]

*/