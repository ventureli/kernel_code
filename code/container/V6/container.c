/*
 * mycontainer — 用 C 从零手写容器运行时
 *
 * 核心依赖：clone(2) + pivot_root(2) + cgroup + mount
 * 这是对 Linux 内核三大隔离机制的直接封装，没有中间层。
 *
 * 作者：Ventrueli | 公众号：从内核到智能
 *
 * 编译：make
 * 用法：sudo ./mycontainer run -r ./alpine-rootfs [-m 256M] [-c 0.5] [--cmd /bin/sh]
 *
 * ================================================================
 * 架构概述
 * ================================================================
 *
 *   main()
 *     ├─ parse_args()           解析命令行参数
 *     ├─ cgroup_setup()         创建 cgroup + 写限制（clone 之前）
 *     ├─ network_host_setup()   创建 bridge + veth pair + NAT
 *     ├─ pipe()                 创建同步管道（User NS 用）
 *     ├─ clone()                创建子进程，带 7 个 namespace 标志
 *     │   ├─ [父] close(sync_pipe[0])             关闭读端
 *     │   │        setup_user_ns()                写 uid_map/gid_map
 *     │   │        write(sync_pipe[1])             通知子进程继续
 *     │   │        network_move_veth()            把 veth-c0 移入容器 netns
 *     │   │        cgroup_add_pid()               把子进程 PID 写入 cgroup
 *     │   │        waitpid()                      等子进程结束
 *     │   │        network_cleanup()              清理 iptables + veth
 *     │   │
 *     │   └─ [子] read(sync_fd)                   等待 uid_map 就绪
 *     │            drop_capabilities()            V3: 裁剪危险能力
 *     │            do_pivot_root()                切换根文件系统
 *     │            mount_pseudo_fs()              挂载 /proc /sys /dev /tmp
 *     │            sethostname()                   设置容器主机名
 *     │            network_child_setup()           rename veth→eth0 + IP + 路由
 *     │            mount(MS_REMOUNT|MS_RDONLY)    V3: rootfs 只读
 *     │            execve()                       执行用户命令
 *     └─ return 0
 */

#include "container.h"

/* ================================================================
 * 全局配置（clone 回调只接受 void*，用全局变量传递配置）
 * ================================================================ */
static container_config g_cfg;

/* ================================================================
 * 工具函数
 * ================================================================ */

void die(const char *msg) {
    fprintf(stderr, "[ERROR] %s: %s\n", msg, strerror(errno));
    exit(1);
}

int write_file(const char *path, const char *value) {
    /* /proc 和 /sys 是内核伪文件，不可以用 O_CREAT/O_TRUNC */
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t len = strlen(value);
    ssize_t n   = write(fd, value, len);
    close(fd);
    return (n == len) ? 0 : -1;
}

/* 写入常规文件（覆盖模式，文件不存在则创建） */
static int write_regular(const char *path, const char *value) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) return -1;
    ssize_t len = strlen(value);
    ssize_t n   = write(fd, value, len);
    close(fd);
    return (n == len) ? 0 : -1;
}

/* ================================================================
 * V2 Netlink 工具层（RTNETLINK 操作内核接口）
 * ================================================================ */

#define NL_BUF 4096

/* 嵌套属性追踪 */
struct nl_nest {
    struct nlmsghdr *nlh;
    int offset;  /* 外层 rtattr 在 buf 中的起始偏移 */
};

/* 发送 netlink 消息并等待 ack，成功返回 0，失败返回 -1 */
static int nl_talk(struct nlmsghdr *nlh) {
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd < 0) return -1;

    struct sockaddr_nl local = { .nl_family = AF_NETLINK };
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        close(fd); return -1;
    }

    nlh->nlmsg_seq = 1;
    struct sockaddr_nl dst = { .nl_family = AF_NETLINK };
    if (sendto(fd, nlh, nlh->nlmsg_len, 0,
               (struct sockaddr *)&dst, sizeof(dst)) < 0) {
        close(fd); return -1;
    }

    char buf[NL_BUF];
    int ret = 0;
    while (1) {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) { ret = -1; break; }
        int remaining = (int)n;
        struct nlmsghdr *r = (struct nlmsghdr *)buf;
        for (; NLMSG_OK(r, remaining); r = NLMSG_NEXT(r, remaining)) {
            if (r->nlmsg_type == NLMSG_ERROR) {
                struct nlmsgerr *e = NLMSG_DATA(r);
                if (e->error != 0) { errno = -e->error; ret = -1; }
                goto out;
            }
            if (r->nlmsg_type == NLMSG_DONE) goto out;
        }
    }
out:
    close(fd);
    return ret;
}

/* 在 nlh 末尾追加 rtattr，返回数据区指针 */
static void *nl_put_attr(struct nlmsghdr *nlh, unsigned short type,
                         unsigned short len) {
    struct rtattr *rta = (struct rtattr *)((char *)nlh + NLMSG_ALIGN(nlh->nlmsg_len));
    rta->rta_type = type;
    rta->rta_len  = RTA_LENGTH(len);
    nlh->nlmsg_len = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(rta->rta_len);
    return RTA_DATA(rta);
}

/* 追加字符串属性 */
static void nl_put_str(struct nlmsghdr *nlh, unsigned short type, const char *str) {
    int slen = (int)strlen(str) + 1;
    memcpy(nl_put_attr(nlh, type, (unsigned short)slen), str, (size_t)slen);
}

/* 追加 u32 属性 */
static void nl_put_u32(struct nlmsghdr *nlh, unsigned short type, uint32_t val) {
    *(uint32_t *)nl_put_attr(nlh, type, sizeof(uint32_t)) = val;
}

/* 开始嵌套属性：预留外层 rtattr 头 */
static void nl_nest_start(struct nl_nest *nest, struct nlmsghdr *nlh) {
    nest->nlh    = nlh;
    nest->offset = NLMSG_ALIGN(nlh->nlmsg_len);
    nlh->nlmsg_len = nest->offset + RTA_LENGTH(0);  /* 只占位，后面补长度 */
}

/* 结束嵌套属性：回填外层 rtattr 的 type + 长度 */
static void nl_nest_end(struct nl_nest *nest, unsigned short type) {
    struct rtattr *rta = (struct rtattr *)((char *)nest->nlh + nest->offset);
    rta->rta_type = type;
    rta->rta_len  = nest->nlh->nlmsg_len - nest->offset;
}

/* 通过 /sys/class/net/<name>/ifindex 获取接口索引 */
static int get_ifindex(const char *name) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/ifindex", name);
    char buf[16];
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return atoi(buf);
}

void print_usage(const char *prog) {
    fprintf(stderr,
        "\n  mycontainer — 从零手写的 Linux 容器运行时\n"
        "\n"
        "  用法:\n"
        "    sudo %s run   -r <rootfs> [选项]\n"
        "    sudo %s run   --image <name> [选项]\n"
        "    sudo %s ps    [-v]\n"
        "    sudo %s stop  <id>\n"
        "    sudo %s exec  <id> <command>\n"
        "    sudo %s rm    <id>\n"
        "    sudo %s import <tarball> <name>\n"
        "    sudo %s images\n"
        "    sudo %s rmi   <name>\n"
        "    sudo %s commit <id> <name>\n"
        "\n"
        "  run 必选 (二选一):\n"
        "    -r, --rootfs <path>   容器的根文件系统目录\n"
        "        --image <name>     从镜像启动 (overlay 模式)\n"
        "\n"
        "  run 可选:\n"
        "    -m, --memory <limit>  内存上限 (如 128M, 256M, 1G)\n"
        "    -c, --cpu <limit>     CPU 上限 (如 0.5 = 半核, 2 = 两核)\n"
        "    -e, --env VAR=VALUE  注入环境变量 (可多次使用)\n"
        "    -w, --workdir <path> 容器内工作目录\n"
        "    -p, --port host:cont 端口映射 (如 -p 8080:80, 可多次)\n"
        "        --ip <addr>      手动指定容器 IP (如 --ip 10.0.0.5)\n"
        "        --net-isolate     隔离网络 (bridge+NAT, 默认共享宿主机网络)\n"
        "        --userns           启用 user namespace 隔离 (默认关闭)\n"
        "        --hostname <name> 容器主机名 (默认: container)\n"
        "        --cmd <command>   启动命令 (默认: /bin/sh)\n"
        "        --rootfs-mode <rw|ro>  rootfs 模式 (默认: rw)\n"
        "        --name <name>    容器名称 (默认自动生成)\n"
        "    -d, --detach          后台运行\n"
        "    -v, --verbose         详细输出\n"
        "    -h, --help            显示帮助\n"
        "\n"
        "  示例:\n"
        "    sudo %s run -r ./alpine-rootfs\n"
        "    sudo %s import ./alpine-rootfs-x86_64.tar.gz alpine\n"
        "    sudo %s run --image alpine --name my\n"
        "    sudo %s images\n"
        "    sudo %s commit my my-custom\n"
        "    sudo %s ps\n"
        "    sudo %s exec my \"ps aux\"\n"
        "    sudo %s stop my\n"
        "    sudo %s rm my\n"
        "    sudo %s rmi alpine\n"
        "\n",
        prog, prog, prog, prog, prog, prog, prog,
        prog, prog, prog, prog, prog, prog, prog,
        prog, prog, prog, prog, prog, prog);
}

/* ================================================================
 * CLI 参数解析
 * ================================================================ */

