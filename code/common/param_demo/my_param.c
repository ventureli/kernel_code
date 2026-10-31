/*
 * my_param —— 系统参数服务极简 demo（客户端命令）
 *
 * 用法：
 *   my_param get   name              -- 读参数（直接读共享内存，零 socket）
 *   my_param set   name value        -- 写参数（走 socket 发给服务端）
 *   my_param watch name [value] [t]  -- 监听参数（注册后阻塞等通知，可选超时秒）
 *
 * 对应 OpenHarmony 的 param 命令，读/写/监听三条路：
 *   get   → mmap 共享内存直接读
 *   set   → 连 socket 发 MSG_SET
 *   watch → 连 socket 发 MSG_WATCH，poll 阻塞等 MSG_NOTIFY
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "my_param_common.h"

/* 打开（映射）共享内存，只读参数用 */
static struct param_area *open_shared_memory(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd < 0) { perror("shm_open"); exit(1); }
    struct param_area *area = mmap(NULL, sizeof(struct param_area),
                                   PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (area == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);
    if (area->magic != SHM_MAGIC) {
        fprintf(stderr, "shared memory 没初始化，请先启动 my_param_server\n");
        exit(1);
    }
    return area;
}

/* 连到服务端 */
static int connect_server(void)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); exit(1); }
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("connect");
        fprintf(stderr, "连不上 %s，my_param_server 跑了吗？\n", SOCKET_PATH);
        exit(1);
    }
    return fd;
}

/* get：直接读共享内存，不连 socket（零 IPC） */
static int cmd_get(const char *name)
{
    struct param_area *area = open_shared_memory();
    for (uint32_t i = 0; i < area->count; i++) {
        if (strcmp(area->params[i].name, name) == 0) {
            printf("%s\n", area->params[i].value);
            return 0;
        }
    }
    fprintf(stderr, "parameter %s not found\n", name);
    return 1;
}

/* set：走 socket 发给服务端（单一写者） */
static int cmd_set(const char *name, const char *value)
{
    int fd = connect_server();
    char line[256];
    snprintf(line, sizeof(line), "%s %s %s\n", MSG_SET, name, value);
    write(fd, line, strlen(line));

    char resp[32];
    int r = read(fd, resp, sizeof(resp) - 1);
    close(fd);
    resp[r > 0 ? r : 0] = '\0';

    if (strncmp(resp, MSG_OK, strlen(MSG_OK)) == 0) {
        printf("Set parameter %s %s success\n", name, value);
        return 0;
    }
    printf("Set parameter %s %s fail\n", name, value);
    return 1;
}

/* watch：连 socket 注册，poll 阻塞等通知（可选超时） */
static int cmd_watch(const char *name, const char *value, int timeout)
{
    int fd = connect_server();
    char line[256];
    if (value && value[0])
        snprintf(line, sizeof(line), "%s %s %s\n", MSG_WATCH, name, value);
    else
        snprintf(line, sizeof(line), "%s %s\n", MSG_WATCH, name);
    write(fd, line, strlen(line));

    struct pollfd pfd = { .fd = fd, .events = POLLIN };
    int ret = poll(&pfd, 1, timeout * 1000);
    if (ret <= 0) {
        printf("Wait parameter %s timeout\n", name);
        close(fd);
        return 1;
    }

    char buf[512];
    int r = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    buf[r > 0 ? r : 0] = '\0';

    if (strncmp(buf, MSG_NOTIFY, strlen(MSG_NOTIFY)) == 0) {
        printf("Wait parameter %s success\n", name);
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("usage:\n"
               "  my_param get   name\n"
               "  my_param set   name value\n"
               "  my_param watch name [value] [timeout]\n");
        return 1;
    }

    const char *cmd = argv[1];
    if (strcmp(cmd, "get") == 0) {
        return cmd_get(argv[2]);
    }
    if (strcmp(cmd, "set") == 0) {
        if (argc < 4) { printf("set 需要 value\n"); return 1; }
        return cmd_set(argv[2], argv[3]);
    }
    if (strcmp(cmd, "watch") == 0) {
        const char *value = (argc >= 4) ? argv[3] : NULL;
        int timeout = (argc >= 5) ? atoi(argv[4]) : 30;
        return cmd_watch(argv[2], value, timeout);
    }

    printf("unknown cmd %s\n", cmd);
    return 1;
}
