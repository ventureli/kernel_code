# 互斥锁内核模块

对应文章：【内核札记-08】互斥锁：当两个进程同时写设备时，打架了怎么办 (2026-07-05)

演示内核中的互斥锁（mutex）使用：
- 创建两个内核线程，同时争抢一把 mutex
- 持有锁的线程 sleep 2 秒，模拟临界区操作
- 另一个线程在锁上阻塞等待
- 卸载时通过 `mutex_lock` + `kthread_stop` 安全清理

## 编译和运行

```bash
make
sudo insmod mutex_demo.ko
dmesg | tail -20
sudo rmmod mutex_demo
```