static void parse_args(int argc, char *argv[]) {
    /* 默认值 */
    memset(&g_cfg, 0, sizeof(g_cfg));
    strcpy(g_cfg.hostname, "container");
    strcpy(g_cfg.cmd,      "/bin/sh");
    strcpy(g_cfg.rootfs_mode, "rw");  /* 默认可写 */
    g_cfg.net_isolate = 0;   /* 默认共享网络 */
    g_cfg.no_userns   = 1;   /* 默认不隔离 user NS */
    g_cfg.verbose     = 0;
    g_cfg.detach      = 0;

    /* 解析从 argv[2] 开始（argv[1] 是 "run"） */
    int i = 2;
    while (i < argc) {
        if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rootfs") == 0) && i+1 < argc) {
            strncpy(g_cfg.rootfs, argv[++i], MAX_PATH_LEN - 1);
        } else if ((strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--memory") == 0) && i+1 < argc) {
            strncpy(g_cfg.memory_limit, argv[++i], sizeof(g_cfg.memory_limit) - 1);
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cpu") == 0) && i+1 < argc) {
            strncpy(g_cfg.cpu_limit, argv[++i], sizeof(g_cfg.cpu_limit) - 1);
        } else if ((strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--env") == 0) && i+1 < argc) {
            size_t len = strlen(g_cfg.env_extra);
            int n = snprintf(g_cfg.env_extra + len, sizeof(g_cfg.env_extra) - len,
                             "%s%s\n", (len > 0 ? "\n" : ""), argv[++i]);
            if (n < 0 || (size_t)n >= sizeof(g_cfg.env_extra) - len) {
                fprintf(stderr, "[WARN] 环境变量过多，已截断\n");
            }
        } else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--workdir") == 0) && i+1 < argc) {
            strncpy(g_cfg.workdir, argv[++i], MAX_PATH_LEN - 1);
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--net-isolate") == 0) {
            g_cfg.net_isolate = 1;
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) && i+1 < argc) {
            if (g_cfg.port_map_count < 4)
                strncpy(g_cfg.port_maps[g_cfg.port_map_count++], argv[++i],
                        sizeof(g_cfg.port_maps[0]) - 1);
            else
                fprintf(stderr, "[WARN] 端口映射已达上限 (4)\n");
        } else if (strcmp(argv[i], "--ip") == 0 && i+1 < argc) {
            strncpy(g_cfg.container_ip, argv[++i], sizeof(g_cfg.container_ip) - 1);
        } else if (strcmp(argv[i], "--hostname") == 0 && i+1 < argc) {
            strncpy(g_cfg.hostname, argv[++i], sizeof(g_cfg.hostname) - 1);
        } else if (strcmp(argv[i], "--cmd") == 0 && i+1 < argc) {
            strncpy(g_cfg.cmd, argv[++i], MAX_CMD_LEN - 1);
        } else if ((strcmp(argv[i], "--rootfs-mode") == 0) && i+1 < argc) {
            strncpy(g_cfg.rootfs_mode, argv[++i], sizeof(g_cfg.rootfs_mode) - 1);
        } else if ((strcmp(argv[i], "--name") == 0) && i+1 < argc) {
            strncpy(g_cfg.container_name, argv[++i], MAX_NAME_LEN - 1);
        } else if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            strncpy(g_cfg.image_name, argv[++i], MAX_NAME_LEN - 1);
        } else if (strcmp(argv[i], "--userns") == 0) {
            g_cfg.no_userns = 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--detach") == 0) {
            g_cfg.detach = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            g_cfg.verbose = 1;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            fprintf(stderr, "未知参数: %s\n", argv[i]);
            print_usage(argv[0]);
            exit(1);
        }
        i++;
    }

    /* rootfs 或 image 必须二选一 */
    if (g_cfg.rootfs[0] == '\0' && g_cfg.image_name[0] == '\0') {
        fprintf(stderr, "[ERROR] 必须指定 --rootfs 或 --image\n");
        print_usage(argv[0]);
        exit(1);
    }
    /* 不能同时指定 */
    if (g_cfg.rootfs[0] != '\0' && g_cfg.image_name[0] != '\0') {
        fprintf(stderr, "[ERROR] --rootfs 和 --image 不能同时指定\n");
        print_usage(argv[0]);
        exit(1);
    }

    /* --image 模式: 检查镜像是否存在 */
    if (g_cfg.image_name[0] != '\0') {
        char img_rootfs[MAX_PATH_LEN];
        struct stat img_st;
        snprintf(img_rootfs, sizeof(img_rootfs),
                 IMAGES_DIR "/%s/rootfs", g_cfg.image_name);
        if (stat(img_rootfs, &img_st) != 0 || !S_ISDIR(img_st.st_mode)) {
            fprintf(stderr, "[ERROR] 镜像 '%s' 不存在，请先 import\n",
                    g_cfg.image_name);
            exit(1);
        }
        /* 用镜像 rootfs 作为 lowerdir 参考路径 */
        strncpy(g_cfg.rootfs, img_rootfs, MAX_PATH_LEN - 1);
    }

    /* rootfs 必须存在（-r 模式也走到这里） */
    if (g_cfg.rootfs[0] == '\0') {
        fprintf(stderr, "[ERROR] 必须指定 --rootfs\n");
        print_usage(argv[0]);
        exit(1);
    }

    /* rootfs 必须存在 */
    struct stat st;
    if (stat(g_cfg.rootfs, &st) != 0 || !S_ISDIR(st.st_mode)) {
        die("rootfs 路径无效或不是目录");
    }
}

/* ================================================================
 * cgroup v2（统一层级）—— 优先使用
 * ================================================================ */

static int cgroup_v2_setup(const container_config *cfg) {
    const char *cg_root = "/sys/fs/cgroup";
    const char *cg_name = "mycontainer";
    char path[MAX_PATH_LEN];

    /* 1. 确保 cgroup v2 已挂载 */
    if (access(cg_root, F_OK) != 0) {
        if (mount("cgroup2", cg_root, "cgroup2", 0, NULL) != 0) {
            return -1;
        }
    }

    /* 2. 创建子 cgroup */
    snprintf(path, sizeof(path), "%s/%s", cg_root, cg_name);

    /* 如果上次没清理，先删再建 */
    if (access(path, F_OK) == 0) rmdir(path);

    if (mkdir(path, 0755) != 0) {
        return -1;
    }

    /* 3. 启用控制器（先读有哪些可用控制器，然后写入） */
    char controllers[256] = {0};
    char ctl_path[CG_BUF_LEN];
    snprintf(ctl_path, sizeof(ctl_path), "%s/cgroup.controllers", cg_root);

    int fd = open(ctl_path, O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, controllers, sizeof(controllers) - 1);
        close(fd);
        if (n > 0) {
            controllers[n] = '\0';
            /* 去掉尾随换行 */
            char *nl = strchr(controllers, '\n');
            if (nl) *nl = '\0';

            snprintf(ctl_path, sizeof(ctl_path), "%s/cgroup.subtree_control", path);
            /* 逐个写入控制器（用 + 前缀表示启用） */
            char buf[256];
            char *saveptr;
            char *token = strtok_r(controllers, " ", &saveptr);
            while (token) {
                snprintf(buf, sizeof(buf), "+%s", token);
                write_file(ctl_path, buf);
                token = strtok_r(NULL, " ", &saveptr);
            }
        }
    }

    /* 4. 设置内存限制 */
    if (cfg->memory_limit[0] != '\0') {
        snprintf(path, sizeof(path), "%s/%s/memory.max", cg_root, cg_name);
        if (write_file(path, cfg->memory_limit) != 0) {
            fprintf(stderr, "[WARN] 设置内存限制失败: %s\n", strerror(errno));
        } else if (cfg->verbose) {
            printf("[INFO] 内存限制: %s\n", cfg->memory_limit);
        }
    }

    /* 5. 设置 CPU 限制 */
    if (cfg->cpu_limit[0] != '\0') {
        snprintf(path, sizeof(path), "%s/%s/cpu.max", cg_root, cg_name);
        /* cpu.max 格式: "$MAX $PERIOD"，默认 period=100000 (100ms)
         * 如 "50000 100000" = 半核, "200000 100000" = 两核 */
        char cpu_buf[64];
        double cpu_val = atof(cfg->cpu_limit);
        int quota = (int)(cpu_val * 100000);
        snprintf(cpu_buf, sizeof(cpu_buf), "%d 100000", quota);
        if (write_file(path, cpu_buf) != 0) {
            fprintf(stderr, "[WARN] 设置 CPU 限制失败: %s\n", strerror(errno));
        } else if (cfg->verbose) {
            printf("[INFO] CPU 限制: %s (%d/100000)\n", cfg->cpu_limit, quota);
        }
    }

    /* 返回子 cgroup 目录的文件描述符（后面写 PID 用） */
    snprintf(path, sizeof(path), "%s/%s", cg_root, cg_name);
    return open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}

/* ================================================================
 * cgroup v1（传统层级）—— 回退方案
 * ================================================================ */

static int cgroup_v1_setup(const container_config *cfg) {
    const char *cg_root  = "/sys/fs/cgroup";
    const char *cg_name  = "mycontainer";
    char path[MAX_PATH_LEN];

    /* 1. 确保 tmpfs 挂载到 /sys/fs/cgroup */
    if (access(cg_root, F_OK) != 0) {
        if (mount("tmpfs", cg_root, "tmpfs", 0, NULL) != 0) {
            return -1;
        }
    }

    /* 2. 创建并挂载 memory cgroup */
    snprintf(path, sizeof(path), "%s/memory", cg_root);
    if (access(path, F_OK) != 0) mkdir(path, 0755);
    if (mount("cgroup", path, "cgroup", 0, "memory") != 0) {
        /* 可能已经挂载了 */
    }

    snprintf(path, sizeof(path), "%s/memory/%s", cg_root, cg_name);
    if (access(path, F_OK) == 0) rmdir(path);
    if (mkdir(path, 0755) != 0) {
        return -1;
    }

    /* 设置内存限制 */
    if (cfg->memory_limit[0] != '\0') {
        snprintf(path, sizeof(path),
                 "%s/memory/%s/memory.limit_in_bytes", cg_root, cg_name);
        if (write_file(path, cfg->memory_limit) != 0) {
            fprintf(stderr, "[WARN] cgroup v1 内存限制设置失败\n");
        } else if (cfg->verbose) {
            printf("[INFO] cgroup v1 内存限制: %s\n", cfg->memory_limit);
        }
    }

    /* 3. 创建并挂载 cpu cgroup */
    snprintf(path, sizeof(path), "%s/cpu", cg_root);
    if (access(path, F_OK) != 0) mkdir(path, 0755);
    mount("cgroup", path, "cgroup", 0, "cpu");

    snprintf(path, sizeof(path), "%s/cpu/%s", cg_root, cg_name);
    if (access(path, F_OK) == 0) rmdir(path);
    if (mkdir(path, 0755) == 0 && cfg->cpu_limit[0] != '\0') {
        /* cgroup v1 cpu 限制：cpu.shares 是相对权重，cpu.cfs_quota_us 是绝对配额 */
        double cpu_val = atof(cfg->cpu_limit);
        int quota = (int)(cpu_val * 100000);
        char cpu_buf[64];
        snprintf(cpu_buf, sizeof(cpu_buf), "%d", quota);

        snprintf(path, sizeof(path),
                 "%s/cpu/%s/cpu.cfs_quota_us", cg_root, cg_name);
        write_file(path, cpu_buf);

        snprintf(path, sizeof(path),
                 "%s/cpu/%s/cpu.cfs_period_us", cg_root, cg_name);
        write_file(path, "100000");
    }

    /* 返回 memory cgroup 的 fd */
    snprintf(path, sizeof(path), "%s/memory/%s", cg_root, cg_name);
    return open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}

/* ---- cgroup 公共接口 ---- */

int cgroup_setup(const container_config *cfg) {
    if (cfg->memory_limit[0] == '\0' && cfg->cpu_limit[0] == '\0') {
        /* 没有设置任何限制，不需要 cgroup */
        if (g_cfg.verbose) printf("[INFO] 未设置资源限制，跳过 cgroup\n");
        return -1;
    }

    /* 优先尝试 cgroup v2，失败则回退 v1 */
    int fd = cgroup_v2_setup(cfg);
    if (fd >= 0) {
        if (g_cfg.verbose) printf("[INFO] 使用 cgroup v2 (统一层级)\n");
        return fd;
    }

    fd = cgroup_v1_setup(cfg);
    if (fd >= 0) {
        if (g_cfg.verbose) printf("[INFO] 使用 cgroup v1 (传统层级)\n");
        return fd;
    }

    fprintf(stderr, "[WARN] cgroup 初始化失败，容器将无资源限制\n");
    return -1;
}

