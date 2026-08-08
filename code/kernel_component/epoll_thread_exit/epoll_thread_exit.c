#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h> 
#include <sys/wait.h>
#include <string.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#define MAX_EVENTS 10
#define MAX_PIDS 128

int main(int argc, char *argv[]) {
    
    //目标进程pid
    pid_t  pids[MAX_PIDS] = {0};
    //pidfd，这个个数和上面的要一致
    pid_t pidfds[MAX_PIDS] = {0};
    int count = 0;
     if (argc < 2) {
        printf("Usage: %s <pid1> <pid2> ...\n", argv[0]);
        return 1;
    }
    for (int i = 1; i < argc && count < MAX_PIDS; i++) {
        char *endptr;
        long pid = strtol(argv[i], &endptr, 10);

        // 检查是否为合法整数
        if (*endptr != '\0') {
            printf("Invalid PID: %s\n", argv[i]);
            continue;
        }

        // 检查是否在合法范围内（0 < pid <= PID_MAX，通常为 4194303）
        if (pid <= 0 || pid > 4194303) {
            printf("PID out of range: %s\n", argv[i]);
            continue;
        }

        pids[count++] = (pid_t)pid;
        printf("Added PID: %d\n", (int)pid);
    }
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1\n");
        return 1;
    }
    int ep_success_count  = 0;
    for(int i = 0 ;i < count ;i ++) {
        printf("current pid is:%d\n", pids[i]);
        int pidfd = syscall(SYS_pidfd_open, pids[i], 0);
        if (pidfd < 0) {
            printf("pid_fd_open error for :%d\n", pids[i]);
            close(pidfd);
            continue;
        }
        struct epoll_event ev= {
            .events = EPOLLIN | EPOLLET,//采用边缘触发
            .data.fd = pidfd,
        };
        if(epoll_ctl(epfd, EPOLL_CTL_ADD, pidfd, &ev) < 0){
            printf("EPOLL_CTL_ADD ERROR\n");
            close(pidfd);
            continue;  //跳过了
        }
        printf("EPOLL_CTL_ADD Success\n");
        pidfds[i] = pidfd;
        ep_success_count += 1;
    }

    int exit_count = 0;
    printf("{exit:%d, ep_success:%d}\n",exit_count, ep_success_count);
    while (exit_count < ep_success_count)
    {
        struct epoll_event events[MAX_EVENTS];
        int n =  epoll_wait(epfd, events, MAX_EVENTS, 1);
        if (n <= 0) {
            continue;
        }
        for(int i = 0  ;i < n ; i++){
            int fd = events[i].data.fd;
            printf("process with pidfd %d has exited %d\n",fd,exit_count);
            exit_count ++;
            printf("{exit:%d, ep_success:%d}\n",exit_count, ep_success_count);
            for(int j = 0 ;j < count; j++)
            {
                if(pidfds[j] == fd && pidfds[j] > 0){
                    printf("close i:%d\n",j);
                    close(pidfds[j]);
                }
            }
        } 
    }

    printf("begin clear\n");
    close(epfd);
    for(int i = 0 ;i < count; i++)
    {
        if(pidfds[i] > 0){
            close(pidfds[i]);
        }
    }
    return 0;
}