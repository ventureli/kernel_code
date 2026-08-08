#include <sys/inotify.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    //初始化 inotify
    printf("hello world 1\n");
    int fd = inotify_init();
    if(fd < 0) {
        perror("inotify_init error");
        exit(-1);
    }
    // //添加监控
    
    printf("hello world 2\n");
    const char *path = "/data/test_inotify_dir";  //这个只能看到当前路径，无法看到绝对路径，/data/下某些东西
    int wd = inotify_add_watch(fd, path, IN_MODIFY | IN_CREATE |  
    IN_DELETE | IN_DELETE_SELF | IN_MOVE);
    if(wd < 0) {
        perror("inotify_add_watch");
        close(fd);
        exit(-1);        
    }
    
    printf("hello world 3\n");
    // //事件循环
    printf("I whill begin watch /data/test_inotify_dir\n");
    char buffer[1024];
    while (1) { 
        printf("hello world 4\n");
        int length = read(fd, buffer, sizeof(buffer));  //这个进程应该是挂起来了
        
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
}
