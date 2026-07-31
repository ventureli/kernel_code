# 用户空间读取 input 事件示例

对应文章：【内核札记-16】输入子系统：从一次物理按键到 `/dev/input/eventX`——input_dev 驱动全解析 (2026-07-13)

用户空间程序，配合内核模块 `input_dev_test` 使用：
- 打开 `/dev/input/eventX` 设备文件
- 读取 `struct input_event` 结构体
- 解析按键事件的类型、代码和值
- 验证虚拟输入设备的数据上报

## 编译和运行

```bash
make
sudo ./user_read_demo
```