int cgroup_add_pid(int cgroup_fd, pid_t pid) {
    if (cgroup_fd < 0) return -1;
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d\n", pid);

    /* cgroup v2: cgroup.procs  |  cgroup v1: tasks */
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), "/proc/self/fd/%d/cgroup.procs", cgroup_fd);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        /* 回退到 v1 的 tasks 文件 */
        snprintf(path, sizeof(path), "/proc/self/fd/%d/tasks", cgroup_fd);
        fd = open(path, O_WRONLY);
    }
    if (fd < 0) return -1;

    ssize_t n = write(fd, pid_str, strlen(pid_str));
    close(fd);
    return (n > 0) ? 0 : -1;
}

void cgroup_cleanup(const container_config *cfg) {
    const char *dirs[] = {
        "/sys/fs/cgroup/mycontainer",
        "/sys/fs/cgroup/memory/mycontainer",
        "/sys/fs/cgroup/cpu/mycontainer",
    };
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); i++) {
        if (access(dirs[i], F_OK) == 0) {
            rmdir(dirs[i]);  /* 只要 tasks 空了就能删 */
        }
    }
    (void)cfg;
}

/* ================================================================
 * pivot_root —— 把 new_root 变成容器的 "/"
 * ================================================================ */

int do_pivot_root(const char *new_root) {
    char put_old[MAX_PATH_LEN];

    /* 0. 确保 mount propagation 不影响 pivot_root */
    mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL);

    /* 1. bind mount new_root 到自身（pivot_root 要求目标不在同一个挂载树） */
    if (mount(new_root, new_root, NULL, MS_BIND | MS_REC, NULL) != 0) {
        fprintf(stderr, "[ERROR] bind mount rootfs 失败: %s\n", strerror(errno));
        return -1;
    }

    /* 2. 在 new_root 下创建 old_root 目录（用来放宿主机的旧根） */
    snprintf(put_old, sizeof(put_old), "%s/.pivot_root_old", new_root);
    if (mkdir(put_old, 0755) != 0) {
        fprintf(stderr, "[ERROR] 创建 %s 失败: %s\n", put_old, strerror(errno));
        return -1;
    }

    /* 3. 切换根！系统调用 pivot_root(new_root, put_old) */
    if (syscall(SYS_pivot_root, new_root, put_old) != 0) {
        fprintf(stderr, "[ERROR] pivot_root(%s, %s) 失败: %s\n",
                new_root, put_old, strerror(errno));
        return -1;
    }

    /* 4. pivot_root 后当前进程的工作目录还在旧根。
     *    chdir("/") 切换到新根，然后卸载旧根 */
    if (chdir("/") != 0) {
        fprintf(stderr, "[ERROR] chdir(/) 失败: %s\n", strerror(errno));
        return -1;
    }

    /* 5. 延迟卸载旧根文件系统 */
    if (umount2("/.pivot_root_old", MNT_DETACH) != 0) {
        fprintf(stderr, "[ERROR] 卸载旧根失败: %s\n", strerror(errno));
        return -1;
    }
    rmdir("/.pivot_root_old");

    return 0;
}

/* ================================================================
 * 挂载伪文件系统
 * ================================================================ */

/*
 * 挂载伪文件系统到指定前缀路径下（用于 pivot_root 之前的挂载）。
 * prefix 为空时直接挂载到 /proc、/sys 等（兼容旧调用）。
 *
 * 之所以要在 pivot_root 之前挂载：新内核（6.x）在非 init user namespace 里
 * 挂载 proc 时，内核会检查 mount_too_revealing() —— 如果当前 mount namespace
 * 里没有已存在的 proc 作为"参照"，挂载就会被拒绝（EPERM）。
 * pivot_root 之后旧根的 /proc 已被卸载，新 /proc 没有参照 → 失败。
 * 在 pivot_root 之前挂到 rootfs 路径下，宿主机的 /proc 就在当前 mount ns 中
 * 充当参照 → 通过内核检查。pivot_root 后自动变成容器的 /proc。
 */
int mount_pseudo_fs_at(const char *rootfs) {
    char proc_path[MAX_PATH_LEN], sys_path[MAX_PATH_LEN];
    char dev_path[MAX_PATH_LEN], tmp_path[MAX_PATH_LEN];

    snprintf(proc_path, sizeof(proc_path), "%s/proc", rootfs);
    snprintf(sys_path,  sizeof(sys_path),  "%s/sys",  rootfs);
    snprintf(dev_path,  sizeof(dev_path),  "%s/dev",  rootfs);
    snprintf(tmp_path,  sizeof(tmp_path),  "%s/tmp",  rootfs);

    /* ---- /proc ---- */
    mkdir(proc_path, 0755);
    if (mount("proc", proc_path, "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0)
        fprintf(stderr, "[WARN] 挂载 /proc 失败: %s\n", strerror(errno));

    /* ---- /sys ---- */
    mkdir(sys_path, 0755);
    if (mount("sysfs", sys_path, "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) != 0) {
        /* 非 init user namespace 回退：bind mount 宿主 /sys */
        if (mount("/sys", sys_path, NULL, MS_BIND | MS_REC, NULL) != 0)
            fprintf(stderr, "[WARN] 挂载 /sys 失败: %s\n", strerror(errno));
    }

    /* ---- /dev：优先 devtmpfs，不可用时回退 tmpfs + 手动 mknod ---- */
    mkdir(dev_path, 0755);
    if (mount("devtmpfs", dev_path, "devtmpfs", MS_NOSUID, NULL) != 0) {
        /* 手机内核通常没开 CONFIG_DEVTMPFS，用 tmpfs 兜底 */
        if (mount("tmpfs", dev_path, "tmpfs", MS_NOSUID | MS_NODEV, NULL) != 0)
            fprintf(stderr, "[WARN] 挂载 /dev (tmpfs) 失败: %s\n", strerror(errno));

        /* 手动创建基础设备节点 */
        #define MKNOD_IN(path, name, mode, maj, min) do { \
            char p[MAX_PATH_LEN]; snprintf(p, sizeof(p), "%s/%s", path, name); \
            mknod(p, mode, makedev(maj, min)); \
        } while(0)

        MKNOD_IN(dev_path, "null",    S_IFCHR | 0666, 1, 3);
        MKNOD_IN(dev_path, "zero",    S_IFCHR | 0666, 1, 5);
        MKNOD_IN(dev_path, "full",    S_IFCHR | 0666, 1, 7);
        MKNOD_IN(dev_path, "random",  S_IFCHR | 0444, 1, 8);
        MKNOD_IN(dev_path, "urandom", S_IFCHR | 0444, 1, 9);
        MKNOD_IN(dev_path, "tty",     S_IFCHR | 0666, 5, 0);
        MKNOD_IN(dev_path, "console", S_IFCHR | 0600, 5, 1);
        MKNOD_IN(dev_path, "ptmx",    S_IFCHR | 0666, 5, 2);

        char pts_path[MAX_PATH_LEN], shm_path[MAX_PATH_LEN];
        snprintf(pts_path, sizeof(pts_path), "%s/pts", dev_path);
        snprintf(shm_path, sizeof(shm_path), "%s/shm", dev_path);
        mkdir(pts_path, 0755);
        mount("devpts", pts_path, "devpts", MS_NOSUID | MS_NOEXEC, NULL);
        mkdir(shm_path, 0755);
        mount("tmpfs", shm_path, "tmpfs", MS_NOSUID | MS_NODEV, NULL);

        #undef MKNOD_IN
    }

    /* ---- /tmp ---- */
    mkdir(tmp_path, 0755);
    if (mount("tmpfs", tmp_path, "tmpfs", MS_NOSUID | MS_NODEV, NULL) != 0)
        fprintf(stderr, "[WARN] 挂载 /tmp 失败: %s\n", strerror(errno));

    return 0;
}

/* 兼容旧调用：在 "/" 下挂载伪文件系统 */
int mount_pseudo_fs(void) {
    return mount_pseudo_fs_at("");
}

/* ================================================================
 * V2 网络原语 —— RTNETLINK 封装
 * ================================================================ */

/* 创建 veth pair：容器侧 veth-c0，宿主机侧 veth-h0 */
static int create_veth_pair(void) {
    /*
     * 先尝试 netlink 创建，部分内核版本可能返回意外错误码（ERANGE），
     * 失败时回退到 ip 命令。
     */
    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));

    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    ifi->ifi_family = AF_UNSPEC;

    /* 容器侧接口名 */
    nl_put_str(nlh, IFLA_IFNAME, "veth-c0");

    /* IFLA_LINKINFO */
    struct nl_nest linkinfo;
    nl_nest_start(&linkinfo, nlh);
    nl_put_str(nlh, IFLA_INFO_KIND, "veth");

    /* IFLA_INFO_DATA */
    struct nl_nest infodata;
    nl_nest_start(&infodata, nlh);

    /* VETH_INFO_PEER (type=1)：嵌套对端接口定义 */
    struct nl_nest peer;
    nl_nest_start(&peer, nlh);

    /* 对端 ifinfomsg + IFLA_IFNAME
     * VETH_INFO_PEER 里嵌套的是裸 struct ifinfomsg（16 字节），不带 nlmsghdr。
     * 这里必须用 sizeof，不能用 NLMSG_LENGTH——后者会多算 16 字节的
     * NLMSG_HDRLEN，导致 IFLA_IFNAME 偏移错误、内核读不到对端名字而自动
     * 命名为 veth0（后续 link_set_master/link_up 找 veth-h0 就会静默失败）。 */
    int pay_start = NLMSG_ALIGN(nlh->nlmsg_len);
    nlh->nlmsg_len = pay_start + sizeof(struct ifinfomsg);
    struct ifinfomsg *pifi = (struct ifinfomsg *)((char *)nlh + pay_start);
    memset(pifi, 0, sizeof(*pifi));
    pifi->ifi_family = AF_UNSPEC;
    nl_put_str(nlh, IFLA_IFNAME, "veth-h0");

    nl_nest_end(&peer,    1);             /* VETH_INFO_PEER */
    nl_nest_end(&infodata, IFLA_INFO_DATA);
    nl_nest_end(&linkinfo, IFLA_LINKINFO);

    if (nl_talk(nlh) == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    if (g_cfg.verbose)
        printf("[INFO] netlink 创建 veth 失败, 回退到 ip 命令\n");
    return system(
        "ip link add veth-c0 type veth peer name veth-h0 2>/dev/null") == 0
           ? 0 : -1;
}

/* 创建 Linux bridge（如果已存在则复用） */
static int create_bridge(void) {
    if (get_ifindex("mc-bridge") >= 0) return 0;  /* 已存在 */

    /*
     * 先尝试 netlink：部分内核版本上 RTM_NEWLINK + NLM_F_EXCL
     * 可能返回意外错误码（ERANGE），此时回退到 ip 命令。
     */
    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));

    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    ifi->ifi_family = AF_UNSPEC;

    nl_put_str(nlh, IFLA_IFNAME, "mc-bridge");

    struct nl_nest li;
    nl_nest_start(&li, nlh);
    nl_put_str(nlh, IFLA_INFO_KIND, "bridge");
    nl_nest_end(&li, IFLA_LINKINFO);

    if (nl_talk(nlh) == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    if (g_cfg.verbose)
        printf("[INFO] netlink 创建 bridge 失败, 回退到 ip 命令\n");
    return system("ip link add mc-bridge type bridge 2>/dev/null") == 0
           ? 0 : -1;
}

