#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>


#define MY_IOCTL_MAGIC 'k'
#define MY_IOCTL_HELLO _IO(MY_IOCTL_MAGIC, 0)
#define MY_IOCTL_ECHO _IOWR(MY_IOCTL_MAGIC, 1, int)

struct ioctl_msg {
    int value;
    char msg[64];
    int flag;
};

int main() {
    int fd =  open("/dev/ioctl_dev", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    //调用无参数命令
    if(ioctl(fd, MY_IOCTL_HELLO) < 0){
        perror("ioctl MY_IOCTL_HELLO");
    }
    //调用带有参数的命令
    struct ioctl_msg msg ={
        .value = 2,
        .msg = "hello world",
        .flag = 0
    };
    // int value = 2;
    if (ioctl(fd, MY_IOCTL_ECHO , &msg) < 0) {
        perror("ioctl MY_IOCTL_ECHO");
    }else{
        printf("kernel return : {%d %s %d}\n",msg.value, msg.msg, msg.flag);
    }
    close(fd);
    return 0;
}