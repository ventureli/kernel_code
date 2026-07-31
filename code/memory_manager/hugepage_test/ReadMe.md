# 巨页 vs 普通页访问延迟对比

对应文章：【内核札记-28】TLB 与巨型页：CPU 里的地址翻译快表 (2026-07-27)

## 巨页 vs 普通页 访问延迟对比

测试内存: 128 MB, 每页首字节随机访问 32768 次

```sh
# 编译
gcc -o hugepage_bench hugepage_bench.c

# 普通页 (4KB) 测试
./hugepage_bench 4k

# 巨页 (2MB) 测试
# 先配置巨页
echo 128 > /proc/sys/vm/nr_hugepages
./hugepage_bench 2m
```

## 知识点

- TLB (Translation Lookaside Buffer) — 地址翻译快表
- 4KB 普通页 vs 2MB/1GB 巨页的 TLB 覆盖率差异
- TLB miss 的性能代价
- 巨页适用于大内存工作负载（数据库、AI 推理）
