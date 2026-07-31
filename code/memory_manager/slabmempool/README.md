# 内存池（mempool + Slab）示例

对应文章：【内核札记-27】内存池：给内核留一条"最后的活路" (2026-07-26)

演示 mempool 的最常见用法——底层 Slab 作为 alloc/free，上层 mempool 包一层保险：
- `mempool_create_kmalloc_pool()` 或结合 Slab 创建 mempool
- `mempool_alloc()` 从池中分配（优先走 Slab，失败时从预留池取）
- `mempool_free()` 归还对象
- 理解 mempool 的"最低水位线"机制

## 编译和运行

```bash
make
sudo insmod slabmempooltest.ko
dmesg | tail -10
sudo rmmod slabmempooltest
```