/* 启动接口（set IFF_UP） */
static int link_up(const char *name) {
    int idx = get_ifindex(name);
    if (idx < 0) {
        fprintf(stderr, "[WARN] link_up: 接口 '%s' 不存在\n", name);
        return -1;
    }

    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));

    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    ifi->ifi_family  = AF_UNSPEC;
    ifi->ifi_index   = idx;
    ifi->ifi_flags   = IFF_UP;
    ifi->ifi_change  = IFF_UP;

    if (nl_talk(nlh) == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    fprintf(stderr, "[WARN] link_up %s: netlink 失败(%s), 回退 ip 命令\n",
            name, strerror(errno));
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "ip link set %s up 2>/dev/null", name);
    if (system(cmd) == 0) return 0;
    fprintf(stderr, "[WARN] link_up %s: ip 命令也失败\n", name);
    return -1;
}

/* 把接口移入指定 PID 的 network namespace */
static int link_set_ns(const char *name, pid_t pid) {
    int idx = get_ifindex(name);
    if (idx < 0) {
        fprintf(stderr, "[WARN] link_set_ns: 接口 '%s' 不存在\n", name);
        return -1;
    }

    char ns_path[64];
    snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/net", pid);
    int ns_fd = open(ns_path, O_RDONLY | O_CLOEXEC);
    if (ns_fd < 0) return -1;

    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));

    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    ifi->ifi_family = AF_UNSPEC;
    ifi->ifi_index  = idx;

    nl_put_u32(nlh, IFLA_NET_NS_FD, (uint32_t)ns_fd);

    int ret = nl_talk(nlh);
    close(ns_fd);

    if (ret == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    fprintf(stderr, "[WARN] link_set_ns %s: netlink 失败(%s), 回退 ip 命令\n",
            name, strerror(errno));
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "ip link set %s netns %d 2>/dev/null", name, pid);
    if (system(cmd) == 0) return 0;
    fprintf(stderr, "[WARN] link_set_ns %s: ip 命令也失败\n", name);
    return -1;
}

/* 把接口挂到 bridge 上 */
static int link_set_master(const char *name, const char *bridge) {
    int idx    = get_ifindex(name);
    int br_idx = get_ifindex(bridge);
    if (idx < 0 || br_idx < 0) {
        fprintf(stderr, "[WARN] link_set_master: 接口不存在 (name='%s' idx=%d, bridge='%s' idx=%d)\n",
                name, idx, bridge, br_idx);
        return -1;
    }

    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));

    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    ifi->ifi_family = AF_UNSPEC;
    ifi->ifi_index  = idx;

    nl_put_u32(nlh, IFLA_MASTER, (uint32_t)br_idx);

    if (nl_talk(nlh) == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    fprintf(stderr, "[WARN] link_set_master %s: netlink 失败(%s), 回退 ip 命令\n",
            name, strerror(errno));
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "ip link set %s master %s 2>/dev/null", name, bridge);
    if (system(cmd) == 0) return 0;
    fprintf(stderr, "[WARN] link_set_master %s: ip 命令也失败\n", name);
    return -1;
}

/* 重命名接口 */
static int link_rename(const char *old_name, const char *new_name) {
    int idx = get_ifindex(old_name);
    if (idx < 0) {
        fprintf(stderr, "[WARN] link_rename: 接口 '%s' 不存在\n", old_name);
        return -1;
    }

    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWLINK;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));

    struct ifinfomsg *ifi = NLMSG_DATA(nlh);
    ifi->ifi_family = AF_UNSPEC;
    ifi->ifi_index  = idx;

    nl_put_str(nlh, IFLA_IFNAME, new_name);

    if (nl_talk(nlh) == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    fprintf(stderr, "[WARN] link_rename %s: netlink 失败(%s), 回退 ip 命令\n",
            old_name, strerror(errno));
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "ip link set %s name %s 2>/dev/null", old_name, new_name);
    if (system(cmd) == 0) return 0;
    fprintf(stderr, "[WARN] link_rename %s: ip 命令也失败\n", old_name);
    return -1;
}

/* 给接口添加 IP 地址 */
static int addr_add(const char *name, const char *ip, int prefix) {
    int idx = get_ifindex(name);
    if (idx < 0) {
        fprintf(stderr, "[WARN] addr_add: 接口 '%s' 不存在\n", name);
        return -1;
    }

    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWADDR;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifaddrmsg));

    struct ifaddrmsg *ifa = NLMSG_DATA(nlh);
    ifa->ifa_family    = AF_INET;
    ifa->ifa_prefixlen = (unsigned char)prefix;
    ifa->ifa_index     = idx;
    ifa->ifa_scope     = RT_SCOPE_UNIVERSE;

    uint32_t addr;
    inet_pton(AF_INET, ip, &addr);
    nl_put_u32(nlh, IFA_LOCAL, addr);

    if (nl_talk(nlh) == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    fprintf(stderr, "[WARN] addr_add %s: netlink 失败(%s), 回退 ip 命令\n",
            name, strerror(errno));
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "ip addr add %s/%d dev %s 2>/dev/null", ip, prefix, name);
    if (system(cmd) == 0) return 0;
    fprintf(stderr, "[WARN] addr_add %s: ip 命令也失败\n", name);
    return -1;
}

/* 添加默认路由 */
static int route_add_default(const char *gateway) {
    char buf[NL_BUF];
    memset(buf, 0, sizeof(buf));
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;

    nlh->nlmsg_type  = RTM_NEWROUTE;
    nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | NLM_F_CREATE | NLM_F_EXCL;
    nlh->nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtmsg));

    struct rtmsg *rt = NLMSG_DATA(nlh);
    rt->rtm_family   = AF_INET;
    rt->rtm_table    = RT_TABLE_MAIN;
    rt->rtm_protocol = RTPROT_STATIC;
    rt->rtm_scope    = RT_SCOPE_UNIVERSE;
    rt->rtm_type     = RTN_UNICAST;

    uint32_t gw;
    inet_pton(AF_INET, gateway, &gw);
    nl_put_u32(nlh, RTA_GATEWAY, gw);

    if (nl_talk(nlh) == 0) return 0;

    /* netlink 失败 → 回退到 ip 命令 */
    fprintf(stderr, "[WARN] route_add_default: netlink 失败(%s), 回退 ip 命令\n",
            strerror(errno));
    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "ip route add default via %s 2>/dev/null", gateway);
    if (system(cmd) == 0) return 0;
    fprintf(stderr, "[WARN] route_add_default: ip 命令也失败\n");
    return -1;
}

/* ================================================================
 * IP 分配 & iptables 辅助
 * ================================================================ */

/* 从计数器文件分配容器 IP（10.0.0.2 起递增） */
static void allocate_ip(char *ip_buf, size_t len) {
    const char *counter_file = "/tmp/mycontainer_ip_counter";
    int counter = 2;

    int fd = open(counter_file, O_RDONLY);
    if (fd >= 0) {
        char buf[16];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) { buf[n] = '\0'; counter = atoi(buf); }
        close(fd);
    }

    snprintf(ip_buf, len, "10.0.0.%d", counter);

    fd = open(counter_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char buf[16];
        int n = snprintf(buf, sizeof(buf), "%d\n", counter + 1);
        if (n > 0) { ssize_t ign = write(fd, buf, (size_t)n); (void)ign; }
        close(fd);
    }
}

/* 调用外部 iptables 命令（静默执行） */
static void iptables_run(const char *fmt, ...) {
    char cmd[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof(cmd), fmt, ap);
    va_end(ap);
    int ret = system(cmd);
    (void)ret;
}

/* ================================================================
 * V2 网络编排 —— 宿主机侧
 * ================================================================ */

int network_host_setup(void) {
    if (!g_cfg.net_isolate) return 0;

    /* 1. 启用 IP 转发 */
    write_file("/proc/sys/net/ipv4/ip_forward", "1");

    /* 1.5 允许 loopback 源地址被路由 —— 本机 hairpin（curl localhost:PORT）
     * 需要：否则 127.0.0.1 会被内核当作 martian source 在重路由时丢弃，
     * 且这种丢弃不打印 dmesg 日志，回包永远回不来 */
    write_file("/proc/sys/net/ipv4/conf/all/route_localnet", "1");

    /* 2. 创建/复用 bridge，分配 IP，拉起 */
    if (create_bridge() != 0) {
        fprintf(stderr, "[WARN] 创建 bridge 失败: %s\n", strerror(errno));
        return -1;
    }
    addr_add("mc-bridge", "10.0.0.1", 24);
    link_up("mc-bridge");

    /* bridge 建好后再开它自己的 route_localnet（接口不存在时写会 ENOENT） */
    write_file("/proc/sys/net/ipv4/conf/mc-bridge/route_localnet", "1");

    if (g_cfg.verbose)
        printf("[INFO] bridge mc-bridge 就绪 (10.0.0.1/24)\n");

    /* 3. 分配容器 IP */
    if (g_cfg.container_ip[0] == '\0')
        allocate_ip(g_cfg.container_ip, sizeof(g_cfg.container_ip));

    if (g_cfg.verbose)
        printf("[INFO] 容器 IP: %s/24\n", g_cfg.container_ip);

    /* 4. 创建 veth pair */
    if (create_veth_pair() != 0) {
        fprintf(stderr, "[WARN] 创建 veth pair 失败: %s\n", strerror(errno));
        return -1;
    }

    /* 5. 宿主机侧 veth-h0：挂 bridge + 启动 */
    if (link_set_master("veth-h0", "mc-bridge") != 0 ||
        link_up("veth-h0") != 0) {
        fprintf(stderr, "[WARN] veth-h0 配置失败，容器网络可能不可用\n");
    } else if (g_cfg.verbose) {
        printf("[INFO] veth 已创建，veth-h0 已挂到 bridge\n");
    }

    /* 6. SNAT —— 容器出站流量做源地址转换 */
    iptables_run(
        "iptables -t nat -C POSTROUTING -s 10.0.0.0/24 ! -o mc-bridge "
        "-j MASQUERADE 2>/dev/null || "
        "iptables -t nat -A POSTROUTING -s 10.0.0.0/24 ! -o mc-bridge "
        "-j MASQUERADE");

    if (g_cfg.verbose)
        printf("[INFO] iptables MASQUERADE 规则已就绪\n");

    return 0;
}

