# nice 值与优先级示例

对应文章：【内核札记-30】nice 值与优先级：谁在抢 CPU 的"蛋糕" (2026-07-29)

演示 nice 值对 CPU 调度的影响：
- 创建一对父子线程，设置不同的 nice 值
- 两个线程同时执行 CPU 密集型计算
- 观察 nice 值差异导致的 CPU 时间分配差异
- nice 值范围：-20（最高优先级）到 +19（最低优先级）

## 编译和运行

```bash
make
sudo insmod nicedt.ko
dmesg | tail -20
sudo rmmod nicedt
```
