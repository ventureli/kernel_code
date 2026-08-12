## 内存屏障演示 (Store Buffer 测试)

经典 Store Buffer 乱序：

```
CPU0: x=1; smp_mb(); r0=y;
CPU1: y=1; smp_mb(); r1=x;
```

无屏障时出现 `r0=0 && r1=0`（两个 CPU 同时读到旧值）。加 `smp_mb()` 后不会。

### 编译运行

```bash
make

# 无屏障
sudo insmod barrier_mod.ko use_barrier=0 sec=5
dmesg | tail -8

# 有屏障
sudo insmod barrier_mod.ko use_barrier=1 sec=5
dmesg | tail -8
```

实测结果（x86 VM，5s）：
- 无屏障：353 轮中 2 次乱序（~0.6%）
- 有屏障：281 轮中 0 次乱序
