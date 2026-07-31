# cgroup 资源控制示例

对应文章：【内核札记-17】cgroup：内核的资源管家——给进程办一张 100M 的"内存限额卡" (2026-07-15)

## 编译和运行结果

```sh
make
sudo insmod cgroupdt.ko
dmesg | tail -10
sudo rmmod cgroupdt
```

## cgroup 基本操作

```sh
# 查看当前 cgroup 层级
ls /sys/fs/cgroup/

# 创建一个 cgroup 并限制内存
sudo mkdir /sys/fs/cgroup/memory/my_group
echo 100M > /sys/fs/cgroup/memory/my_group/memory.limit_in_bytes

# 将进程加入 cgroup
echo $PID > /sys/fs/cgroup/memory/my_group/cgroup.procs
```

## 知识点

- cgroup v1 vs v2 的区别
- memory / cpu / blkio 等子系统
- `memory.limit_in_bytes` — 内存限额
- cgroup 的层级继承关系
