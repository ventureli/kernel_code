# 自旋锁内核模块

对应文章：【内核札记-09】锁的江湖：spinlock、futex、mutex 与 atomic——内核并发控制的四种武器 (2026-07-06)

演示内核自旋锁（spinlock）的使用：
- 两个内核线程争抢一把 spinlock
- 持有锁期间 `mdelay(100)`，在锁上自旋等待
- 对比 mutex（睡眠等待）和 spinlock（忙等待）的区别
- 适用于短临界区、不能睡眠的上下文

## 编译和运行

```bash
make
sudo insmod spinlock_demo.ko
dmesg | tail -10
sudo rmmod spinlock_demo
```