/* 把容器侧 veth 移入子进程的 network namespace */
int network_move_veth(pid_t child_pid) {
    if (!g_cfg.net_isolate) return 0;

    if (link_set_ns("veth-c0", child_pid) != 0) {
        fprintf(stderr, "[WARN] 移动 veth-c0 到容器 netns 失败: %s\n",
                strerror(errno));
        return -1;
    }

    if (g_cfg.verbose)
        printf("[INFO] veth-c0 已移入容器 PID=%d 的 netns\n", child_pid);

    /* 应用端口映射（此时容器 IP 已确定） */
    for (int i = 0; i < g_cfg.port_map_count; i++) {
        char hp[8], cp[8];
        if (sscanf(g_cfg.port_maps[i], "%7[0-9]:%7[0-9]", hp, cp) != 2)
            continue;

        iptables_run(
            "iptables -t nat -C PREROUTING -p tcp --dport %s -j DNAT "
            "--to-destination %s:%s 2>/dev/null || "
            "iptables -t nat -A PREROUTING -p tcp --dport %s -j DNAT "
            "--to-destination %s:%s", hp, g_cfg.container_ip, cp,
            hp, g_cfg.container_ip, cp);

        iptables_run(
            "iptables -C FORWARD -p tcp -d %s --dport %s -j ACCEPT 2>/dev/null || "
            "iptables -A FORWARD -p tcp -d %s --dport %s -j ACCEPT",
            g_cfg.container_ip, cp, g_cfg.container_ip, cp);

        /* 本机 hairpin：让 curl localhost:<host_port> 也能通。
         * 本机发起的包走 OUTPUT 链（不经过 PREROUTING），需单独 DNAT；
         * 源地址 127.0.0.1 靠 network_host_setup 里的 route_localnet=1
         * 才允许被路由出去，再由下面的 POSTROUTING SNAT 改写成 bridge IP。 */
        iptables_run(
            "iptables -t nat -C OUTPUT -p tcp --dport %s -j DNAT "
            "--to-destination %s:%s 2>/dev/null || "
            "iptables -t nat -A OUTPUT -p tcp --dport %s -j DNAT "
            "--to-destination %s:%s", hp, g_cfg.container_ip, cp,
            hp, g_cfg.container_ip, cp);

        iptables_run(
            "iptables -t nat -C POSTROUTING -s 127.0.0.0/8 -p tcp -d %s --dport %s "
            "-j SNAT --to-source 10.0.0.1 2>/dev/null || "
            "iptables -t nat -A POSTROUTING -s 127.0.0.0/8 -p tcp -d %s --dport %s "
            "-j SNAT --to-source 10.0.0.1",
            g_cfg.container_ip, cp, g_cfg.container_ip, cp);

        if (g_cfg.verbose)
            printf("[INFO] 端口映射 %s:%s -> %s:%s\n",
                   hp, cp, g_cfg.container_ip, cp);
    }

    return 0;
}

/* ================================================================
 * V2 网络编排 —— 容器侧
 * ================================================================ */

int network_child_setup(void) {
    if (!g_cfg.net_isolate) return 0;

    /* 等待父进程把 veth-c0 移入（轮询，最多等 1 秒） */
    int ifidx = -1;
    for (int i = 0; i < 100; i++) {
        ifidx = get_ifindex("veth-c0");
        if (ifidx >= 0) break;
        usleep(10000);  /* 10ms */
    }
    if (ifidx < 0) {
        fprintf(stderr, "[WARN] veth-c0 未出现，网络不可用\n");
        return -1;
    }

    /* 1. 重命名 veth-c0 → eth0 */
    if (link_rename("veth-c0", "eth0") != 0) {
        fprintf(stderr, "[WARN] 重命名 veth-c0 → eth0 失败: %s\n",
                strerror(errno));
        return -1;
    }

    /* 2. 拉起 lo */
    link_up("lo");

    /* 3. 拉起 eth0 */
    if (link_up("eth0") != 0) {
        fprintf(stderr, "[WARN] 启动 eth0 失败: %s\n", strerror(errno));
        return -1;
    }

    /* 4. 分配 IP */
    if (addr_add("eth0", g_cfg.container_ip, 24) != 0) {
        fprintf(stderr, "[WARN] 给 eth0 添加 IP %s 失败: %s\n",
                g_cfg.container_ip, strerror(errno));
        return -1;
    }

    /* 5. 默认路由 → bridge */
    if (route_add_default("10.0.0.1") != 0) {
        fprintf(stderr, "[WARN] 添加默认路由失败: %s\n", strerror(errno));
        return -1;
    }

    if (g_cfg.verbose)
        printf("[容器] 网络就绪: eth0=%s/24, gw=10.0.0.1\n",
               g_cfg.container_ip);

    return 0;
}

/* ================================================================
 * V2 网络清理
 * ================================================================ */

void network_cleanup(void) {
    if (!g_cfg.net_isolate) return;

    /* 删除端口映射 DNAT 规则 */
    for (int i = 0; i < g_cfg.port_map_count; i++) {
        char hp[8], cp[8];
        if (sscanf(g_cfg.port_maps[i], "%7[0-9]:%7[0-9]", hp, cp) != 2)
            continue;

        iptables_run(
            "iptables -t nat -D PREROUTING -p tcp --dport %s -j DNAT "
            "--to-destination %s:%s 2>/dev/null", hp, g_cfg.container_ip, cp);

        iptables_run(
            "iptables -t nat -D OUTPUT -p tcp --dport %s -j DNAT "
            "--to-destination %s:%s 2>/dev/null", hp, g_cfg.container_ip, cp);

        iptables_run(
            "iptables -t nat -D POSTROUTING -s 127.0.0.0/8 -p tcp -d %s --dport %s "
            "-j SNAT --to-source 10.0.0.1 2>/dev/null", g_cfg.container_ip, cp);

        iptables_run(
            "iptables -D FORWARD -p tcp -d %s --dport %s -j ACCEPT 2>/dev/null",
            g_cfg.container_ip, cp);
    }

    /* 删除宿主机侧 veth（容器侧的随 netns 销毁自动消失） */
    if (get_ifindex("veth-h0") >= 0)
        iptables_run("ip link del veth-h0 2>/dev/null");

    if (g_cfg.verbose)
        printf("[INFO] 网络资源已清理\n");
}

/* ================================================================
 * V3 User Namespace 设置 —— 父进程侧
 * ================================================================ */

int setup_user_ns(pid_t child_pid) {
    char path[128];
    char map[64];

    /* 1. 先写 setgroups=deny（内核 3.19+ 强制要求，必须在 gid_map 之前） */
    snprintf(path, sizeof(path), "/proc/%d/setgroups", child_pid);
    if (write_file(path, "deny") != 0) {
        fprintf(stderr, "[WARN] 写 setgroups 失败: %s\n", strerror(errno));
        /* 非致命：旧内核不需要这一步 */
    }

    /* 2. 写 uid_map：容器内 0 → 宿主机真实用户（sudo 场景） */
    uid_t host_uid = 1000;
    char *sudo_uid = getenv("SUDO_UID");
    if (sudo_uid) host_uid = (uid_t)atoi(sudo_uid);

    snprintf(map, sizeof(map), "0 %u 1", host_uid);
    snprintf(path, sizeof(path), "/proc/%d/uid_map", child_pid);
    if (write_file(path, map) != 0) {
        fprintf(stderr, "[WARN] 写 uid_map 失败: %s\n", strerror(errno));
        return -1;
    }

    /* 3. 写 gid_map：容器内 0 → 宿主机真实用户组 */
    gid_t host_gid = 1000;
    char *sudo_gid = getenv("SUDO_GID");
    if (sudo_gid) host_gid = (gid_t)atoi(sudo_gid);

    snprintf(map, sizeof(map), "0 %u 1", host_gid);
    snprintf(path, sizeof(path), "/proc/%d/gid_map", child_pid);
    if (write_file(path, map) != 0) {
        fprintf(stderr, "[WARN] 写 gid_map 失败: %s\n", strerror(errno));
        return -1;
    }

    if (g_cfg.verbose)
        printf("[INFO] User NS: 容器 root → 宿主机 UID=%u, GID=%u\n",
               host_uid, host_gid);

    return 0;
}

/* ================================================================
 * V3 Capabilities 裁剪 —— 子进程侧
 * ================================================================ */

int drop_capabilities(void) {
    /* 容器不需要的危险能力清单 */
    unsigned long caps[] = {
        CAP_SYS_MODULE,     /* 禁止加载内核模块           */
        CAP_SYS_RAWIO,      /* 禁止直接 I/O 端口/内存访问 */
        CAP_SYS_BOOT,       /* 禁止重启                   */
        CAP_SYS_TIME,       /* 禁止修改系统时钟           */
        CAP_SYSLOG,         /* 禁止读取内核日志           */
        CAP_WAKE_ALARM,     /* 禁止设置唤醒闹钟           */
        CAP_MAC_ADMIN,      /* 禁止修改 MAC 策略          */
        CAP_MAC_OVERRIDE,   /* 禁止覆盖 MAC               */
        CAP_SYS_PACCT,      /* 禁止进程记账               */
        CAP_BLOCK_SUSPEND,  /* 禁止阻止挂起               */
    };
    int count = sizeof(caps) / sizeof(caps[0]);

    for (int i = 0; i < count; i++) {
        if (prctl(PR_CAPBSET_DROP, caps[i], 0, 0, 0) != 0) {
            if (g_cfg.verbose)
                fprintf(stderr, "[WARN] 丢弃 cap %lu 失败: %s\n",
                        caps[i], strerror(errno));
        }
    }

    if (g_cfg.verbose)
        printf("[容器] 已丢弃 %d 项危险能力\n", count);

    return 0;
}

/* ================================================================
 * 子进程 —— 在隔离的 namespace 里执行
 * ================================================================ */

