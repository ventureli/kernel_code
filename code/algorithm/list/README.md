# 内核链表 list_head 示例

对应文章：【内核札记-13】内核数据结构：从 list_head 到 maple_tree——钩子、红黑树与"数组变树"的艺术 (2026-07-10)

演示内核链表 `list_head` 的使用：
- 用户态程序，包含精简版 `list.h`
- `LIST_HEAD` 定义链表头
- `list_add` / `list_add_tail` 添加节点
- `list_for_each_entry` 遍历链表
- `list_del` 删除节点
- 结合 `container_of` 宏从链表指针反推结构体指针

## 编译和运行

```bash
make
./list_demo
```
