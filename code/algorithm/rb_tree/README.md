# 内核红黑树 rbtree 示例

对应文章：【内核札记-13】内核数据结构：从 list_head 到 maple_tree——钩子、红黑树与"数组变树"的艺术 (2026-07-10)

演示内核红黑树的使用：
- `rb_root` 定义树根
- `rb_insert_color` + 手动比较实现插入
- `rb_entry` 从树节点反推结构体
- `rb_first` / `rb_next` 中序遍历
- 对比链表 O(n) 和红黑树 O(log n) 的查找性能

## 编译和运行

```bash
make
sudo insmod rbtreedt.ko
dmesg | tail -10
sudo rmmod rbtreedt
```
