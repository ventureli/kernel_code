/*
 * my_param_server —— 系统参数服务极简 demo（服务端）
 *
 * 职责（对应 OpenHarmony 的 param_service，做了大量简化）：
 *   1. 创建并持有共享内存，是唯一写者
 *   2. 监听 Unix 域套接字
 *   3. 用 epoll 管「监听 socket + N 条客户端连接」
 *   4. 处理 SET（写共享内存 + 通知 watcher）和 WATCH（注册监听）
 *
 * 简化点：
 *   - 共享内存是简单数组，不是 Trie
 *   - 没有 DAC/SELinux 权限
 *   - 通知直接走 socket，没有 param_watcher 那一层
 *   - 读消息假设一次 read 拿到一整条（demo 简化，不做粘包处理）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "my_param_common.h"

static struct param_area *g_area = NULL;   /* 共享内存映射 */

/* 监听者列表（demo 用定长数组，真实实现是链表） */
static struct watcher {
    int fd;                 /* 这个监听者的 socket 连接 */
    char name[MAX_NAME];    /* 监听哪个参数 */
    char value[MAX_VALUE];  /* 等什么值；空串 = 任意值都命中 */
} g_watchers[MAX_PARAMS];
static int g_watcher_count = 0;

/* 初始化共享内存：shm_open + ftruncate + mmap */
static void init_shared_memory(void)
{
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd < 0) { perror("shm_open"); exit(1); }
    /* shm_open 刚创建的共享内存对象大小是 0；
     * ftruncate 把它的「大小」设置成 sizeof(struct param_area) 字节（扩展部分填 0），
     * 这样后面的 mmap 才有足够的空间去映射。不调它的话 mmap 0 字节会失败。 */
    if (ftruncate(fd, sizeof(struct param_area)) != 0) { perror("ftruncate"); exit(1); }
    g_area = mmap(NULL, sizeof(struct param_area), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (g_area == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);

    if (g_area->magic != SHM_MAGIC) {   /* 首次启动，清零初始化 */
        memset(g_area, 0, sizeof(*g_area));
        g_area->magic = SHM_MAGIC;
    }
    printf("shared memory ready, %u params\n", g_area->count);
}

/* 初始化监听 socket：socket + bind + listen */
static int init_listen_socket(void)
{
    unlink(SOCKET_PATH);   /* 清理上次残留的 socket 文件 */
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) { perror("bind"); exit(1); }
    /* listen：把 socket 转为「监听」状态，开始接收连接请求。
     * 第二个参数 16 是 backlog ——「已建立连接但还没被 accept」的排队上限，demo 里 16 足够。
     * 返回 0 成功、非 0 失败。 */
    if (listen(fd, 16) != 0) { perror("listen"); exit(1); }
    return fd;
}

/* 查找参数（只查不建），找到返回指针，否则返回 NULL */
static struct param_entry *find_param(const char *name)
{
    for (uint32_t i = 0; i < g_area->count; i++) {
        if (strcmp(g_area->params[i].name, name) == 0)
            return &g_area->params[i];
    }
    return NULL;
}

/* 新增参数（只建不查），满了返回 NULL */
static struct param_entry *add_param(const char *name)
{
    if (g_area->count >= MAX_PARAMS) return NULL;
    struct param_entry *e = &g_area->params[g_area->count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, MAX_NAME - 1);
    return e;
}

/* 找到或新建（SET 用：写参数时不存在就建） */
static struct param_entry *find_or_add(const char *name)
{
    struct param_entry *e = find_param(name);
    return (e != NULL) ? e : add_param(name);
}

/* 参数变化时，通知所有匹配的 watcher（一对多 fan-out 的核心） */
static void notify_watchers(const char *name, const char *value)
{
    char msg[MAX_NAME + MAX_VALUE + 32];
    snprintf(msg, sizeof(msg), "%s %s=%s\n", MSG_NOTIFY, name, value);
    for (int i = 0; i < g_watcher_count; i++) {
        struct watcher *w = &g_watchers[i];
        if (strcmp(w->name, name) == 0 &&
            (w->value[0] == '\0' || strcmp(w->value, value) == 0)) {
            write(w->fd, msg, strlen(msg));   /* 每个 watcher 各写一份 */
        }
    }
}

