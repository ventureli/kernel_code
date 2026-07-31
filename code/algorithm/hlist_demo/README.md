# 内核哈希链表 hlist 示例

对应文章：【内核札记-13】内核数据结构：从 list_head 到 maple_tree——钩子、红黑树与"数组变树"的艺术 (2026-07-10)

演示内核哈希链表 `hlist` 的使用：
- 内核模块形式
- `HLIST_HEAD` 定义哈希桶数组
- `hlist_add_head` 添加到桶头
- `hlist_for_each_entry` 遍历桶内节点
- 适用于哈希表冲突链场景

## 编译和运行

```bash
make
sudo insmod hlist_demo.ko
dmesg | tail -10
sudo rmmod hlist_demo
```
