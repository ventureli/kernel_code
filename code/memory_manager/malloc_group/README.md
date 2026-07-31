# kmalloc / vmalloc 对比示例

对应文章：【内核札记-22】malloc 三兄弟：kmalloc、vmalloc、malloc 的江湖地位 (2026-07-20)

内核模块，对比 kmalloc 和 vmalloc 的差异：
- `kmalloc` — 分配物理连续内存（≤ 4MB），来自 Slab 分配器
- `vmalloc` — 分配虚拟连续内存（可很大），物理页可能不连续
- 加载时各申请 10KB 内存并打印地址信息
- 卸载时分别释放

## 编译和运行

```bash
make
sudo insmod kmalloctest.ko
dmesg | tail -10
sudo rmmod kmalloctest
```
