#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/wait.h>

#define CGROUP_NAME "my_cgroup"
#define MAX_ALLOC 128 * 1024 * 1024 //128M

//运行时探测到的 cgroup 信息
static int  use_v2 = 0;        //1: cgroup v2(统一层级)  0: cgroup v1
static char cgroup_root[64];   //memory 控制器所在层级的挂载点
static char cgroup_path[128];  //my_cgroup 的完整路径

//写入Cgroup
int cgroup_write(const char * path , const char *value) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        fprintf(stderr, "open %s failed: %s\n", path, strerror(errno));
        return -1;
    }
    int len =  write(fd, value, strlen(value));
    if (len < 0) {
        fprintf(stderr, "write %s failed: %s\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

//探测 memory cgroup 挂在哪里、是 v1 还是 v2
//5.10(Android 12/老发行版): v1, /dev/memcg 或 /sys/fs/cgroup/memory
//5.15(Android 13+/Ubuntu 22.04 等): 默认 v2 统一层级, /sys/fs/cgroup

int detect_cgroup() {
    if (access("/dev/memcg/memory.limit_in_bytes", F_OK) == 0) {
        strcpy(cgroup_root, "/dev/memcg");                 //Android cgroup v1
        use_v2 = 0;
    } else if (access("/sys/fs/cgroup/memory/memory.limit_in_bytes", F_OK) == 0) {
        strcpy(cgroup_root, "/sys/fs/cgroup/memory");      //发行版 cgroup v1
        use_v2 = 0;
    } else if (access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0) {
        strcpy(cgroup_root, "/sys/fs/cgroup");             //cgroup v2
        use_v2 = 1;
    } else {
        fprintf(stderr, "no memory cgroup hierarchy found\n");
        return -1;
    }
    snprintf(cgroup_path, sizeof(cgroup_path), "%s/%s", cgroup_root, CGROUP_NAME);
    printf("using %s (cgroup %s)\n", cgroup_path, use_v2 ? "v2" : "v1");
    return 0;
}

//把当前进程挪回根 cgroup（写 0 到根层级的 cgroup.procs）
int leave_cgroup() {
    char path[192];
    snprintf(path, sizeof(path), "%s/cgroup.procs", cgroup_root);
    return cgroup_write(path, "0");
}

//创建CGroup
int create_cgroup() {
    if (access(cgroup_path, F_OK) == 0) {
        //如果存在，先把自己挪回根层级再删除
        //注意：要写根的 cgroup.procs；写 my_cgroup/cgroup.procs 是把自己加进去，rmdir 反而会 EBUSY
        leave_cgroup();
        if (rmdir(cgroup_path) < 0) {
            perror("rmdir error");
            return  -1;
        }
    }

    if(mkdir(cgroup_path, 0755) < 0) {
        perror("mkdir");
        return -1;
    }
    //V2 版本需要在父级手动启用内存控制器（根上一般已默认开启，重复写无害）
    if (use_v2) {
        char path[192];
        snprintf(path, sizeof(path), "%s/cgroup.subtree_control", cgroup_root);
        if (cgroup_write(path, "+memory") < 0) {
            return -1;
        }
    }
    return 0;
}

//设置内存限制（单位，字节）
int set_memory_limit(unsigned long limit) {
    char limit_str[32];
    snprintf(limit_str, sizeof(limit_str), "%lu", limit);
    char path[192];
    //v1: memory.limit_in_bytes   v2: memory.max
    snprintf(path, sizeof(path), "%s/%s", cgroup_path,
             use_v2 ? "memory.max" : "memory.limit_in_bytes");
    if (cgroup_write(path, limit_str) < 0) {
        return -1;
    }
    //v2 下超限会先往 swap 挤，关掉 swap 才能稳定触发 OOM（失败不致命）
    if (use_v2) {
        snprintf(path, sizeof(path), "%s/memory.swap.max", cgroup_path);
        cgroup_write(path, "0");
    }
    return 0;
}


//加入group
int join_cgroup(pid_t pid) {
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    char path[192];
    snprintf(path, sizeof(path), "%s/cgroup.procs", cgroup_path);

    return cgroup_write(path, pid_str);
}

//测试内存限制

void test_memory_limit(){
    printf("Starting memory test (allocation 128M)...\n");
    char *p  = malloc(MAX_ALLOC);
    if (!p) {
        perror("maloc");
        return;
    }
    memset(p,0,MAX_ALLOC);
    for (int i = 0 ; i < 100; i++ ){
        p[i] = 'A';
    }

    for (int i = 100000 ; i < 100 + 100000; i++ ){
        p[i] = 'b';
    }
    printf("Memory allocated :%d MB\n", MAX_ALLOC/1024/1024);
    sleep(1000);
    free(p);
}

int main() {

    if(geteuid() != 0) {
        fprintf(stderr, "This is program must be run as root\n");
        return 1;
    }

    if(detect_cgroup() < 0) {
        return 1;
    }

    if(create_cgroup() < 0) {
        return 1;
    }

    if (set_memory_limit(100 * 1024 * 1024) < 0) {
        return 1;
    }
    pid_t  pid = getpid();
    printf("pid is:%d\n",pid);
    if (join_cgroup(pid) < 0 ) {
        return 1;
    }
    test_memory_limit();
    //清理：先把自己挪回根 cgroup，再删除 my_cgroup
    if (leave_cgroup() < 0) {
        return 1;
    }
    if (rmdir(cgroup_path) < 0) {
        perror("rmdir");
    }
    return 0;
}
