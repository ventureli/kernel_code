/*
 * mmap_leak_probe.c —— 用 kprobe + kretprobe 抓「只 mmap 不 munmap」的内存泄漏
 *
 * 场景：
 *   用户程序 leak_demo 循环 mmap(MAP_ANONYMOUS)，却故意漏掉部分 munmap，
 *   导致匿名 VMA 不断增多（内存只涨不降，pmap 里 [anon] 段越堆越多）。
 *
 * 本模块在系统调用层取证，思路是「配对 + 求差」：
 *   - kretprobe 挂 __arm64_sys_mmap：
 *       函数返回时拿到「真实映射地址」——注意 mmap 的第一个参数只是建议地址，
 *       真正的地址是返回值，所以必须用 kretprobe 才能拿到；
 *       连同长度/flags 记进一张哈希表；
 *   - kprobe 挂 __arm64_sys_munmap：
 *       按地址把哈希表里对应条目删掉；
 *   - /proc/mmap_leak：
 *       把哈希表里「剩下的条目」dump 出来 —— 剩下的就是 mmap 了却没 munmap 的泄漏映射。
 *
 * 编译: make
 * 加载: sudo insmod mmap_leak_probe.ko target=leak_demo
 * 观察: cat /proc/mmap_leak
 * 卸载: sudo rmmod mmap_leak_probe
 *
 * ─────────────────────────────────────────────────────
 * 关键点（ARM64 专属坑，也是本篇的核心知识点）：
 *   __arm64_sys_mmap / __arm64_sys_munmap 这种「系统调用 wrapper」，
 *   函数签名不是 (addr, len, prot, ...)，而是 (const struct pt_regs *regs)！
 *   真正的系统调用参数，藏在 regs 指向的那份「保存的用户态寄存器」里，
 *   也就是 regs->regs[0..5]。所以要先取 regs 指针，再解引用一次才能拿到参数。
 *   这和昨天的 do_sys_openat2(dfd, filename, ...)（普通参数、直接取）完全不同。
 * ─────────────────────────────────────────────────────
 */

#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/hashtable.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/err.h>

/* 只跟踪这个进程的 mmap/munmap，避免被全系统海量 mmap 刷爆 */
static char *target = "leak_demo";
module_param(target, charp, 0644);
MODULE_PARM_DESC(target, "only track mmap/munmap from this process");

#define MAP_ANON_BIT 0x20   /* MAP_ANONYMOUS（asm-generic 上等于 MAP_ANON） */
#define TABLE_BITS   8      /* 哈希表 bucket 数 = 2^8 = 256 */

/* ──────────────────────────────────────────────────
 * 数据结构：每条「活着的映射」占一个节点
 * ────────────────────────────────────────────────── */
struct map_entry {
    unsigned long addr;     /* 映射地址（mmap 的返回值） */
    unsigned long len;      /* 长度 */
    unsigned long flags;    /* mmap 的 flags（判断匿名/文件映射） */
    char comm[TASK_COMM_LEN];
    struct hlist_node node;
};

static DEFINE_HASHTABLE(map_table, TABLE_BITS);
static DEFINE_SPINLOCK(map_lock);

/* kretprobe 的 entry_handler 需要把「长度/flags」暂存起来，
 * 等 return_handler 拿到返回值（真实地址）后再一起入表 */
struct mmap_ctx {
    unsigned long len;
    unsigned long flags;
};

/* ──────────────────────────────────────────────────
 * 取系统调用参数：解引用 wrapper 的 regs 指针
 *
 * __arm64_sys_mmap(const struct pt_regs *regs) 的唯一参数，
 * 就是一份「保存了用户态寄存器」的 pt_regs。
 * regs->regs[0..5] 才是真正的 6 个系统调用参数。
 * ────────────────────────────────────────────────── */
static void syscall_args(struct pt_regs *regs, unsigned long out[6])
{
    struct pt_regs *args = (struct pt_regs *)regs->regs[0];

    out[0] = args->regs[0];  /* addr  */
    out[1] = args->regs[1];  /* len   */
    out[2] = args->regs[2];  /* prot  */
    out[3] = args->regs[3];  /* flags */
    out[4] = args->regs[4];  /* fd    */
    out[5] = args->regs[5];  /* off   */
}

/* ──────────────────────────────────────────────────
 * mmap：entry_handler 存 len/flags
 * ────────────────────────────────────────────────── */
static int entry_mmap(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct mmap_ctx *ctx = (struct mmap_ctx *)ri->data;
    unsigned long a[6];

    /* 非目标进程直接跳过（返回非 0，不安装返回钩子） */
    if (strcmp(current->comm, target))
        return 1;

    syscall_args(regs, a);
    ctx->len   = a[1];
    ctx->flags = a[3];
    return 0;
}

/* ──────────────────────────────────────────────────
 * mmap：return_handler 拿返回值（真实地址），入表
 * ────────────────────────────────────────────────── */
