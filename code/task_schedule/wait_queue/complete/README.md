# completion 完成量示例

对应文章：【内核札记-11】等待队列：当条件不满足时，优雅地让出 CPU (2026-07-08)

演示 completion（完成量）的使用——等待队列的一个特化用法：
- `init_completion()` 初始化完成量
- `wait_for_completion()` 等待事件完成
- `complete()` / `complete_all()` 通知等待者
- 适合"一次性通知"的场景

## 编译和运行

```bash
make
sudo insmod complete_all1.ko
dmesg | tail -10
sudo rmmod complete_all1
```
