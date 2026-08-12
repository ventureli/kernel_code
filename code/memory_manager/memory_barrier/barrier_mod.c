/*
 * barrier_mod.c — 演示 smp_mb() 内存屏障
 *
 * 两个 kthread 绑到不同 CPU，跑 Store Buffer 测试：
 *   CPU0: x=1; smp_mb(); r0=y;
 *   CPU1: y=1; smp_mb(); r1=x;
 *
 * 用法：
 *   sudo insmod barrier_mod.ko use_barrier=0 sec=5
 *   sudo insmod barrier_mod.ko use_barrier=1 sec=5
 *   # 等几秒自动打印结果，dmesg 查看
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");

static int use_barrier = 1;
module_param(use_barrier, int, 0644);

static int sec = 5;
module_param(sec, int, 0644);

static volatile int x, y, r0, r1;
static volatile int stop;
static unsigned long good, bad, total;
static struct task_struct *t0, *t1, *tc;

/* CPU0：写 x，读 y */
static int cpu0_fn(void *arg)
{
    while (!READ_ONCE(stop)) {
        WRITE_ONCE(x, 1);
        if (use_barrier) smp_mb();
        WRITE_ONCE(r0, READ_ONCE(y));
        msleep(1);  /* 让出 CPU */
    }
    return 0;
}

/* CPU1：写 y，读 x */
static int cpu1_fn(void *arg)
{
    while (!READ_ONCE(stop)) {
        WRITE_ONCE(y, 1);
        if (use_barrier) smp_mb();
        WRITE_ONCE(r1, READ_ONCE(x));
        msleep(1);
    }
    return 0;
}

/* 采集线程：看到两个都写完了就采样 */
static int collector_fn(void *arg)
{
    while (!READ_ONCE(stop)) {
        if (READ_ONCE(x) == 1 && READ_ONCE(y) == 1) {
            total++;
            if (READ_ONCE(r0) == 0 && READ_ONCE(r1) == 0)
                bad++;
            else
                good++;
            WRITE_ONCE(x, 0);
            WRITE_ONCE(y, 0);
        }
        msleep(1);
    }
    return 0;
}

static int __init barrier_mod_init(void)
{
    pr_info("=== 内存屏障测试 ===\n");
    pr_info("模式: %s,  时长: %d 秒\n",
            use_barrier ? "有 smp_mb()" : "无屏障", sec);

    if (num_online_cpus() < 2) {
        pr_err("需要至少 2 个 CPU 核\n");
        return -EINVAL;
    }

    t0 = kthread_create(cpu0_fn, NULL, "barrier-cpu0");
    t1 = kthread_create(cpu1_fn, NULL, "barrier-cpu1");
    tc = kthread_create(collector_fn, NULL, "barrier-collector");
    if (IS_ERR(t0) || IS_ERR(t1) || IS_ERR(tc)) return -ENOMEM;

    kthread_bind(t0, 0);
    kthread_bind(t1, 1);
    wake_up_process(t0);
    wake_up_process(t1);
    wake_up_process(tc);

    ssleep(sec);

    WRITE_ONCE(stop, 1);
    kthread_stop(t0);
    kthread_stop(t1);
    kthread_stop(tc);

    pr_info("===== 结果 =====\n");
    pr_info("总轮次: %lu\n", total);
    pr_info("正常:   %lu  (r0=1 或 r1=1)\n", good);
    pr_info("乱序:   %lu  (r0=0 且 r1=0)\n", bad);

    if (bad > 0)
        pr_info("\n[!] 检测到 %lu 次 CPU 乱序！\n", bad);
    else
        pr_info("\n[OK] 未检测到乱序——smp_mb() 生效\n");

    return 0;
}

static void __exit barrier_mod_exit(void) { }

module_init(barrier_mod_init);
module_exit(barrier_mod_exit);
