# 内核模块发信号给用户进程

对应文章：【内核札记-03】内核模块发信号：从内核主动通知用户进程 (2026-07-01)

## 运行步骤

1. 先编译内核模块和用户空间程序：
```sh
cd hello_world_param_sig
make
cd user_space_demo
make
```

2. 运行用户空间程序（会显示其 PID）：
```sh
./wait_sig_demo
# 输出: Waiting for signal 42... My PID is 12345
```

3. 在另一个终端加载内核模块并传入 PID：
```sh
sudo insmod hello_world_with_param_sig.ko pid=12345
```

4. 用户空间程序收到信号：
```sh
# 输出: Received signal 42! Kernel module sent me a signal!
```

## 知识点

- `send_sig()` / `kill_pid()` — 内核向用户进程发送信号
- 实时信号（SIGRTMIN ~ SIGRTMAX）— 用户自定义用途
- `signal()` / `sigaction()` — 用户空间注册信号处理器
- `/sys/module/<name>/parameters/pid` — 通过 sysfs 向内核传递 PID
