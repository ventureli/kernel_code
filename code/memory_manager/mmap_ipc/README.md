# mmap 进程间通信示例

对应文章：【内核札记-19】mmap：一块内存，两个进程——虚拟内存(二) (2026-07-17)

演示使用 mmap 实现进程间共享内存通信：
- `mmap_writer.c` — 写进程：mmap 文件到内存，写入消息
- `mmap_reader.c` — 读进程：mmap 同一个文件，读取共享内容
- `mmap_double_writer.c` — 两个写进程竞争写入
- 使用 `MAP_SHARED` 标志实现真正的内存共享
- 通过 System V 信号量同步读写操作

## 编译和运行

```bash
gcc -o writer mmap_writer.c
gcc -o reader mmap_reader.c
# 先运行 reader，再运行 writer
./reader &
./writer "hello from mmap"
```
