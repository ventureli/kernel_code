/*
 * kprobe demo —— 钩住 do_sys_openat2，打印「谁正在打开哪个文件」
 *
 * 编译: make
 * 加载: sudo insmod kprobe_demo.ko
 * 卸载: sudo rmmod kprobe_demo
 * 观察: sudo dmesg | tail
 *
 * 说明: 现代 glibc 的 open() 实际走 openat 系统调用 -> do_sys_openat2，
 *       所以 do_sys_open 虽是真符号却几乎不会被调到（钩了没输出）。
 *       do_sys_openat2 才是必经之路。
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/uaccess.h>

/* 只打印这个进程的 open，避免刷屏（do_sys_openat2 是内核最烫的路径） */
static char *target = "cat";
module_param(target, charp, 0644);
MODULE_PARM_DESC(target, "only log open from this process (default: cat)");

static struct kprobe kp = {
    .symbol_name = "do_sys_openat2",
};

static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
    const char __user *filename;
    char *comm = current->comm;              /* 当前进程名 */
    char buf[256];

    /* 只关心目标进程，否则每个进程每次 open 都打印会刷屏卡死 */
    if (strcmp(comm, target))
        return 0;

    /* do_sys_openat2 的第 2 个参数是文件路径（第 1 个是 dfd）。
     * 它是 __user 指针，必须用 strncpy_from_user 拷进内核缓冲，
     * 直接 %s 解引用在 ARM64（开 PAN）上会 Oops。 */
    filename = (const char __user *)regs_get_kernel_argument(regs, 1);
    if (strncpy_from_user(buf, filename, sizeof(buf)) < 0)
        return 0;

    pr_info("Process %s is trying to open file: %s\n", comm, buf);
    return 0;
}

static int __init kprobe_init(void)
{
    int ret;

    kp.pre_handler = handler_pre;
    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("register_kprobe failed, ret=%d\n", ret);
        return ret;
    }
    pr_info("kprobe module loaded, monitoring do_sys_openat2\n");
    return 0;
}

static void __exit kprobe_exit(void)
{
    unregister_kprobe(&kp);
    pr_info("kprobe module unloaded\n");
}

module_init(kprobe_init);
module_exit(kprobe_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ventrueli");
MODULE_DESCRIPTION("Hook do_sys_openat2 with kprobe");
