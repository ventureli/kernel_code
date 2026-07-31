# mprotect 内存保护示例

对应文章：【内核札记-19】mmap：一块内存，两个进程——虚拟内存(二) (2026-07-17)

演示 mmap 家族中的 mprotect 系统调用：
- `mmap_dt2_r.c` — 基础 mmap 读写示例
- `mmap_dt2.c` — mmap 匿名映射示例
- `mmap_protect_dt2.c` — 运行时修改内存访问权限
  - `PROT_READ | PROT_WRITE` → 可读写
  - `PROT_READ` → 只读（写操作触发 SIGSEGV）
  - `PROT_NONE` → 不可访问

## 编译和运行

```bash
gcc -o mmap_protect mmap_protect_dt2.c
./mmap_protect
```
