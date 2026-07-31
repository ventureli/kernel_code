# Tasklet 软中断示例

对应文章：【内核札记-31】运行上下文：什么时候能睡，什么时候不能 (2026-07-30)

## 编译和运行

```sh
make
sudo insmod tasklet_demo.ko
dmesg | tail -10
sudo rmmod tasklet_demo
```

## 知识点

- `tasklet_init()` — 初始化 tasklet
- `tasklet_schedule()` — 调度 tasklet 在软中断上下文执行
- tasklet 回调不能睡眠（软中断上下文）
- `tasklet_kill()` — 等待 tasklet 完成再卸载模块
- 进程上下文 vs 软中断上下文的区别
- `in_irq()` / `in_softirq()` / `in_interrupt()` 判断当前上下文
