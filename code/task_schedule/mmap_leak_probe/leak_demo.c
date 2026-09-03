/*
 * leak_demo.c —— 一个「静默泄漏」内存的进程（模拟真实 bug）
 *
 * 它看起来就是个普通的长驻进程，全程不打印任何「我泄漏了」之类的话
 * （真实程序不会自己报告泄漏），只能靠 mmap_leak_probe 去 /proc/mmap_leak 里抓。
 *
 * 每轮循环干两件事：
 *   ① mmap 2MB 匿名内存 —— 用完就 munmap，不泄漏（正常内存分配）；
 *   ② mmap 20MB 匿名内存 —— 每 LEAK_MOD 次漏掉 1 次 munmap（bug 在这里）。
 * 两路都 memset 真正 touch，mmap/munmap 之间随机 usleep 1~200 毫秒，
 * 既保留「先持有内存用一阵再释放」的真实感，又够快、方便观察增长。
 *
 * 运行：./leak_demo &
 *       然后另开终端 cat /proc/mmap_leak，能看到泄漏的 20MB 映射越堆越多；
 *       观察完 kill 掉即可（Ctrl-C 或 kill <pid>）。
 *
 * 编译：gcc -O2 -Wall -o leak_demo leak_demo.c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>

#define NORMAL_SIZE  (2UL << 20)    /* 2MB —— 每次都正常释放 */
#define LEAK_SIZE    (20UL << 20)    /* 20MB —— 偶尔漏掉 munmap（泄漏源） */
#define LEAK_MOD     3              /* 每 3 次，漏掉 1 次 munmap */

int main(void)
{
    unsigned long iter = 0;

    srand(time(NULL));
    printf("pid=%d\n", getpid());   /* 只报个 pid，方便 pmap / 观察定位 */

    for (;;) {
        /* ① 2MB：正常分配 → 随机持有一阵 → 释放 */
        void *normal = mmap(NULL, NORMAL_SIZE, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (normal == MAP_FAILED) { perror("mmap 2MB"); return 1; }
        memset(normal, 0xCD, NORMAL_SIZE);   /* touch */
        usleep(1 + rand() % 200000);         /* 持有 1~200ms */
        munmap(normal, NORMAL_SIZE);

        /* ② 20MB：正常分配 → 随机持有一阵 → 偶尔忘了释放 */
        void *leak = mmap(NULL, LEAK_SIZE, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (leak == MAP_FAILED) { perror("mmap 20MB"); return 1; }
        memset(leak, 0xAB, LEAK_SIZE);   /* touch，让 RSS 也涨 */
        usleep(1 + rand() % 200000);     /* 持有 1~200ms */

        if (iter % LEAK_MOD != 0)
            munmap(leak, LEAK_SIZE);
        /* else：这条路径忘了 munmap —— 泄漏 */

        iter++;
    }

    return 0;
}
