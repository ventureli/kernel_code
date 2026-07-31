#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define  MAX_SIZE  1024

int main(int argc, char *argv[]) {
    int *current_point  = sbrk(0);
    int *old = current_point;
    current_point = sbrk(MAX_SIZE * MAX_SIZE);
    if (current_point == (void *)(-1)) {
        perror("sbrk error");
        exit(1);
    }

    printf("old: %p \np=%p \n", old, current_point);
    int *new_value = sbrk(0);
    printf("new:%p\n",new_value);
    printf("pid:%d\n",getpid());


    // while(1);
    sbrk(-MAX_SIZE * MAX_SIZE);  //释放内存

    // int *new_value2 = sbrk(0);
    // printf("new:%p\n",new_value2s);
    sleep(10);
    return 0;
}

//通过 cat /proc/9451/maps 可以查看对一个的的mmap情况