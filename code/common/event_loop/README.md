# event_loop —— 两种事件循环的最简 demo

配合软总线第 6 篇（#95）「通用 common」的 Looper 机制，两套最小实现，把「loop 线程怎么睡、怎么醒」这件事跑给你看。

```
event_loop/
├── cond_wait/
│   └── cond_wait_loop.c     经典版：pthread condvar + 互斥锁 + 按时间排序链表
└── epoll_eventfd/
    └── epoll_loop.c         epoll 版：一个 epoll 统一等 eventfd + signalfd + socketpair + timerfd
```

两版都遵守同一条主线：**loop 直接跑在主线程里**，额外只开 1 个 producer 线程演示「别的线程从任何地方扔消息」。

## 两种实现的对比

| | cond_wait 版 | epoll 版 |
|---|---|---|
| 对应原型 | OH 软总线 `message_handler.c`（SoftBusLooper） | Android Looper / 软总线连接层 reactor |
| 睡眠 | `pthread_cond_wait` / `cond_timedwait` | `epoll_wait` |
| 唤醒 | `pthread_cond_broadcast` | `write(eventfd)` |
| 延迟消息 | 链表按 time 排序 + 带超时 `cond_timedwait` | 链表按 time 排序 + `timerfd` 定时 |
| 能等的外部事件 | ❌ 只能被**同进程线程**唤醒 | ✅ 信号（signalfd）/ 其他进程数据（socketpair）/ 定时器（timerfd） |

**一句话结论**：condvar 是本进程内存里的同步原语，只能线程间用；epoll 等的「事件」可以来自**进程外**（信号、别的进程/网络的数据），所以凡是还要伺候 socket/信号/定时器的场景必须用 epoll，纯消息分发用 condvar 就够。

## 编译运行

cond_wait 版（pthread 是 POSIX，macOS / Linux 都能编）：

```bash
cd event_loop/cond_wait
gcc cond_wait_loop.c -o cond_wait_loop -pthread
./cond_wait_loop
```

epoll 版（epoll/eventfd/signalfd/timerfd 是 Linux 专有，**macOS 编不了**，在 WSL/Linux 上跑）：

```bash
cd event_loop/epoll_eventfd
gcc epoll_loop.c -o epoll_loop -pthread
./epoll_loop
```

## 预期输出

cond_wait 版：`what=1,2` 立即处理；`what=3,4` 约 300ms 后（另一线程投）；`what=5` 约 1s 后（延迟消息，靠 `cond_timedwait`）；约 2s 后收到 QUIT 退出。

epoll 版：`what=1,2` 立即（eventfd 线程消息）→ 约 500ms 后子进程的 socket 数据进来、再过约 300ms SIGUSR1 信号进来；`what=5` 约 1s 后（timerfd 定时）；约 2s 后 QUIT 退出。信号、跨进程数据、定时器这三类源都被同一个 `epoll_wait` 接住。