int child_setup(void *arg) {
    int sync_fd = (int)(intptr_t)arg;

    /* ---- 0. User NS 同步（仅当启用 user namespace 时）---- */
    if (sync_fd >= 0) {
        char ready;
        if (read(sync_fd, &ready, 1) != 1) {
            fprintf(stderr, "[FATAL] user namespace 同步失败\n");
            exit(1);
        }
        close(sync_fd);

        /* 确认 uid_map 生效，强制设置 UID=0 */
        if (setresuid(0, 0, 0) != 0 || setresgid(0, 0, 0) != 0) {
            fprintf(stderr, "[WARN] setresuid/setresgid 失败: %s (uid=%d, gid=%d)\n",
                    strerror(errno), getuid(), getgid());
        }
    }

    /*
     * 重建 mount namespace。clone() 给的 mount ns 可能与后续 bind mount
     * 冲突导致 pivot_root EINVAL，unshare 得到一个干净的 mount ns。
     * 有 user NS 时，这还让 mount ns 归属当前 user NS。
     */
    if (unshare(CLONE_NEWNS) != 0) {
        fprintf(stderr, "[FATAL] 重建 mount namespace 失败: %s\n", strerror(errno));
        exit(1);
    }

    if (g_cfg.verbose) {
        printf("[容器] PID=%d  PPID=%d  uid=%d(%s) gid=%d  进入子进程\n",
               getpid(), getppid(),
               getuid(), getuid() == 0 ? "root" : "!",
               getgid());
    }

    /* ---- 0.5 V3：裁剪危险 capabilities（在 pivot_root 之前）---- */
    drop_capabilities();

    /* ---- 1. 切换根文件系统 ---- */
    /*
     * user NS 启用时：必须在 pivot_root 前挂 proc/sys（mount_too_revealing 需要参照）。
     * 无 user NS 时：pivot_root 先、mount 后（避免 submount 导致 pivot_root EINVAL）。
     */
    if (g_cfg.image_name[0] != '\0') {
        char lower[MAX_PATH_LEN], upper[MAX_PATH_LEN];
        char work[MAX_PATH_LEN], merged[MAX_PATH_LEN];

        const char *cid = g_cfg.container_name[0]
                          ? g_cfg.container_name : "default";

        container_overlay_paths(cid, lower, upper, work, merged,
                                sizeof(lower));

        if (overlay_mount(lower, upper, work, merged) != 0) {
            fprintf(stderr, "[FATAL] overlay 挂载失败，容器无法启动\n");
            exit(1);
        }

        if (sync_fd >= 0)
            mount_pseudo_fs_at(merged);

        if (do_pivot_root(merged) != 0) {
            fprintf(stderr, "[FATAL] pivot_root 失败，容器无法启动\n");
            exit(1);
        }
    } else {
        if (sync_fd >= 0)
            mount_pseudo_fs_at(g_cfg.rootfs);

        if (do_pivot_root(g_cfg.rootfs) != 0) {
            fprintf(stderr, "[FATAL] pivot_root 失败，容器无法启动\n");
            exit(1);
        }
    }

    /* pivot_root 后挂载伪文件系统（无 user NS 时走这路） */
    if (sync_fd < 0)
        mount_pseudo_fs();

    /* DNS */
    mkdir("/etc", 0755);
    write_regular("/etc/resolv.conf", "nameserver 8.8.8.8\n");

    /* ---- 3. 设置主机名 ---- */
    if (sethostname(g_cfg.hostname, strlen(g_cfg.hostname)) != 0) {
        fprintf(stderr, "[WARN] 设置主机名失败: %s\n", strerror(errno));
    }

    /* ---- 3.2 V2 网络：容器侧配置（rename veth→eth0 + IP + 路由）---- */
    network_child_setup();

    /* ---- 3.5 设置工作目录 ---- */
    if (g_cfg.workdir[0] != '\0') {
        if (chdir(g_cfg.workdir) != 0) {
            fprintf(stderr, "[WARN] chdir %s 失败: %s\n", g_cfg.workdir, strerror(errno));
        }
    }

    /* ---- 3.6 注入用户环境变量 ---- */
    if (g_cfg.env_extra[0] != '\0') {
        char *saveptr;
        char buf[sizeof(g_cfg.env_extra)];
        strncpy(buf, g_cfg.env_extra, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *line = strtok_r(buf, "\n", &saveptr);
        while (line) {
            if (line[0] != '\0') putenv(line);
            line = strtok_r(NULL, "\n", &saveptr);
        }
    }

    /* ---- 4. 设置环境变量 PS1（让 shell 好认） ---- */
    setenv("PS1", "\\[\\e[1;32m\\]container:\\w\\$ \\[\\e[0m\\]", 1);
    setenv("HOME", "/root", 1);
    setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", 1);
    setenv("TERM", "xterm", 1);

    /* ---- 4.5 V3: rootfs 模式处理 ---- */
    if (strcmp(g_cfg.rootfs_mode, "ro") == 0) {
        /* ro 模式：整个 rootfs 只读 */
        if (mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY | MS_BIND, NULL) != 0)
            fprintf(stderr, "[WARN] remount / readonly 失败: %s\n", strerror(errno));
        else if (g_cfg.verbose)
            printf("[容器] rootfs 模式=ro (完全只读)\n");
    }
    /* rw 模式（默认）：什么都不做，rootfs 保持可写 */

    /* ---- 5. 执行用户命令 ---- */
    if (g_cfg.verbose) {
        printf("[容器] 执行: %s\n", g_cfg.cmd);
    }

    char *shell = "/bin/sh";
    char *argv_exec[] = { shell, "-c", g_cfg.cmd, NULL };

    execve(shell, argv_exec, environ);

    /* execve 不会返回，如果到了这里就是失败了 */
    fprintf(stderr, "[FATAL] execve %s 失败: %s\n", shell, strerror(errno));
    exit(1);
}

/* ================================================================
 * V4 状态管理
 * ================================================================ */

#define STATE_DIR "/data/mycontainer/containers"

int state_init(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "mkdir -p " STATE_DIR);
    return system(cmd) == 0 ? 0 : -1;
}

static void state_path(const char *id, const char *file, char *buf, size_t size) {
    snprintf(buf, size, STATE_DIR "/%s/%s", id, file);
}

static int state_mkdir(const char *id) {
    char dir[256];
    snprintf(dir, sizeof(dir), STATE_DIR "/%s", id);
    return mkdir(dir, 0755) == 0 ? 0 : (errno == EEXIST ? 0 : -1);
}

static int write_state_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) return -1;
    ssize_t len = strlen(value);
    ssize_t n   = write(fd, value, len);
    close(fd);
    return (n == len) ? 0 : -1;
}

static int read_state_file(const char *path, char *buf, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, size - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    /* trim trailing newline */
    if (n > 0 && buf[n-1] == '\n') buf[n-1] = '\0';
    return 0;
}

int state_save(const container_info *ci) {
    state_init();
    state_mkdir(ci->id);

    char path[512];
    char buf[128];

    state_path(ci->id, "pid", path, sizeof(path));
    snprintf(buf, sizeof(buf), "%d", ci->pid);
    if (write_state_file(path, buf) != 0) return -1;

    state_path(ci->id, "name", path, sizeof(path));
    if (write_state_file(path, ci->name) != 0) return -1;

    state_path(ci->id, "rootfs", path, sizeof(path));
    if (write_state_file(path, ci->rootfs) != 0) return -1;

    state_path(ci->id, "cmd", path, sizeof(path));
    if (write_state_file(path, ci->cmd) != 0) return -1;

    state_path(ci->id, "created_at", path, sizeof(path));
    snprintf(buf, sizeof(buf), "%ld", (long)ci->created_at);
    if (write_state_file(path, buf) != 0) return -1;

    return 0;
}

int state_load(const char *id, container_info *ci) {
    char path[512];
    char buf[MAX_PATH_LEN];

    memset(ci, 0, sizeof(*ci));
    strncpy(ci->id, id, MAX_ID_LEN - 1);

    state_path(id, "name", path, sizeof(path));
    if (read_state_file(path, ci->name, MAX_NAME_LEN) != 0)
        return -1;

    state_path(id, "pid", path, sizeof(path));
    if (read_state_file(path, buf, sizeof(buf)) != 0) return -1;
    ci->pid = (pid_t)atoi(buf);

    state_path(id, "rootfs", path, sizeof(path));
    read_state_file(path, ci->rootfs, MAX_PATH_LEN);

    state_path(id, "cmd", path, sizeof(path));
    read_state_file(path, ci->cmd, MAX_CMD_LEN);

    state_path(id, "created_at", path, sizeof(path));
    if (read_state_file(path, buf, sizeof(buf)) == 0)
        ci->created_at = (time_t)atol(buf);

    return 0;
}

int state_delete(const char *id) {
    char dir[512];
    snprintf(dir, sizeof(dir), STATE_DIR "/%s", id);

    char path[512];
    const char *files[] = {"pid", "name", "rootfs", "cmd", "created_at", "image", NULL};
    for (int i = 0; files[i]; i++) {
        state_path(id, files[i], path, sizeof(path));
        unlink(path);  /* best-effort, ignore errors */
    }
    return rmdir(dir);
}

int state_list(container_info *list, int max) {
    state_init();
    DIR *d = opendir(STATE_DIR);
    if (!d) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max) {
        if (ent->d_name[0] == '.') continue;
        if (state_load(ent->d_name, &list[count]) == 0)
            count++;
    }
    closedir(d);
    return count;
}

int state_update_pid(const char *id, pid_t pid) {
    char path[512], buf[64];
    state_path(id, "pid", path, sizeof(path));
    snprintf(buf, sizeof(buf), "%d", pid);
    return write_state_file(path, buf);
}

/* ================================================================
 * V4 生命周期命令
 * ================================================================ */

int cmd_ps(int verbose) {
    container_info list[64];
    int n = state_list(list, 64);

    if (!verbose) {
        printf("%-20s %-15s %-8s %-8s %s\n",
               "ID", "NAME", "PID", "STATUS", "ROOTFS");
        printf("---------------------------------------------------------------"
               "--------------------\n");
    }

    for (int i = 0; i < n; i++) {
        int alive = (list[i].pid > 0 && kill(list[i].pid, 0) == 0);
        const char *status = alive ? "running" : "stopped";

        if (verbose) {
            printf("ID:      %s\n", list[i].id);
            printf("Name:    %s\n", list[i].name[0] ? list[i].name : "-");
            printf("PID:     %d (%s)\n", list[i].pid, status);
            printf("Rootfs:  %s\n", list[i].rootfs);
            printf("Cmd:     %s\n", list[i].cmd);
            char tbuf[64];
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S",
                     localtime(&list[i].created_at));
            printf("Created: %s\n", tbuf);
            printf("---\n");
        } else {
            printf("%-20s %-15s %-8d %-8s %s\n",
                   list[i].id,
                   list[i].name[0] ? list[i].name : "-",
                   alive ? list[i].pid : -1,
                   status,
                   list[i].rootfs);
        }
    }

    if (n == 0) printf("(没有容器)\n");
    return 0;
}

int cmd_stop(const char *id) {
    container_info ci;
    if (state_load(id, &ci) != 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 不存在\n", id);
        return -1;
    }
    if (ci.pid <= 0 || kill(ci.pid, 0) != 0) {
        fprintf(stderr, "[INFO] 容器 '%s' 已停止\n", id);
        return 0;
    }

    printf("正在停止容器 '%s' (PID=%d)...\n", id, ci.pid);
    if (kill(ci.pid, SIGTERM) != 0) {
        fprintf(stderr, "[ERROR] 发送 SIGTERM 失败: %s\n", strerror(errno));
        return -1;
    }

    /* 等待最多 3 秒 */
    for (int i = 0; i < 30; i++) {
        if (kill(ci.pid, 0) != 0) {
            printf("容器 '%s' 已停止\n", id);
            state_update_pid(id, -1);
            return 0;
        }
        usleep(100000);  /* 100ms */
    }

    /* 超时，强制 SIGKILL */
    printf("超时，发送 SIGKILL...\n");
    kill(ci.pid, SIGKILL);
    state_update_pid(id, -1);
    printf("容器 '%s' 已强制停止\n", id);
    return 0;
}

