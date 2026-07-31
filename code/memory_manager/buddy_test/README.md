# 伙伴分配器示例

对应文章：【内核札记-24】伙伴分配器：内核的"物理内存银行"怎么记账 (2026-07-23)

演示内核伙伴系统（Buddy Allocator）的使用：
- `alloc_pages()` 申请 2^order 个物理页
- `__free_pages()` 释放物理页
- 尝试不同 order 值观察分配行为
- 理解伙伴系统的分裂与合并机制

## 编译和运行

```bash
make
sudo insmod allocpagetest.ko
dmesg | tail -10
sudo rmmod allocpagetest
```
