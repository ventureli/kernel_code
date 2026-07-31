# 内核线程创建示例

对应文章：【内核札记-29】内核线程：没有用户空间的"幕后工作者" (2026-07-28)

演示如何创建内核线程：
- `kthread_create()` 创建内核线程
- `wake_up_process()` 唤醒线程开始执行
- 内核线程没有用户空间（`mm = NULL`），只能访问内核地址空间
- 对比用户进程 — 内核线程运行在进程上下文，可以睡眠

## 编译和运行

```bash
make
sudo insmod ktcreate_nodedt.ko
dmesg | tail -10
sudo rmmod ktcreate_nodedt
```
