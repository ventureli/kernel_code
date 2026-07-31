# proc 文件系统示例

对应文章：【内核札记-15】/proc 文件系统：内核的"状态面板"——从 `cat /proc/cpuinfo` 到自己写 proc 文件 (2026-07-13)

演示如何使用 `proc_create()` 创建自定义 proc 文件：
- 创建 `/proc/hello_proc` 文件节点
- 实现 `read` 回调，返回内核中的动态数据
- 演示 proc 文件作为内核与用户空间的数据通道

## 编译和运行

```bash
make
sudo insmod proc_demo.ko
cat /proc/hello_proc
sudo rmmod proc_demo
```
