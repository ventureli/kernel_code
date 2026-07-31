# 内核 maple_tree 示例

对应文章：【内核札记-13】内核数据结构：从 list_head 到 maple_tree——钩子、红黑树与"数组变树"的艺术 (2026-07-10)

演示内核 maple_tree 的使用：
- `DEFINE_MTREE` 初始化 maple tree
- `mtree_store` 插入键值对
- `mtree_load` 按 key 查找
- `mtree_erase` 删除节点
- maple_tree 是 Linux 6.1 引入的新数据结构，用于替代 VMA 管理中的红黑树

## 编译和运行

```bash
make
sudo insmod maple_tree_demo.ko
dmesg | tail -10
sudo rmmod maple_tree_demo
```
