
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <sys/epoll.h>

#define DEVICE "/dev/epoll_char_dev"

int main() {
    int fd = open(DEVICE, O_RDWR| O_NONBLOCK);
    if(fd < 0) {
        perror("open");
        return 1;
    }
    //写入数据
    const char *msg =  "Hello from user Space !\n";
    write(fd, msg, strlen(msg));
    printf("Wrote :%s",msg);

    //使用epoll进行等待
    int epfd = epoll_create1(0);
    if(epfd < 0){
        perror("epoll_create1 error1");
        close(fd);
        return 1;
    }
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epol_ctl");
        close(fd);
        close(epfd);
        return 1;
    }

    struct epoll_event events[1];
    while(1) {
        printf("Waiting for data.....\n");
        int n = epoll_wait(epfd, events,1, -1);
        if(n < 0){
            perror("epoll_wait");
            close(fd);
            close(epfd);
            return 1;
        }
        if(events[0].events & EPOLLIN){
            char buffer[128];
            int len = read(fd,buffer,sizeof(buffer)-1);
            buffer[len] = '\0';
            printf("read from device:%s(over)\n",buffer);
            if(strcmp(buffer,"exit\n") == 0){
                printf("go exit for :`%s` cmd",buffer);
                break;
            }
        }

    }
    
    close(fd);
    close(epfd);
    return 0;


}