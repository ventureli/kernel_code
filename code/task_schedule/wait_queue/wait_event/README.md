# 等待队列 - wait_event 宏族

对应文章：【内核札记-11】等待队列：当条件不满足时，优雅地让出 CPU (2026-07-08)

使用内核提供的 `wait_event` 宏族简化等待队列操作：
- `wait_event_interruptible(wq, condition)` 一行替代手工流程
- 自动处理 `set_current_state` + `schedule()` + 唤醒后检查
- 对比手工方式，代码量减少 80%

## 编译和运行

```bash
make
sudo insmod wait_event_demo1.ko
dmesg | tail -10
sudo rmmod wait_event_demo1
```
