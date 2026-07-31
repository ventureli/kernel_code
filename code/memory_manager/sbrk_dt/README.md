# sbrk 系统调用示例

对应文章：【内核札记-18】sbrk：malloc 背后的"地皮批发商"——虚拟内存(一) (2026-07-16)

## 运行结果

```sh
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/memory_manager/sbrk_dt$ gcc -o sbrk_dt sbrk_dt.c
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/memory_manager/sbrk_dt$ ./sbrk_dt

初始 program break: 0x55a7b3c4d000
sbrk(0) = 0x55a7b3c4d000  (当前 break)
sbrk(4096) 之后: 0x55a7b3c4e000  (增加了 4KB)
sbrk(0) = 0x55a7b3c4e000  (确认新位置)
```

## 知识点

- `sbrk()` / `brk()` — 调整进程堆的边界（program break）
- program break — 堆顶，`malloc` 从这里分配小内存
- 虚拟内存中堆的增长方向（向上增长）
- `sbrk` vs `mmap` — 小块用 brk，大块用 mmap
