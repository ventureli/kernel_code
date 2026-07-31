# 内核位锁哈希链表 hlist_bl 示例

对应文章：【内核札记-13】内核数据结构：从 list_head 到 maple_tree——钩子、红黑树与"数组变树"的艺术 (2026-07-10)

演示内核位锁哈希链表 `hlist_bl` 的使用：
- 在每个桶头嵌入一个 bit-spinlock
- `hlist_bl_lock` / `hlist_bl_unlock` 细粒度锁
- 适用于高并发哈希表场景（如 VFS 的 dentry cache）

## 编译和运行

```bash
make
sudo insmod hlist_bl_demo.ko
dmesg | tail -10
sudo rmmod hlist_bl_demo
```
