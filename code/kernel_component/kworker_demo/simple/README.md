# Workqueue 简单示例

对应文章：【内核札记-14】kworker：内核的"打工人"——workqueue 机制全解析 (2026-07-12)

Workqueue 的 "Hello World" 示例：
- 使用系统默认 workqueue（`system_wq`）
- `INIT_WORK` 初始化 work
- `schedule_work` 将任务提交到 workqueue
- kworker 内核线程在进程上下文中执行任务

## 编译和运行

```bash
make
sudo insmod kworker_demo.ko
dmesg | tail -10
sudo rmmod kworker_demo
```
