/*
 * vdso_bench.c — 对比 vDSO 和纯 syscall 的性能差异
 *
 * gettimeofday() 走 vDSO：内核把时间数据映射到用户空间，直接读内存
 * syscall(SYS_gettimeofday) 走真正的系统调用：用户态→内核→返回
 *
 * 编译：gcc -O2 -o vdso_bench vdso_bench.c
 * 运行：./vdso_bench
 */
#include <stdio.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <time.h>

#define LOOPS 10000000  /* 一千万次 */

int main(void)
{
    struct timeval tv;
    struct timespec t1, t2;
    long long delta_ns;
    int i;

    /* ── 走 vDSO ── */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (i = 0; i < LOOPS; i++)
        gettimeofday(&tv, NULL);          /* glibc 自动走 vDSO */
    clock_gettime(CLOCK_MONOTONIC, &t2);

    delta_ns = (t2.tv_sec - t1.tv_sec) * 1000000000LL
             + (t2.tv_nsec - t1.tv_nsec);
    printf("vDSO gettimeofday : %d 次, 总耗时 %lld ns, 平均 %lld ns/次\n",
           LOOPS, delta_ns, delta_ns / LOOPS);

    /* ── 走纯 syscall ── */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (i = 0; i < LOOPS; i++)
        syscall(SYS_gettimeofday, &tv, NULL);  /* 强制走系统调用 */
    clock_gettime(CLOCK_MONOTONIC, &t2);

    delta_ns = (t2.tv_sec - t1.tv_sec) * 1000000000LL
             + (t2.tv_nsec - t1.tv_nsec);
    printf("syscall gettimeofday: %d 次, 总耗时 %lld ns, 平均 %lld ns/次\n",
           LOOPS, delta_ns, delta_ns / LOOPS);

    return 0;
}
