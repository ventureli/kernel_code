# 内核定时器示例

对应文章：【内核札记-25】内核定时器：jiffies 滴答声中的时间魔法 (2026-07-24)

演示内核定时器（timer_list）的使用：
- `timer_setup()` 初始化定时器
- `mod_timer()` 设置/修改超时时间
- 定时器回调在软中断上下文执行（不能睡眠）
- 实现每隔 2 秒打印一次的"心跳定时器"
- 卸载时 `del_timer_sync()` 确保定时器已停止

## 编译和运行

```bash
make
sudo insmod timer_demo.ko
dmesg | tail -20
sudo rmmod timer_demo
```
