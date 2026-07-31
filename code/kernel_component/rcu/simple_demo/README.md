# RCU 简单示例

对应文章：【内核札记-10】RCU：不加锁也能并发的黑魔法 (2026-07-07)

最简单的 RCU（Read-Copy-Update）用法示例：
- 模块初始化时依次执行读、写、再读
- 使用 `rcu_read_lock()` / `rcu_read_unlock()` 保护读侧
- 使用 `rcu_assign_pointer()` / `synchronize_rcu()` 完成写侧更新
- 演示 RCU 的"订阅-发布"机制

## 编译和运行

```bash
make
sudo insmod rcu_learn.ko
dmesg | tail -10
sudo rmmod rcu_learn
```
