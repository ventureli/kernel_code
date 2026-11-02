/*
 * epoll_loop.c —— 事件循环「统一多路复用版」最简 demo
 *
 * 机制：epoll + eventfd + signalfd + socketpair + timerfd
 * 对应：Android Handler/Looper 的思路（把「消息」也变成一个 fd，塞进 epoll 一起等）
 *
 * 核心一句话：loop 睡在 epoll_wait 上（CPU 归零），所有「事件源」统一成 fd：
 *   - eventfd  ：同进程别的线程往队列里扔消息，写 8 字节叫醒 loop；
 *   - signalfd ：信号（SIGUSR1）变成一个可读 fd，不用再写信号处理函数；
 *   - socketpair：别的进程（fork 出的子进程）通过 fd 送消息进来；
 *   - timerfd  ：延迟消息的「定时器」，到点变成可读，实现 post_delay。
 *
 * 线程/进程模型：loop 直接跑在【主线程】里；只有 1 个 producer 线程（eventfd）
 *               + 1 个 fork 出的子进程（socketpair + SIGUSR1）演示跨进程。
 *
 * 编译（Linux 专用，epoll/eventfd/signalfd/timerfd 是 Linux 特有 API）：
 *   gcc epoll_loop.c -o epoll_loop -pthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/time.h>

#define WHAT_QUIT 0      /* 收到这条消息 loop 退出 */
#define MAX_EVENTS 16

/* 一条消息：what 当类型，arg1 当数据，time_ms 是到期时间（毫秒） */
typedef struct Msg {
    int what;
    uint64_t arg1;
    int64_t time_ms;
    struct Msg *next;
} Msg;

/* 一个 Looper：epoll 实例 + 各种事件源 fd + 按时间排序的队列 */
typedef struct {
    int epfd;
    int wake_fd;     /* eventfd：别的线程叫醒用 */
    int sig_fd;      /* signalfd：把 SIGUSR1 变成 fd */
    int sock_fd;     /* socketpair 本端：接收子进程发来的消息 */
    int peer_fd;     /* socketpair 对端：交给子进程 */
    int timer_fd;    /* timerfd：延迟消息的定时器 */
    pthread_mutex_t lock;
    Msg *head;       /* 按 time_ms 升序，队头最早到期 */
} Looper;

static int64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* 把一个 fd 加进 epoll（边沿触发） */
static void add_fd(int epfd, int fd)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl");
        exit(1);
    }
}

static void looper_init(Looper *l)
{
    l->epfd = epoll_create1(0);
    if (l->epfd < 0) { perror("epoll_create1"); exit(1); }

    l->wake_fd = eventfd(0, EFD_NONBLOCK);
    if (l->wake_fd < 0) { perror("eventfd"); exit(1); }
    add_fd(l->epfd, l->wake_fd);

    /* 屏蔽 SIGUSR1 并交给 signalfd，之后发信号不会打断进程，只会让 sig_fd 可读 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    l->sig_fd = signalfd(-1, &mask, SFD_NONBLOCK);
    if (l->sig_fd < 0) { perror("signalfd"); exit(1); }
    add_fd(l->epfd, l->sig_fd);

    /* socketpair：本端留给自己，对端给子进程，实现跨进程发消息 */
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) { perror("socketpair"); exit(1); }
    l->sock_fd = fds[0];
    l->peer_fd = fds[1];
    add_fd(l->epfd, l->sock_fd);

    /* timerfd：初始不启动（0 超时 = 禁能），延迟消息到点时由 reset_timerfd 重新武装 */
    l->timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (l->timer_fd < 0) { perror("timerfd_create"); exit(1); }
    add_fd(l->epfd, l->timer_fd);

    pthread_mutex_init(&l->lock, NULL);
    l->head = NULL;
}

/* 按到期时间升序插入（队头最早到期） */
static void enqueue_sorted(Looper *l, Msg *m)
{
    Msg **p = &l->head;
    while (*p != NULL && (*p)->time_ms <= m->time_ms) {
        p = &(*p)->next;
    }
    m->next = *p;
    *p = m;
}

/* 把 timerfd 重新武装成「到队头最早到期时间触发」 */
static void reset_timerfd(Looper *l, int64_t time_ms)
{
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    int64_t delay_ms = time_ms - now_ms();
    if (delay_ms < 0) delay_ms = 0;
    its.it_value.tv_sec  = delay_ms / 1000;
    its.it_value.tv_nsec = (delay_ms % 1000) * 1000000;
    timerfd_settime(l->timer_fd, 0, &its, NULL);
}

static void looper_post(Looper *l, int what, uint64_t arg1)
{
    Msg *m = calloc(1, sizeof(Msg));
    m->what = what;
    m->arg1 = arg1;
    m->time_ms = now_ms();

    pthread_mutex_lock(&l->lock);
    enqueue_sorted(l, m);
    pthread_mutex_unlock(&l->lock);

    /* 立即消息：写 eventfd 叫醒 loop */
    uint64_t one = 1;
    (void)write(l->wake_fd, &one, sizeof(one));
}

static void looper_post_delay(Looper *l, int what, uint64_t arg1, int delay_ms)
{
    Msg *m = calloc(1, sizeof(Msg));
    m->what = what;
    m->arg1 = arg1;
    m->time_ms = now_ms() + delay_ms;

    pthread_mutex_lock(&l->lock);
    enqueue_sorted(l, m);
    /* 延迟消息：重新武装 timerfd 到最早到期时间 */
    reset_timerfd(l, l->head->time_ms);
    pthread_mutex_unlock(&l->lock);
}

