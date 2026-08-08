#include <sys/inotify.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdio.h>

int main() {
    // 创建 inotify 实例
    int inotify_fd = inotify_init();   //注意这里初始化的时候是生成了一个fd
    int inotify_wd = inotify_add_watch(inotify_fd, "/data/test_inotify_dir", IN_MODIFY | IN_CREATE |  
    IN_DELETE | IN_DELETE_SELF | IN_MOVE);  //添加一个watch事件

    // 创建 epoll 实例
    int epoll_fd = epoll_create(1);
    struct epoll_event event, events[2];

    // 将 inotify_fd 注册到 epoll
    event.events = EPOLLIN;
    event.data.fd = inotify_fd;  //这恶FD可以
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd, &event);

    // // 将 socket_fd（假设已创建）注册到 epoll
    // event.data.fd = socket_fd;
    // epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &event);  //这里增加了一个

    // 事件循环
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, 2, -1);
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == inotify_fd) {
                // 处理 inotify 事件
                char buffer[1024]; 
                int length = read(inotify_fd, buffer, sizeof(buffer));  //这个进程应该是挂起来了
                if (length < 0) {
                    printf("read error");
                    exit(-1);
                }
                printf("read size %d\n",length);
                for (char *ptr = buffer; ptr < buffer+length;) {
                    struct inotify_event *event = (struct inotify_event *)ptr;
                    if(event->len > 0){
                        printf("File:%-16s ", event->name);
                    }
                    if(event->mask & IN_CREATE) {
                        printf("[CREATE]");
                    }
                    if(event->mask & IN_DELETE) {
                        printf("[DELETE]");
                    } 
                    if(event->mask & IN_MODIFY) {
                        printf("[MODIFY]");
                    }
                    if (event->mask & IN_MOVED_TO) {
                        printf("[MOVED_IN]");
                    } 
                    if (event->mask & IN_MOVED_FROM) {
                        printf("[MOVED_OUT]");
                    }
                    printf("\n");
                    ptr += sizeof(struct inotify_event) + event->len; 
                } 
            } 
            // else if (events[i].data.fd == socket_fd) {  //这里可以进行socket fd的操作
            //     // 处理 socket 事件
            //     char buffer[1024];
            //     int len = read(socket_fd, buffer, sizeof(buffer));
            // }
        }
    }
    inotify_rm_watch(inotify_fd, inotify_wd);
    close(inotify_fd);
    close(epoll_fd);
    return 0;
}