static int ret_mmap(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct mmap_ctx *ctx = (struct mmap_ctx *)ri->data;
    struct map_entry *entry;
    unsigned long addr = regs->regs[0];   /* x0 = 返回值 = 真实映射地址 */

    if (IS_ERR_VALUE(addr))   /* MAP_FAILED，跳过 */
        return 0;

    entry = kmalloc(sizeof(*entry), GFP_ATOMIC);
    if (!entry)
        return 0;

    entry->addr  = addr;
    entry->len   = ctx->len;
    entry->flags = ctx->flags;
    strncpy(entry->comm, current->comm, TASK_COMM_LEN);

    spin_lock(&map_lock);
    hash_add(map_table, &entry->node, addr);   /* hash_add 内部会做 hash_min */
    spin_unlock(&map_lock);

    return 0;
}

/* ──────────────────────────────────────────────────
 * munmap：按地址把条目删掉
 * ────────────────────────────────────────────────── */
static int handler_munmap(struct kprobe *p, struct pt_regs *regs)
{
    struct map_entry *entry;
    unsigned long a[6];
    unsigned long addr;

    if (strcmp(current->comm, target))
        return 0;

    syscall_args(regs, a);
    addr = a[0];   /* munmap 的第 1 个参数就是映射地址 */

    spin_lock(&map_lock);
    hash_for_each_possible(map_table, entry, node, addr) {
        if (entry->addr == addr) {
            hash_del(&entry->node);
            kfree(entry);
            break;
        }
    }
    spin_unlock(&map_lock);

    return 0;
}

/* ──────────────────────────────────────────────────
 * /proc/mmap_leak：dump 剩下的条目 = 泄漏的映射
 * ────────────────────────────────────────────────── */
static ssize_t proc_read(struct file *file, char __user *ubuf,
                         size_t size, loff_t *off)
{
    struct map_entry *entry;
    char *kbuf;
    size_t pos = 0;
    unsigned int bkt;
    int count = 0;
    int cap = 4096;

    if (*off > 0)   /* 一次读完，不处理多次读 */
        return 0;

    kbuf = kzalloc(cap, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    pos += scnprintf(kbuf + pos, cap - pos, "=== mmap 了但没 munmap 的映射（泄漏）===\n");

    spin_lock(&map_lock);
    hash_for_each(map_table, bkt, entry, node) {
        pos += scnprintf(kbuf + pos, cap - pos,
                         "[%s] addr=0x%lx  len=0x%lx (%lu KB)  flags=0x%lx  %s\n",
                         entry->comm, entry->addr, entry->len,
                         entry->len >> 10, entry->flags,
                         (entry->flags & MAP_ANON_BIT) ? "(匿名映射)" : "(文件映射)");
        count++;
    }
    spin_unlock(&map_lock);

    pos += scnprintf(kbuf + pos, cap - pos, "泄漏映射总数: %d\n", count);

    if (copy_to_user(ubuf, kbuf, pos)) {
        kfree(kbuf);
        return -EFAULT;
    }

    kfree(kbuf);
    *off = pos;
    return pos;
}

static const struct proc_ops mmap_leak_proc_ops = {
    .proc_read = proc_read,
};

/* ──────────────────────────────────────────────────
 * 探针结构体
 * ────────────────────────────────────────────────── */
static struct kretprobe krp_mmap = {
    .kp.symbol_name = "__arm64_sys_mmap",
    .entry_handler  = entry_mmap,
    .handler        = ret_mmap,
    .data_size      = sizeof(struct mmap_ctx),
    .maxactive      = 20,
};

static struct kprobe kp_munmap = {
    .symbol_name = "__arm64_sys_munmap",
    .pre_handler = handler_munmap,
};

/* ──────────────────────────────────────────────────
 * 模块初始化 / 退出
 * ────────────────────────────────────────────────── */
static int __init mmap_leak_init(void)
{
    int ret;

    ret = register_kretprobe(&krp_mmap);
    if (ret < 0) {
        pr_err("register_kretprobe(__arm64_sys_mmap) failed: %d\n", ret);
        return ret;
    }

    ret = register_kprobe(&kp_munmap);
    if (ret < 0) {
        pr_err("register_kprobe(__arm64_sys_munmap) failed: %d\n", ret);
        unregister_kretprobe(&krp_mmap);
        return ret;
    }

    if (!proc_create("mmap_leak", 0444, NULL, &mmap_leak_proc_ops)) {
        unregister_kretprobe(&krp_mmap);
        unregister_kprobe(&kp_munmap);
        pr_err("proc_create(mmap_leak) failed\n");
        return -ENOMEM;
    }

    pr_info("mmap_leak_probe loaded, target=%s\n", target);
    return 0;
}

static void __exit mmap_leak_exit(void)
{
    struct map_entry *entry;
    struct hlist_node *tmp;
    unsigned int bkt;

    /* 清理哈希表 */
    hash_for_each_safe(map_table, bkt, tmp, entry, node) {
        hash_del(&entry->node);
        kfree(entry);
    }

    remove_proc_entry("mmap_leak", NULL);
    unregister_kretprobe(&krp_mmap);
    unregister_kprobe(&kp_munmap);
    pr_info("mmap_leak_probe unloaded\n");
}

module_init(mmap_leak_init);
module_exit(mmap_leak_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ventrueli");
MODULE_DESCRIPTION("Detect mmap/munmap mismatch (memory leak) with kprobe + kretprobe");
