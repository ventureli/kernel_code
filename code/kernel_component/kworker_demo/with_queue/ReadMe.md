# Workqueue 自定义队列示例

对应文章：【内核札记-14】kworker：内核的"打工人"——workqueue 机制全解析 (2026-07-12)

演示创建自定义 workqueue：

## 运行步骤

```sh
make
sudo insmod kworker_wq.ko
dmesg | tail -20
sudo rmmod kworker_wq
```

## 知识点

- `alloc_workqueue()` — 创建自定义 workqueue
- `queue_work()` — 将 work 提交到指定 workqueue
- `destroy_workqueue()` — 销毁自定义 workqueue
- 自定义 workqueue vs 系统 `system_wq` 的选择
