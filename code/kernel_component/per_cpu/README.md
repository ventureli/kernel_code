# per-CPU 变量示例

对应文章：【内核札记-12】per-CPU 变量：每个 CPU 人手一份，锁都不用抢 (2026-07-09)

演示内核 per-CPU 变量的使用：
- `DEFINE_PER_CPU` 定义 per-CPU 变量
- `get_cpu_var()` / `put_cpu_var()` 访问当前 CPU 的变量副本
- 每个 CPU 拥有独立的变量实例，无需加锁
- 对比全局变量 + 锁的性能优势

## 编译和运行

```bash
make
sudo insmod percputest.ko
dmesg | tail -10
sudo rmmod percputest
```
