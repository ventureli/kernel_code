# 等待队列 - 手工用法

对应文章：【内核札记-11】等待队列：当条件不满足时，优雅地让出 CPU (2026-07-08)

最底层的等待队列用法，不使用任何宏封装：
- 手动定义 `wait_queue_head_t` 和 `wait_queue_entry_t`
- 手动调用 `init_waitqueue_head()` / `add_wait_queue()`
- 手动 `set_current_state(TASK_INTERRUPTIBLE)` 让出 CPU
- 手动 `schedule()` 触发调度

## 编译和运行

```bash
make
sudo insmod normal_wait.ko
dmesg | tail -10
sudo rmmod normal_wait
```
