# 进程暂停与恢复

对应文章：【内核札记-05】暂停、恢复与进程状态：用 SIGSTOP 和 SIGCONT 操控进程的生杀大权 (2026-07-02)

## 运行结果

```sh
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/task_schedule/schedule_test$ make
xuan@xuan-u22:~/Desktop/day_work/kernel_learn/code/task_schedule/schedule_test$ cd user_space_demo && make

# 终端1：运行用户空间程序
xuan@xuan-u22:~/.../user_space_demo$ ./timer_demo
Timer demo running. PID: 5678
[1] 5678

# 终端2：发送 SIGSTOP
kill -SIGSTOP 5678
# timer_demo 停止输出

# 发送 SIGCONT
kill -SIGCONT 5678
# timer_demo 继续输出
```

## 使用内核模块控制

```sh
sudo insmod schedule_demo.ko target_pid=5678
# 内核模块会自动发送 SIGSTOP 和 SIGCONT
```

## 知识点

- SIGSTOP / SIGCONT — 暂停和恢复进程
- 进程状态：TASK_RUNNING / TASK_STOPPED / TASK_INTERRUPTIBLE
- `send_signal()` 内核 API
