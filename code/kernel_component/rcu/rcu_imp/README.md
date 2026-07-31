# RCU 并发示例

对应文章：【内核札记-10】RCU：不加锁也能并发的黑魔法 (2026-07-07)

真正的 RCU 并发演示——读者和写者同时跑：
- 两个读者线程持续读取共享指针指向的数据
- 一个写者线程每隔一段时间更新数据
- 读者通过 `rcu_read_lock()` 保证读取期间数据不被释放
- 写者通过 `synchronize_rcu()` 等待所有读者完成

## 编译和运行

```bash
make
sudo insmod rcu.ko
dmesg | tail -20
sudo rmmod rcu
```
