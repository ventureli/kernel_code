# ioctl 字符设备驱动

对应文章：【内核札记-06】ioctl 与字符设备：打开 /dev 下的大门 (2026-07-03)

这个 demo 创建了一个字符设备 `/dev/ioctl_dev`，通过给这个设备发送 ioctl 命令可以进行通信。

## 操作步骤

1. 编译内核模块：
```sh
make
```

2. 加载模块：
```sh
sudo insmod ioctl_demo.ko
```

3. 编译并运行用户空间程序：
```sh
gcc -o user_demo user_space_demo.c
sudo ./user_demo
```

4. 查看结果：
```sh
dmesg | tail -10
```

5. 卸载模块：
```sh
sudo rmmod ioctl_demo
```

## 知识点

- `register_chrdev()` — 注册字符设备
- `file_operations.unlocked_ioctl` — ioctl 回调函数
- `_IOW` / `_IOR` / `_IOWR` — ioctl 命令编码宏
- `copy_from_user()` / `copy_to_user()` — 内核与用户空间数据交换
