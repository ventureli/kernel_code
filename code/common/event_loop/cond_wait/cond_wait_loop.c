/*
 * cond_wait_loop.c —— 事件循环「经典版」最简 demo
 *
 * 机制：pthread 条件变量 + 互斥锁 + 按到期时间排序的链表
 * 对应：OH 软总线 core/common/message_handler/message_handler.c
 *       （SoftBusLooper 的 LoopTask / PostMessageAtTime 就是这一套）
 *
 * 核心一句话：loop 就睡在 pthread_cond_wait 上（CPU 归零），
 *             别人扔消息 = 加锁 + 插链表 + cond_broadcast 叫醒。
 *             延迟消息 = 链表按 time 排序 + cond_timedwait 睡到最早到期时间。
 *             （这里的「定时」靠 cond_timedwait，等价于 epoll 版的 timerfd）
 *
 * 线程模型：loop 直接跑在【主线程】里；只有一个 producer 线程演示「别的线程扔消息」。
 *
 * 编译：gcc cond_wait_loop.c -o cond_wait_loop -pthread
 * （pthread 是 POSIX，macOS / Linux 都能编；本 demo 定时用 gettimeofday/REALTIME）
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>

#define WHAT_QUIT 0   /* 收到这条消息 loop 退出 */

/* 一条消息：what 当类型，arg1 当数据，time_ms 是到期时间（毫秒） */
typedef struct Msg {
    int what;
    uint64_t arg1;
    int64_t time_ms;
    struct Msg *next;
} Msg;

/* 一个 Looper：一把锁 + 一个条件变量 + 一条按时间排序的链表队列 */
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    Msg *head;          /* 按 time_ms 升序的链表，队头最早到期 */
} Looper;

static int64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void looper_init(Looper *l)
{
    pthread_mutex_init(&l->lock, NULL);
    pthread_cond_init(&l->cond, NULL);
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

/* 立即投递：到期时间 = now */
static void looper_post(Looper *l, int what, uint64_t arg1)
{
    Msg *m = calloc(1, sizeof(Msg));
    m->what = what;
    m->arg1 = arg1;
    m->time_ms = now_ms();

    pthread_mutex_lock(&l->lock);
    enqueue_sorted(l, m);
    pthread_cond_broadcast(&l->cond);   /* 关键：叫醒睡着的 loop */
    pthread_mutex_unlock(&l->lock);
}

/* 延迟投递：到期时间 = now + delay_ms */
static void looper_post_delay(Looper *l, int what, uint64_t arg1, int delay_ms)
{
    Msg *m = calloc(1, sizeof(Msg));
    m->what = what;
    m->arg1 = arg1;
    m->time_ms = now_ms() + delay_ms;

    pthread_mutex_lock(&l->lock);
    enqueue_sorted(l, m);
    pthread_cond_broadcast(&l->cond);
    pthread_mutex_unlock(&l->lock);
}

/* 消息处理回调：在 loop 里串行执行，别处永远碰不到共享状态 */
static void handle_message(Msg *m)
{
    printf("[loop] 处理 what=%d arg1=%llu\n", m->what, (unsigned long long)m->arg1);
    fflush(stdout);
}

/* loop：直接跑在【主线程】里，串行消费链表里的消息，遇到 WHAT_QUIT 返回 */
static void loop_run(Looper *l)
{
    for (;;) {
        pthread_mutex_lock(&l->lock);

        /* 队列空 → 无限等，直到有人 broadcast */
        while (l->head == NULL) {
            pthread_cond_wait(&l->cond, &l->lock);
        }

        Msg *m = l->head;
        int64_t now = now_ms();
        if (now >= m->time_ms) {
            /* 到点了：出队，解锁后再处理（处理不占锁） */
            l->head = m->next;
            pthread_mutex_unlock(&l->lock);
            if (m->what == WHAT_QUIT) {
                free(m);
                break;
            }
            handle_message(m);
            free(m);
        } else {
            /* 还没到点：带超时睡到「最早到期时间」，到点自动醒（这就是 cond 版的定时器） */
            struct timespec ts;
            ts.tv_sec  = m->time_ms / 1000;
            ts.tv_nsec = (m->time_ms % 1000) * 1000000;
            pthread_cond_timedwait(&l->cond, &l->lock, &ts);
            pthread_mutex_unlock(&l->lock);
            /* 回到循环顶：可能是超时到点，也可能是新消息提前叫醒 */
        }
    }

    printf("[loop] 退出（主线程）\n");
}

/* 唯一一个额外线程：模拟「另一个线程从代码任何地方」往 loop 里扔消息 */
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

    /* 主线程自己先投：两条立即 + 一条延迟 1s + 一条 2s 后的 QUIT */
    looper_post(&l, 1, 100);
    looper_post(&l, 2, 200);
    looper_post_delay(&l, 5, 500, 1000);
    looper_post_delay(&l, WHAT_QUIT, 0, 2000);

    loop_run(&l);                         /* 主线程直接跑 loop，处理到 QUIT 返回 */

    pthread_join(ptid, NULL);
    pthread_mutex_destroy(&l.lock);
    pthread_cond_destroy(&l.cond);
    return 0;
}
