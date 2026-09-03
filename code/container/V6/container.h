#ifndef CONTAINER_H
#define CONTAINER_H

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/socket.h>
#include <linux/sched.h>      /* CLONE_NEW* */
#include <linux/netlink.h>    /* NETLINK_ROUTE */
#include <linux/rtnetlink.h>  /* RTM_NEWLINK, RTM_NEWADDR, RTM_NEWROUTE */
#include <linux/if.h>        /* IFF_UP */
#include <linux/if_link.h>    /* IFLA_*, VETH_INFO_PEER */
#include <arpa/inet.h>        /* inet_pton */
#include <stdarg.h>           /* va_list */
#include <sys/prctl.h>        /* prctl */
#include <linux/capability.h> /* CAP_* */
#include <time.h>             /* time_t */
#include <dirent.h>           /* opendir, readdir */
#include <grp.h>              /* setgroups */

/* ================================================================
 * 常量
 * ================================================================ */

#define STACK_SIZE      (1024 * 1024 * 2)   /* 子进程栈 2MB */
#define MAX_CMD_LEN     512
#define MAX_PATH_LEN    512
#define MAX_NAME_LEN    64
#define MAX_ID_LEN      32
#define CG_BUF_LEN      (MAX_PATH_LEN + 64)   /* cgroup 路径 + 后缀余量 */
/* ================================================================
 * 容器运行时配置
 * ================================================================ */

typedef struct {
    char  rootfs[MAX_PATH_LEN];   /* 容器根文件系统路径 */
    char  hostname[64];           /* 容器主机名 */
    char  cmd[MAX_CMD_LEN];       /* 容器启动命令 */
    char  memory_limit[32];       /* 内存限制，如 "128M", "256M" */
    char  cpu_limit[32];          /* CPU 限制，如 "0.5", "1.0" */
    char  workdir[MAX_PATH_LEN];  /* 工作目录，为空则用根目录 */
    char  env_extra[4096];        /* 额外环境变量，\n 分隔 */
    char  container_ip[16];       /* 容器 IP（空则自动分配） */
    char  port_maps[4][32];       /* 端口映射，如 "8080:80" */
    int   port_map_count;         /* 端口映射条数 */
    int   net_isolate;            /* 是否隔离网络 */
    int   no_userns;              /* 跳过 CLONE_NEWUSER（嵌入式设备等） */
    int   verbose;                /* 详细输出 */
    char  rootfs_mode[16];        /* rootfs 模式: rw (默认) | ro */
    char  container_name[MAX_NAME_LEN];  /* V4: 容器名称 */
    int   detach;                 /* V4: 后台运行 */
    char  image_name[MAX_NAME_LEN];      /* V6: 镜像名称，为空则用 rootfs 目录 */
} container_config;

/* ================================================================
 * V4 容器状态
 * ================================================================ */

typedef struct {
    char  id[MAX_ID_LEN];         /* 容器 ID */
    char  name[MAX_NAME_LEN];     /* 容器名称 */
    pid_t pid;                    /* 宿主机视角的 init PID，-1=已停止 */
    char  rootfs[MAX_PATH_LEN];   /* rootfs 路径 */
    char  cmd[MAX_CMD_LEN];       /* 启动命令 */
    time_t created_at;            /* 创建时间 */
} container_info;

/* ================================================================
 * cgroup 操作
 * ================================================================ */

/* 返回值：成功返回 cgroup 目录 fd，失败返回 -1 */
int  cgroup_setup(const container_config *cfg);
int  cgroup_add_pid(int cgroup_fd, pid_t pid);
void cgroup_cleanup(const container_config *cfg);

/* ================================================================
 * 容器子进程环境
 * ================================================================ */

/* 在子进程中执行：设置文件系统、主机名、执行用户命令 */
int  child_setup(void *arg) __attribute__((noreturn));

/* 在指定路径下挂载伪文件系统（prefix="" 等价于旧 mount_pseudo_fs） */
int  mount_pseudo_fs_at(const char *rootfs);
int  mount_pseudo_fs(void);

/* pivot_root 标准流程 change_root*/
int  do_pivot_root(const char *new_root);

/* ================================================================
 * 工具函数
 * ================================================================ */

void die(const char *msg);
int  write_file(const char *path, const char *value);
void print_usage(const char *prog);

/* ================================================================
 * V2 网络栈
 * ================================================================ */

/* 宿主机侧：创建 bridge + veth pair + NAT（clone 之前调用） */
int  network_host_setup(void);

/* 父进程：把容器侧 veth 移入子进程 netns（clone 之后调用） */
int  network_move_veth(pid_t child_pid);

/* 容器侧：重命名 veth → eth0，配 IP + 路由 + 拉起接口 */
int  network_child_setup(void);

/* 清理：删 iptables 规则 + 宿主机侧 veth */
void network_cleanup(void);

/* ================================================================
 * V3 安全加固
 * ================================================================ */

/* 父进程侧：写 /proc/<pid>/setgroups + uid_map + gid_map */
int  setup_user_ns(pid_t child_pid);

/* 子进程侧：裁剪危险 capabilities */
int  drop_capabilities(void);

/* ================================================================
 * V4 状态管理
 * ================================================================ */

/* 创建状态目录 (/var/lib/mycontainer/containers/) */
int  state_init(void);

/* 保存容器状态 */
int  state_save(const container_info *ci);

/* 读取容器状态 */
int  state_load(const char *id, container_info *ci);

/* 删除容器状态 */
int  state_delete(const char *id);

/* 列出所有容器，返回数量 */
int  state_list(container_info *list, int max);

/* 更新容器 PID */
int  state_update_pid(const char *id, pid_t pid);

/* ================================================================
 * V4 生命周期命令
 * ================================================================ */

int  cmd_ps(int verbose);
int  cmd_stop(const char *id);
int  cmd_exec(const char *id, const char *command);
int  cmd_rm(const char *id);

/* ================================================================
 * V6 OverlayFS + 本地镜像管理
 * ================================================================ */

/* 挂载/卸载 overlay 文件系统 */
int  overlay_mount(const char *lowerdir, const char *upperdir,
                   const char *workdir, const char *merged);
int  overlay_umount(const char *merged);

/* 镜像管理命令 */
int  cmd_import(const char *tarball, const char *name);
int  cmd_images(void);
int  cmd_rmi(const char *name);
int  cmd_commit(const char *id, const char *name);

/* ================================================================
 * V6 路径常量 & 辅助
 * ================================================================ */

#define IMAGES_DIR  "/var/lib/mycontainer/images"

/* 构建容器 overlay 各层路径 */
void container_overlay_paths(const char *id,
                             char *lower, char *upper,
                             char *work, char *merged, size_t size);

#endif /* CONTAINER_H */