/* 把队列里所有「已到期」的消息全部取出来，返回一条链表（不占锁处理） */
static Msg *drain_ready(Looper *l)
{
    Msg *head = NULL, *tail = NULL;
    int64_t now = now_ms();
    while (l->head != NULL && l->head->time_ms <= now) {
        Msg *m = l->head;
        l->head = m->next;
        m->next = NULL;
        if (tail) tail->next = m; else head = m;
        tail = m;
    }
    /* 队列里还有未到期的 → 重新武装 timerfd 到下一个到期点 */
    if (l->head != NULL) {
        reset_timerfd(l, l->head->time_ms);
    }
    return head;
}

/* 消息处理回调：在 loop 里串行执行，别处永远碰不到共享状态 */
static void handle_message(Msg *m)
{
    if (m->what == WHAT_QUIT) return;
    printf("[loop] 处理 what=%d arg1=%llu\n", m->what, (unsigned long long)m->arg1);
    fflush(stdout);
}

/* 把取出的消息逐条处理掉，遇到 WHAT_QUIT 返回 1 */
static int process_ready(Msg *head)
{
    int quit = 0;
    while (head != NULL) {
        Msg *next = head->next;
        if (head->what == WHAT_QUIT) quit = 1;
        else handle_message(head);
        free(head);
        head = next;
    }
    return quit;
}

static int read_eventfd(Looper *l)
{
    uint64_t v;
    while (read(l->wake_fd, &v, sizeof(v)) > 0) { /* drain */ }
    pthread_mutex_lock(&l->lock);
    Msg *ready = drain_ready(l);
    pthread_mutex_unlock(&l->lock);
    return process_ready(ready);
}

static int read_timerfd(Looper *l)
{
    uint64_t expirations;
    while (read(l->timer_fd, &expirations, sizeof(expirations)) > 0) { /* drain */ }
    pthread_mutex_lock(&l->lock);
    Msg *ready = drain_ready(l);
    pthread_mutex_unlock(&l->lock);
    return process_ready(ready);
}

static void read_signalfd(Looper *l)
{
    struct signalfd_siginfo si;
    while (read(l->sig_fd, &si, sizeof(si)) > 0) {
        printf("[loop] 收到信号 SIGUSR1（来自进程 %u）\n", si.ssi_pid);
        fflush(stdout);
    }
}

static void read_socketfd(Looper *l)
{
    char buf[64];
    ssize_t n;
    while ((n = read(l->sock_fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("[loop] 收到跨进程消息：%s", buf);
        fflush(stdout);
    }
}

/* loop：直接跑在【主线程】里，epoll_wait 一次等所有事件源，遇到 WHAT_QUIT 返回 */
static void loop_run(Looper *l)
{
    int quit = 0;
    while (!quit) {
        struct epoll_event evs[MAX_EVENTS];
        int n = epoll_wait(l->epfd, evs, MAX_EVENTS, -1);   /* 阻塞，睡到有事件 */
        for (int i = 0; i < n && !quit; i++) {
            int fd = evs[i].data.fd;
            if (fd == l->wake_fd) {
                quit = read_eventfd(l);
            } else if (fd == l->timer_fd) {
                quit = read_timerfd(l);
            } else if (fd == l->sig_fd) {
                read_signalfd(l);
            } else if (fd == l->sock_fd) {
                read_socketfd(l);
            }
        }
    }
    printf("[loop] 退出（主线程）\n");
}

/* 唯一一个额外线程：模拟「同进程另一个线程」从任何地方往 loop 里扔消息 */
static void *producer_thread(void *arg)
{
    Looper *l = (Looper *)arg;
    usleep(300 * 1000);                  /* 300ms 后才投，模拟异步来源 */
    looper_post(l, 3, 300);
    looper_post(l, 4, 400);
    return NULL;
}

int main(void)
{
    Looper l;
    looper_init(&l);

    pthread_t ptid;
    pthread_create(&ptid, NULL, producer_thread, &l);   /* 只有这一个额外线程 */

    /* fork 一个子进程：它持 peer_fd，模拟「别的进程」往 loop 里发消息 + 发信号 */
    pid_t pid = fork();
    if (pid == 0) {
        /* 子进程：关掉自己用不到的 fd，然后给父进程发消息 */
        close(l.wake_fd);
        close(l.sig_fd);
        close(l.sock_fd);
        usleep(500 * 1000);
        const char *msg = "hello from child process\n";
        write(l.peer_fd, msg, strlen(msg));
        usleep(300 * 1000);
        kill(getppid(), SIGUSR1);
        close(l.peer_fd);
        _exit(0);
    }

    /* 主线程自己先投：两条立即 + 一条延迟 1s + 一条 2s 后的 QUIT */
    looper_post(&l, 1, 100);
    looper_post(&l, 2, 200);
    looper_post_delay(&l, 5, 500, 1000);
    looper_post_delay(&l, WHAT_QUIT, 0, 2000);

    loop_run(&l);                         /* 主线程直接跑 loop，处理到 QUIT 返回 */

    pthread_join(ptid, NULL);
    return 0;
}
