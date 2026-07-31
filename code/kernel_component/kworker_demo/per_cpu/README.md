# Workqueue per-CPU 队列示例

对应文章：【内核札记-14】kworker：内核的"打工人"——workqueue 机制全解析 (2026-07-12)

演示 workqueue 的 per-CPU 队列机制：
- 创建专属 workqueue
- 每个 CPU 拥有独立的工作队列
- 任务被提交到当前 CPU 的队列中执行

## 编译和运行

```bash
make
sudo insmod per_cpu_queue_demo.ko
dmesg | tail -10
sudo rmmod per_cpu_queue_demo
```
