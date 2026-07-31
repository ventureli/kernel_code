# VMA 遍历示例

对应文章：【内核札记-20】缺页异常：空头支票的兑付现场——虚拟内存(三) (2026-07-18)

内核模块，遍历进程的虚拟内存区域（VMA）：
- 通过 pid 找到目标进程的 `task_struct`
- 遍历 `mm_struct->mmap` 链表
- 打印每个 VMA 的起始/结束地址、权限标志
- 按 `vm_flags` 将 VMA 分类（可读/可写/可执行/共享/私有）
- 等价于在内核中手工复刻 `/proc/pid/maps`

## 编译和运行

```bash
make
sudo insmod vmatest.ko target_pid=1234
dmesg | tail -30
sudo rmmod vmatest
```
