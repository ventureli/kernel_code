# proc seq_file 示例

对应文章：【内核札记-15】/proc 文件系统：内核的"状态面板"——从 `cat /proc/cpuinfo` 到自己写 proc 文件 (2026-07-13)

演示如何使用 `seq_file` 接口创建 proc 文件：
- 相比 `proc_create()` 更高级的 API
- 自动处理分页（PAGE_SIZE 边界）
- 适用于输出大量数据的场景（类似 `/proc/cpuinfo`）
- 实现 `seq_start` / `seq_next` / `seq_show` / `seq_stop` 回调

## 编译和运行

```bash
make
sudo insmod proc_seq_file.ko
cat /proc/seq_demo
sudo rmmod proc_seq_file
```
