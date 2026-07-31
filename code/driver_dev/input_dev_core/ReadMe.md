# 虚拟输入设备驱动

对应文章：【内核札记-16】输入子系统：从一次物理按键到 `/dev/input/eventX`——input_dev 驱动全解析 (2026-07-13)

创建一个虚拟 input_dev，定时上报按键事件。

## 编译和运行

```sh
make
sudo insmod input_dev_test.ko
```

通过下面命令可以看到 eventX 代表的是什么：
```sh
cat /proc/bus/input/devices
```

用户空间读取按键事件：
```sh
cd user_read_demo
make
sudo ./user_read_demo
```

```sh
sudo rmmod input_dev_test
```

## 知识点

- `input_allocate_device()` / `input_register_device()` — 创建并注册输入设备
- `input_report_key()` / `input_sync()` — 上报按键事件
- `set_bit(EV_KEY, ...)` — 设置设备支持的事件类型
- `/dev/input/eventX` — 输入事件设备节点