int cmd_exec(const char *id, const char *command) {
    container_info ci;
    if (state_load(id, &ci) != 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 不存在\n", id);
        return -1;
    }
    if (ci.pid <= 0 || kill(ci.pid, 0) != 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 未运行 (PID=%d)\n", id, ci.pid);
        return -1;
    }

    /*
     * V5: 原生 setns() 实现 —— 不再依赖外部 nsenter 命令。
     *
     * setns() 的权限模型：
     *   宿主机 root 拥有 CAP_SYS_ADMIN，可以直接加入任意容器的
     *   mnt/uts/ipc/net/pid/cgroup namespace，不需要先进 user ns。
     *
     * 不进入 user namespace 的原因：
     *   一旦 setns(&user_ns) 就会丢弃宿主机 root 身份，导致后续无法
     *   访问容器文件系统中的 root 专属文件。保持宿主机 root 进入容器
     *   其他 namespace 是 docker exec / nsenter -a（不含 -U）的惯用做法。
     */

    /* ---- 1. 打开目标容器的所有 namespace fd（在 fork 之前）---- */
    const char *ns_names[] = {"mnt", "uts", "ipc", "net", "pid", "cgroup"};
    int ns_count = sizeof(ns_names) / sizeof(ns_names[0]);
    int ns_fds[6] = {-1, -1, -1, -1, -1, -1};

    for (int i = 0; i < ns_count; i++) {
        char ns_path[128];
        snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns/%s", ci.pid, ns_names[i]);
        ns_fds[i] = open(ns_path, O_RDONLY | O_CLOEXEC);
        if (ns_fds[i] < 0) {
            fprintf(stderr, "[ERROR] 无法打开 /proc/%d/ns/%s: %s\n",
                    ci.pid, ns_names[i], strerror(errno));
            for (int j = 0; j < i; j++) close(ns_fds[j]);
            return -1;
        }
    }

    /* ---- 2. fork 子进程执行 setns + exec ---- */
    pid_t child = fork();
    if (child == -1) {
        perror("fork");
        for (int i = 0; i < ns_count; i++) close(ns_fds[i]);
        return -1;
    }

    if (child == 0) {
        /*
         * 子进程：逐个加入目标容器的 namespace。
         *
         * 顺序说明：
         *   - mnt 先进：后续 chdir("/") 才能看到容器的根文件系统
         *   - pid  最后进：进入 PID ns 后 getpid() 会变成容器内的 PID，
         *     但不影响 execve（新进程仍需 fork 才能获得容器 PID 视图）
         *   - uts/ipc/net/cgroup 顺序无严格要求
         */

        /* mnt 以外按顺序进 */
        for (int i = 0; i < ns_count; i++) {
            if (setns(ns_fds[i], 0) != 0) {
                fprintf(stderr, "[ERROR] setns(%s) 失败: %s\n",
                        ns_names[i], strerror(errno));
                _exit(1);
            }
            close(ns_fds[i]);
        }

        /* 现在身处容器的文件系统 + 网络 + PID 等全部 namespace 中 */
        if (chdir("/") != 0) {
            fprintf(stderr, "[ERROR] exec: chdir(/) 失败: %s\n", strerror(errno));
            _exit(1);
        }

        /* 执行用户命令 */
        char *shell = "/bin/sh";
        char *argv_exec[] = { shell, "-c", (char *)command, NULL };
        execve(shell, argv_exec, environ);

        fprintf(stderr, "[ERROR] execve %s 失败: %s\n", shell, strerror(errno));
        _exit(1);
    }

    /* ---- 3. 父进程：关闭 fd，等待子进程结束 ---- */
    for (int i = 0; i < ns_count; i++) close(ns_fds[i]);

    int status;
    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid");
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int cmd_rm(const char *id) {
    container_info ci;
    if (state_load(id, &ci) != 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 不存在\n", id);
        return -1;
    }
    if (ci.pid > 0 && kill(ci.pid, 0) == 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 还在运行，请先 stop\n", id);
        return -1;
    }

    /* V6: 清理 overlay 目录（upper 可能有写入内容，用 rm -rf） */
    char upper[MAX_PATH_LEN], merged[MAX_PATH_LEN], work[MAX_PATH_LEN];
    snprintf(upper,  sizeof(upper),  STATE_DIR "/%s/upper",  id);
    snprintf(merged, sizeof(merged), STATE_DIR "/%s/merged", id);
    snprintf(work,   sizeof(work),   STATE_DIR "/%s/work",   id);

    /* 保险：尝试卸载 overlay */
    if (access(merged, F_OK) == 0) overlay_umount(merged);

    /* 删除 overlay 相关目录（system 调用 rm -rf，简单可靠） */
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" \"%s\" \"%s\"",
             upper, merged, work);
    int rc = system(cmd);
    (void)rc;

    /* 原有的状态清理 */
    if (state_delete(id) != 0) {
        fprintf(stderr, "[ERROR] 删除容器 '%s' 状态失败: %s\n",
                id, strerror(errno));
        return -1;
    }
    printf("容器 '%s' 已删除\n", id);
    return 0;
}

/* ================================================================
 * V6 OverlayFS —— 分层文件系统
 * ================================================================ */

void container_overlay_paths(const char *id,
                             char *lower, char *upper,
                             char *work, char *merged, size_t size) {
    snprintf(lower,  size, IMAGES_DIR "/%s/rootfs", g_cfg.image_name);
    snprintf(upper,  size, STATE_DIR "/%s/upper",    id);
    snprintf(work,   size, STATE_DIR "/%s/work",     id);
    snprintf(merged, size, STATE_DIR "/%s/merged",   id);
}

int overlay_mount(const char *lowerdir, const char *upperdir,
                  const char *workdir, const char *merged) {
    mkdir(upperdir, 0755);
    mkdir(workdir,  0755);
    mkdir(merged,   0755);

    char opts[3 * MAX_PATH_LEN + 64];
    snprintf(opts, sizeof(opts),
             "lowerdir=%s,upperdir=%s,workdir=%s",
             lowerdir, upperdir, workdir);

    if (g_cfg.verbose)
        printf("[INFO] overlay: lower=%s upper=%s\n", lowerdir, upperdir);

    if (mount("overlay", merged, "overlay", 0, opts) != 0) {
        fprintf(stderr, "[ERROR] overlay mount 失败: %s\n",
                strerror(errno));
        return -1;
    }
    return 0;
}

int overlay_umount(const char *merged) {
    /* 尝试卸载，容器可能已经 mount namespace 销毁自动清掉了 */
    if (umount2(merged, MNT_DETACH) != 0) {
        if (errno != EINVAL && errno != ENOENT) {
            fprintf(stderr, "[WARN] overlay umount %s: %s\n",
                    merged, strerror(errno));
            return -1;
        }
    }
    return 0;
}

/* ================================================================
 * V6 镜像管理
 * ================================================================ */

int cmd_import(const char *tarball, const char *name) {
    /* 检查 tarball 存在 */
    struct stat st;
    if (stat(tarball, &st) != 0) {
        fprintf(stderr, "[ERROR] 文件不存在: %s\n", tarball);
        return -1;
    }

    char rootfs[MAX_PATH_LEN];
    snprintf(rootfs, sizeof(rootfs), IMAGES_DIR "/%s/rootfs", name);

    char cmd[2048];
    /* 先创建目录 */
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", rootfs);
    if (system(cmd) != 0) {
        fprintf(stderr, "[ERROR] 创建镜像目录失败\n");
        return -1;
    }

    /* 解压 .tar.gz */
    printf("导入镜像 '%s' ...\n", name);
    snprintf(cmd, sizeof(cmd), "tar xzf \"%s\" -C \"%s\"", tarball, rootfs);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[ERROR] 解压失败 (tar xzf)，请确认是标准的 .tar.gz 格式\n");
        return -1;
    }

    printf("镜像 '%s' 导入成功 (%s)\n", name, rootfs);
    return 0;
}

int cmd_images(void) {
    DIR *d = opendir(IMAGES_DIR);
    if (!d) {
        printf("(没有镜像)\n");
        return 0;
    }

    printf("%-20s %s\n", "NAME", "PATH");
    printf("----------------------------------------\n");

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char path[MAX_PATH_LEN];
        snprintf(path, sizeof(path), IMAGES_DIR "/%s/rootfs", ent->d_name);
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("%-20s %s\n", ent->d_name, path);
        }
    }
    closedir(d);
    return 0;
}

int cmd_rmi(const char *name) {
    char path[MAX_PATH_LEN];
    snprintf(path, sizeof(path), IMAGES_DIR "/%s", name);

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "[ERROR] 镜像 '%s' 不存在\n", name);
        return -1;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[ERROR] 删除镜像 '%s' 失败\n", name);
        return -1;
    }

    printf("镜像 '%s' 已删除\n", name);
    return 0;
}

int cmd_commit(const char *id, const char *name) {
    container_info ci;
    if (state_load(id, &ci) != 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 不存在\n", id);
        return -1;
    }
    if (ci.pid > 0 && kill(ci.pid, 0) == 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 还在运行，请先 stop\n", id);
        return -1;
    }

    char source_rootfs[MAX_PATH_LEN];
    int   is_overlay = 0;
    char lower[MAX_PATH_LEN], upper[MAX_PATH_LEN];
    char work[MAX_PATH_LEN], merged[MAX_PATH_LEN];
    char tmp_tar[256];

    /*
     * 判断容器用的是 image 还是 rootfs 模式：
     * 检查是否存在 overlay upper 目录。
     * 路径: STATE_DIR/<id>/upper
     */
    snprintf(upper, sizeof(upper), STATE_DIR "/%s/upper", id);
    struct stat st;
    if (stat(upper, &st) == 0 && S_ISDIR(st.st_mode)) {
        /* overlay 模式：需要读到 image_name */
        is_overlay = 1;

        /* 从状态文件中读 image_name */
        char img_path[MAX_PATH_LEN];
        snprintf(img_path, sizeof(img_path), STATE_DIR "/%s/image", id);
        char image_name[MAX_NAME_LEN] = {0};
        int fd = open(img_path, O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, image_name, sizeof(image_name) - 1);
            if (n > 0 && image_name[n-1] == '\n') image_name[n-1] = '\0';
            close(fd);
        }
        if (image_name[0] == '\0') {
            fprintf(stderr, "[ERROR] 无法识别容器镜像信息\n");
            return -1;
        }

        container_overlay_paths(id, lower, upper, work, merged,
                                sizeof(lower));
        /* 覆盖 upper/ merged 路径（container_overlay_paths 用了 g_cfg，
         * 但 commit 时 g_cfg 可能是空的，手动构造） */
        snprintf(lower,  sizeof(lower),  IMAGES_DIR "/%s/rootfs", image_name);
        snprintf(upper,  sizeof(upper),  STATE_DIR "/%s/upper",   id);
        snprintf(work,   sizeof(work),   STATE_DIR "/%s/work",    id);
        snprintf(merged, sizeof(merged), STATE_DIR "/%s/merged",  id);

        /* 重新挂载 overlay 获取完整 merged 视图 */
        printf("重新挂载 overlay 获取完整视图...\n");
        if (overlay_mount(lower, upper, work, merged) != 0) {
            return -1;
        }
        strncpy(source_rootfs, merged, MAX_PATH_LEN - 1);
    } else {
        /* rootfs 模式：直接用 rootfs */
        if (ci.rootfs[0] == '\0') {
            fprintf(stderr, "[ERROR] 无法获取容器 rootfs 路径\n");
            return -1;
        }
        strncpy(source_rootfs, ci.rootfs, MAX_PATH_LEN - 1);
    }

    /* 打包 */
    snprintf(tmp_tar, sizeof(tmp_tar), "/tmp/mycontainer_commit_%s.tar.gz", name);
    printf("打包容器文件系统为镜像 '%s' ...\n", name);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "tar czf \"%s\" -C \"%s\" . 2>/dev/null",
             tmp_tar, source_rootfs);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "[ERROR] tar 打包失败\n");
        if (is_overlay) overlay_umount(merged);
        return -1;
    }

    /* 卸载 overlay（如果挂载了） */
    if (is_overlay) overlay_umount(merged);

    /* 导入为新镜像 */
    ret = cmd_import(tmp_tar, name);
    unlink(tmp_tar);

    return ret;
}

/* ================================================================
 * 主入口
 * ================================================================ */