/* 处理一条客户端消息 */
static void handle_message(int fd, const char *line)
{
    char cmd[16], name[MAX_NAME], value[MAX_VALUE];
    int n = sscanf(line, "%15s %63s %127[^\n]", cmd, name, value);
    if (n < 2) return;

    if (strcmp(cmd, MSG_SET) == 0) {
        if (n < 3) return;
        struct param_entry *e = find_or_add(name);
        if (e == NULL) { write(fd, MSG_ERR "\n", 4); return; }
        strncpy(e->value, value, MAX_VALUE - 1);
        e->value[MAX_VALUE - 1] = '\0';
         
        write(fd, MSG_OK "\n", strlen(MSG_OK "\n"));
        notify_watchers(name, value);                        /* 通知监听者 */
        printf("SET %s = %s\n", name, value);
    } else if (strcmp(cmd, MSG_WATCH) == 0) {
        /* 去重：同一个 fd 对同一个名字重复 watch，只更新 value，不重复加 */
        for (int i = 0; i < g_watcher_count; i++) {
            if (g_watchers[i].fd == fd && strcmp(g_watchers[i].name, name) == 0) {
                g_watchers[i].value[0] = '\0';
                if (n >= 3) strncpy(g_watchers[i].value, value, MAX_VALUE - 1);
                printf("WATCH(update) %s (fd=%d)\n", name, fd);
                return;
            }
        }
        if (g_watcher_count >= MAX_PARAMS) return;
        struct watcher *w = &g_watchers[g_watcher_count++];
        w->fd = fd;
        strncpy(w->name, name, MAX_NAME - 1);
        w->value[0] = '\0';
        if (n >= 3) strncpy(w->value, value, MAX_VALUE - 1);
        printf("WATCH %s (fd=%d)\n", name, fd);
        /* 如果当前值已经满足条件，立即通知一次（find 只查不建，避免 watch 副作用地创建参数） */
        struct param_entry *e = find_param(name);
        if (e != NULL && e->value[0] != '\0' &&
            (w->value[0] == '\0' || strcmp(w->value, e->value) == 0)) {
            notify_watchers(name, e->value);
        }
    }
}

/* 客户端断开时，清掉它注册的所有 watcher */
static void remove_watchers(int fd)
{
    for (int i = 0; i < g_watcher_count; ) {
        if (g_watchers[i].fd == fd) {
            g_watchers[i] = g_watchers[--g_watcher_count];
        } else {
            i++;
        }
    }
}

int main(void)
{
    signal(SIGPIPE, SIG_IGN);   /* 客户端断开时 write 会触发 SIGPIPE，忽略 */

    init_shared_memory();
    int listen_fd = init_listen_socket();
    printf("my_param_server listening on %s\n", SOCKET_PATH);

    int epfd = epoll_create1(0);
    struct epoll_event ev, events[64];
    ev.events = EPOLLIN; 
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    for (;;) {
        int nfds = epoll_wait(epfd, events, 64, -1);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            if (fd == listen_fd) {
                /* 新客户端连进来：accept 从积压队列取出一个连接，返回专属通道 cfd
                 *（listen_fd 继续监听、不受影响）。然后把 cfd 加进 epoll 监控，
                 * 之后它一发消息（EPOLLIN 可读）epoll_wait 就会报出来。 */
                int cfd = accept(listen_fd, NULL, NULL);
                if (cfd < 0) continue;
                ev.events = EPOLLIN; 
                ev.data.fd = cfd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
            } else {
                char buf[512];
                ssize_t r = read(fd, buf, sizeof(buf) - 1);
                if (r <= 0) {   /* 断开 */
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    remove_watchers(fd);
                    continue;
                }
                buf[r] = '\0';
                handle_message(fd, buf);
            }
        }
    }
    return 0;
}
