# Workqueue 带数据示例

对应文章：【内核札记-14】kworker：内核的"打工人"——workqueue 机制全解析 (2026-07-12)

演示如何向 work 传递自定义数据：
- 将数据嵌入 `work_struct` 所在的结构体
- 通过 `container_of` 从 `work_struct` 反推外層结构体
- 在 work 处理函数中访问自定义数据

## 编译和运行

```bash
make
sudo insmod work_struct_with_data.ko
dmesg | tail -10
sudo rmmod work_struct_with_data
```