/* ---- 信号处理：前台 run 时转发 SIGINT/SIGTERM 给容器子进程 ---- */
static volatile pid_t g_child_pid;
static char g_argv0[256];

static void signal_forward(int sig) {
    if (g_child_pid > 0) kill(g_child_pid, sig);
}

/* ---- 生成容器 ID ---- */
static void generate_id(char *buf, size_t size) {
    snprintf(buf, size, "default");
}

/* ---- 执行容器启动（从 run 命令调用）---- */
static int run_container(void) {
    printf("\n"
           "  ╔══════════════════════════════════════╗\n"
           "  ║  mycontainer — 手写容器运行时        ║\n"
           "  ╚══════════════════════════════════════╝\n\n");

    if (g_cfg.verbose) {
        printf("  配置:\n");
        printf("    rootfs    = %s\n",   g_cfg.rootfs);
        printf("    hostname  = %s\n",   g_cfg.hostname);
        printf("    cmd       = %s\n",   g_cfg.cmd);
        printf("    memory    = %s\n",   g_cfg.memory_limit[0] ? g_cfg.memory_limit : "(不限制)");
        printf("    cpu       = %s\n",   g_cfg.cpu_limit[0]   ? g_cfg.cpu_limit   : "(不限制)");
        printf("    workdir   = %s\n",   g_cfg.workdir[0]     ? g_cfg.workdir     : "(/)");
        printf("    env       = %s\n",   g_cfg.env_extra[0]   ? g_cfg.env_extra   : "(无)");
        printf("    network   = %s\n",   g_cfg.net_isolate    ? "隔离 (bridge+NAT)" : "共享宿主机");
        if (g_cfg.net_isolate) {
            printf("    ip        = %s\n", g_cfg.container_ip[0] ? g_cfg.container_ip : "(自动分配)");
            for (int j = 0; j < g_cfg.port_map_count; j++)
                printf("    port map  = %s\n", g_cfg.port_maps[j]);
        }
        if (g_cfg.detach) printf("    mode      = 后台运行\n");
        printf("\n");
    }

    /* ---- 2. 创建 cgroup（在 clone 之前，子进程会继承）---- */
    int cgroup_fd = cgroup_setup(&g_cfg);

    /* ---- 2.5 V2 网络：宿主机侧准备（bridge + veth + NAT）---- */
    network_host_setup();

    /* ---- 3. 创建同步管道（User NS：父→子 通知 uid_map 就绪）---- */
    int sync_pipe[2];
    if (pipe(sync_pipe) != 0) die("pipe");

    /* ---- 4. 分配子进程栈 ---- */
    char *stack = malloc(STACK_SIZE);
    if (!stack) die("malloc stack");

    /* ---- 4. clone() —— 创建隔离进程 ---- */
    if (!g_cfg.no_userns)
        setgroups(0, NULL);

    int use_userns = !g_cfg.no_userns;

    int clone_flags = SIGCHLD
                    | CLONE_NEWPID
                    | CLONE_NEWNS
                    | CLONE_NEWUTS
                    | CLONE_NEWIPC
                    | CLONE_NEWCGROUP;

    if (use_userns)
        clone_flags |= CLONE_NEWUSER;

    if (g_cfg.net_isolate) {
        clone_flags |= CLONE_NEWNET;
    }

    if (g_cfg.verbose) {
        printf("[INFO] clone() flags: 0x%x\n", clone_flags);
        printf("[INFO] 创建子进程...\n");
    }

    /* 无 user NS 时传 -1，子进程跳过同步 */
    int sync_fd_arg = use_userns ? sync_pipe[0] : -1;

    pid_t child = clone(child_setup,
                        stack + STACK_SIZE,
                        clone_flags,
                        (void *)(intptr_t)sync_fd_arg);

    if (child == -1) {
        cgroup_cleanup(&g_cfg);
        free(stack);
        die("clone");
    }

    close(sync_pipe[0]);

    if (g_cfg.verbose) {
        printf("[INFO] 子进程 PID=%d\n\n", child);
    }

    if (use_userns) {
        /* ---- 4.5 V3: 写 uid_map / gid_map ---- */
        setup_user_ns(child);

        /* ---- 4.6 通知子进程继续 ---- */
        char go = 'g';
        if (write(sync_pipe[1], &go, 1) != 1) {
            fprintf(stderr, "[WARN] 通知子进程失败\n");
        }
    }
    close(sync_pipe[1]);

    /* ---- 5. 网络：移 veth + 端口映射 ---- */
    network_move_veth(child);

    /* ---- 6. cgroup ---- */
    cgroup_add_pid(cgroup_fd, child);

    /* ---- 6.5 V4: 保存容器状态 ---- */
    container_info ci;
    memset(&ci, 0, sizeof(ci));

    /* 生成容器 ID */
    if (g_cfg.container_name[0]) {
        strncpy(ci.id, g_cfg.container_name, MAX_ID_LEN - 1);
    } else {
        generate_id(ci.id, MAX_ID_LEN);
    }
    strncpy(ci.name, g_cfg.container_name[0] ? g_cfg.container_name : ci.id,
            MAX_NAME_LEN - 1);
    ci.pid        = child;
    ci.created_at = time(NULL);
    strncpy(ci.rootfs, g_cfg.rootfs, MAX_PATH_LEN - 1);
    strncpy(ci.cmd, g_cfg.cmd, MAX_CMD_LEN - 1);

    /* V6: overlay 模式 —— 预创建 upper/work/merged 目录并写 image 记录 */
    if (g_cfg.image_name[0] != '\0') {
        char upper[MAX_PATH_LEN], work[MAX_PATH_LEN], merged[MAX_PATH_LEN];
        char lower[MAX_PATH_LEN];
        container_overlay_paths(ci.id, lower, upper, work, merged,
                                sizeof(upper));
        mkdir(upper,  0755);
        mkdir(work,   0755);
        mkdir(merged, 0755);

        /* 记录用的哪个镜像（commit 时需要） */
        char img_path[CG_BUF_LEN];
        state_path(ci.id, "image", img_path, sizeof(img_path));
        write_state_file(img_path, g_cfg.image_name);
    }

    /* 检查同名容器是否已在运行 */
    container_info existing;
    if (state_load(ci.id, &existing) == 0
        && existing.pid > 0
        && kill(existing.pid, 0) == 0) {
        fprintf(stderr, "[ERROR] 容器 '%s' 已在运行 (PID=%d)\n"
                        "  请先 sudo %s stop %s 停止它，或 --name 指定其他名字\n",
                ci.id, existing.pid, g_argv0, ci.id);
        kill(child, SIGKILL);
        waitpid(child, NULL, 0);   /* 等子进程退出再释放栈 */
        network_cleanup();
        cgroup_cleanup(&g_cfg);
        free(stack);
        return 1;
    }

    /* 如果之前有已停止的同名容器，先清理旧状态 */
    state_delete(ci.id);
    state_save(&ci);

    if (g_cfg.detach) {
        printf("容器已启动: %s (PID=%d)\n", ci.id, child);
        printf("  sudo %s ps     查看容器\n", g_argv0);
        printf("  sudo %s stop %s  停止容器\n", g_argv0, ci.id);
        printf("  sudo %s exec %s \"<cmd>\"  执行命令\n", g_argv0, ci.id);
    }

    /* ---- 7. 等待子进程结束（前台模式）/ 直接退出（后台模式）---- */
    if (g_cfg.detach) {
        /* 后台模式：父进程直接退出 */
        free(stack);
        if (cgroup_fd >= 0) close(cgroup_fd);
        return 0;
    }

    /* 前台模式：注册信号转发 */
    g_child_pid = child;
    signal(SIGINT,  signal_forward);
    signal(SIGTERM, signal_forward);

    int status;
    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid");
    }
    printf("\n");

    /* ---- 8. 清理 ---- */
    network_cleanup();
    if (cgroup_fd >= 0) close(cgroup_fd);
    cgroup_cleanup(&g_cfg);
    free(stack);

    /* ---- 8.5 更新状态为已停止 ---- */
    state_update_pid(ci.id, -1);

    /* ---- 9. 返回子进程的退出码 ---- */
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "容器被信号 %d (%s) 终止\n",
                WTERMSIG(status), strsignal(WTERMSIG(status)));
        return 128 + WTERMSIG(status);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    strncpy(g_argv0, argv[0], sizeof(g_argv0) - 1);

    /* ---- 0. 检查 root 权限（所有命令都需要）---- */
    if (geteuid() != 0) {
        fprintf(stderr,
            "[ERROR] 必须用 root 权限运行。\n"
            "  请使用: sudo %s <run|ps|stop|exec|rm> ...\n", argv[0]);
        exit(1);
    }

    if (argc < 2) {
        print_usage(argv[0]);
        exit(1);
    }

    const char *subcmd = argv[1];

    /* ---- 子命令分发 ---- */

    if (strcmp(subcmd, "run") == 0) {
        parse_args(argc, argv);
        /* detach 模式下 fork 一次，让容器在后台运行 */
        if (g_cfg.detach) {
            pid_t f = fork();
            if (f == -1) die("fork");
            if (f > 0) {
                /* 父进程：写状态后立即退出 */
                printf("容器正在后台启动...\n");
                /* run_container 在子进程中调用，子进程会写 state */
                exit(0);
            }
            /* 子进程继续执行 run_container */
            /* 脱离终端 */
            setsid();
        }
        return run_container();

    } else if (strcmp(subcmd, "ps") == 0) {
        int verbose = (argc > 2 && (strcmp(argv[2], "-v") == 0 ||
                                     strcmp(argv[2], "--verbose") == 0));
        return cmd_ps(verbose);

    } else if (strcmp(subcmd, "stop") == 0) {
        const char *id = (argc >= 3) ? argv[2] : "default";
        return cmd_stop(id);

    } else if (strcmp(subcmd, "exec") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: sudo %s exec [<id>] <command>\n", argv[0]);
            exit(1);
        }
        /* exec <id> <cmd>  或  exec <cmd>（id 默认 default） */
        if (argc >= 4) {
            return cmd_exec(argv[2], argv[3]);
        } else {
            return cmd_exec("default", argv[2]);
        }

    } else if (strcmp(subcmd, "rm") == 0) {
        const char *id = (argc >= 3) ? argv[2] : "default";
        return cmd_rm(id);

    } else if (strcmp(subcmd, "import") == 0) {
        if (argc < 4) {
            fprintf(stderr, "用法: sudo %s import <tarball.tar.gz> <name>\n",
                    argv[0]);
            exit(1);
        }
        return cmd_import(argv[2], argv[3]);

    } else if (strcmp(subcmd, "images") == 0) {
        return cmd_images();

    } else if (strcmp(subcmd, "rmi") == 0) {
        if (argc < 3) {
            fprintf(stderr, "用法: sudo %s rmi <name>\n", argv[0]);
            exit(1);
        }
        return cmd_rmi(argv[2]);

    } else if (strcmp(subcmd, "commit") == 0) {
        if (argc < 4) {
            fprintf(stderr, "用法: sudo %s commit <id> <name>\n", argv[0]);
            exit(1);
        }
        return cmd_commit(argv[2], argv[3]);

    } else {
        print_usage(argv[0]);
        exit(1);
    }
}
