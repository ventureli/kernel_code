#ifndef MY_PARAM_COMMON_H
#define MY_PARAM_COMMON_H

#include <stdint.h>

/* 共享内存 / 协议 的公共定义，服务端和客户端共用 */

#define MAX_PARAMS   128
#define MAX_NAME     64
#define MAX_VALUE    128

/* Unix 域套接字路径（demo 放在 /tmp，不用管权限） */
#define SOCKET_PATH  "/tmp/my_param.sock"
/* 共享内存名（POSIX shm_open 的名字，不带斜杠） */
#define SHM_NAME     "/my_param_shm"
/* 共享内存魔数，用来判断服务端有没有初始化过 */
#define SHM_MAGIC    0x50415241   /* "PARA" */

/* 一个参数：名字 + 值 + 版本号 */
struct param_entry {
    char name[MAX_NAME];
    char value[MAX_VALUE];
    uint32_t commit_id;          /* 参数版本号，每次 set 自增（对应文章里的 commitId） */
};

/* 整块共享内存的布局：一个简单数组（demo 用数组，真实实现是 Trie） */
struct param_area {
    uint32_t magic;              /* 魔数 */
    uint64_t global_commit_id;   /* 全局版本号，每次 set 自增 */
    uint32_t count;              /* 当前参数个数 */
    struct param_entry params[MAX_PARAMS];
};

/* 客户端 → 服务端 消息 */
#define MSG_SET    "SET"
#define MSG_WATCH  "WATCH"

/* 服务端 → 客户端 消息 */
#define MSG_OK     "OK"
#define MSG_ERR    "ERR"
#define MSG_NOTIFY "NOTIFY"

#endif /* MY_PARAM_COMMON_H */
