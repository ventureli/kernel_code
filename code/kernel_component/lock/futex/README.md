# Futex 用户态锁示例

对应文章：【内核札记-09】锁的江湖：spinlock、futex、mutex 与 atomic——内核并发控制的四种武器 (2026-07-06)

纯用户态程序，演示 futex（快速用户空间互斥锁）：
- 手写 futex 锁，不依赖 pthread 库
- 使用 `__atomic_compare_exchange_n` 实现 CAS 操作
- 通过 `syscall(SYS_futex, ...)` 实现睡眠/唤醒
- 两个线程竞争同一把锁

## 编译和运行

```bash
gcc -o futex_thread futex_thread.c -lpthread
./futex_thread
```
