#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include<unistd.h>
#include<fcntl.h>
typedef struct {  
    char name[6];
    int age;
}people;


int main(int argc, char *argv[]) {

    int fd, i;
    people *user_1;
    fd = open(argv[1], O_RDWR , 007777);
    //MAP_SHARED 代表进程共享区间
    user_1 = (people *) mmap(NULL, sizeof(people)* 10, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (user_1 == (void *)-1) {
        fprintf(stderr, "mmap:%s \n", strerror(errno));
        close(fd);
        return  -1;
    }
    close(fd);
    for (int i = 0; i < 10; i++) {
         printf("name :%s age:%d \n",(*(user_1 + i)).name, (*(user_1 + i)).age);
    } 
    munmap(user_1, sizeof(people) * 10);
    return 0;

}