/*
 * hugepage_bench.c — 对比 4KB 普通页 vs 2MB 巨页的访问性能
 *
 * 原理：遍历 128MB 内存，随机访问每个页的第一个字节。
 * 4KB 页 → 32768 个 TLB 条目    (128MB / 4KB)
 * 2MB 页 → 64 个 TLB 条目       (128MB / 2MB)
 * 2MB 页的 TLB miss 率远低于 4KB，访问延迟更低。
 *
 * 用法:
 *   # 先配置巨页:
 *   echo 128 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
 *   # 编译运行:
 *   gcc -O2 hugepage_bench.c -o hugepage_bench
 *   ./hugepage_bench
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#define TEST_SIZE (128UL * 1024 * 1024)   // 128MB 测试区
#define PAGE_4K   4096
#define PAGE_2M   (2UL * 1024 * 1024)
#define STRIDE    (PAGE_4K)               // 每页只访问首字节，最大化 TLB 压力
#define ACCESSES  (TEST_SIZE / STRIDE)     // 总访问次数

// 计时工具：返回微秒
static unsigned long now_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000UL + tv.tv_usec;
}

// 随机打乱访问顺序，模拟真实工作负载（避免预取干扰）
static void shuffle(int *arr, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

static double bench_access(char *base, int *indices, int n) {
    volatile char sink;           // volatile 防止编译器优化掉读操作
    unsigned long start, end;

    start = now_us();
    for (int i = 0; i < n; i++) {
        sink = base[indices[i]];  // 每次读一页的第一个字节
    }
    end = now_us();
    (void)sink;
    return (double)(end - start) / n;  // 平均每次访问的微秒数
}

int main(void) {
    char *addr_4k, *addr_2m;
    int *indices;
    int n = ACCESSES;

    /* 预生成随机访问序列（两组共用，公平对比） */
    indices = malloc(n * sizeof(int));
    if (!indices) { perror("malloc indices"); return 1; }
    for (int i = 0; i < n; i++)
        indices[i] = i * STRIDE;
    shuffle(indices, n);

    printf("=== 巨页 vs 普通页 访问延迟对比 ===\n");
    printf("测试内存: %lu MB, 每页首字节随机访问 %d 次\n\n",
           TEST_SIZE / (1024*1024), n);

    /* ======= 普通 4KB 页 ======= */
    addr_4k = mmap(NULL, TEST_SIZE,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS,
                   -1, 0);
    if (addr_4k == MAP_FAILED) {
        perror("mmap 4K"); return 1;
    }

    // 触发实际物理页分配（写每个页的第一个字节）
    for (int i = 0; i < n; i++)
        addr_4k[indices[i]] = (char)(i % 256);

    printf("[4KB 页] 已分配 %d 个 4KB 页\n", n);
    double lat_4k = bench_access(addr_4k, indices, n);
    printf("[4KB 页] 平均每次访问延迟: %.1f ns\n\n", lat_4k * 1000);

    munmap(addr_4k, TEST_SIZE);

    /* ======= 2MB 巨页 ======= */
#ifndef MAP_HUGETLB
    printf("[2MB 巨页] 当前系统不支持 MAP_HUGETLB (非 Linux 或需升级内核)\n");
    free(indices);
    return 0;
#else
    addr_2m = mmap(NULL, TEST_SIZE,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB,
                   -1, 0);
    if (addr_2m == MAP_FAILED) {
        fprintf(stderr,
                "mmap HUGETLB 失败 (errno=%d)\n"
                "  请先执行:\n"
                "  echo 128 | sudo tee /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages\n",
                errno);
        free(indices);
        return 0;  // 非致命，给用户提示
    }

    for (int i = 0; i < n; i++)
        addr_2m[indices[i]] = (char)(i % 256);

    int huge_pages = TEST_SIZE / PAGE_2M;
    printf("[2MB 巨页] 已分配 %d 个 2MB 巨页 (TLB 条目: %d → %d)\n",
           huge_pages, n, huge_pages);
    double lat_2m = bench_access(addr_2m, indices, n);
    printf("[2MB 巨页] 平均每次访问延迟: %.1f ns\n\n", lat_2m * 1000);

    munmap(addr_2m, TEST_SIZE);
#endif
    free(indices);

    printf("=== 结论 ===\n");
#ifdef MAP_HUGETLB
    if (addr_2m != MAP_FAILED) {
        printf("巨页比 4KB 页快 %.1f%% (%.1f ns → %.1f ns)\n",
               (lat_4k - lat_2m) / lat_4k * 100,
               lat_4k * 1000, lat_2m * 1000);
        printf("原因: 2MB 页的 TLB 条目数是 4KB 页的 1/%d，miss 率大幅降低\n",
               (int)(PAGE_2M / PAGE_4K));
    }
#else
    printf("请在 Linux 上运行本程序以对比巨页性能\n");
#endif
    return 0;
}
