# 等待队列 - 自定义唤醒函数

对应文章：【内核札记-11】等待队列：当条件不满足时，优雅地让出 CPU (2026-07-08)

演示自定义唤醒回调函数的使用：
- 通过 `DEFINE_WAIT_FUNC` 注册自定义唤醒函数
- 在唤醒时执行额外逻辑（日志记录等）
- 理解 `autoremove_wake_function` 和自定义函数的区别

## 编译和运行

```bash
make
sudo insmod user_wait_sync.ko
dmesg | tail -10
sudo rmmod user_wait_sync
```
