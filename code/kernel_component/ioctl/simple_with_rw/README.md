# 字符设备 read/write + ioctl

对应文章：【内核札记-07】read、write 与 VFS：让字符设备像文件一样读写 (2026-07-04)

在 ioctl 字符设备基础上增加了 read/write 支持。

## 代码文件

- `ioctl_demo_rw.c` — 透传模式：write 写入的数据用 read 读出
- `ioctl_demo_rw_state.c` — 状态机模式：write 发送控制命令（字符串），read 返回模块内部状态

## 操作

```sh
# 编译
make

# 加载模块
sudo insmod ioctl_demo_rw.ko  # 或 ioctl_demo_rw_state.ko

# 测试 write/read
echo "hello kernel" > /dev/ioctl_rw_dev
cat /dev/ioctl_rw_dev

# 编译用户空间测试程序
gcc -o user_demo user_space_demo.c
sudo ./user_demo

# 卸载
sudo rmmod ioctl_demo_rw
```

## 知识点

- `file_operations.read` / `.write` — VFS 读写回调
- `copy_from_user()` / `copy_to_user()` — 用户-内核数据拷贝
- VFS 通用文件模型 — 一切皆文件
- `struct file_operations` — 驱动与 VFS 的接口契约
