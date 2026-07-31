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
    char temp ='A';
    fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 007777);
    lseek(fd, sizeof(people) * 10 -1 , SEEK_SET);
    write(fd, "", 1);

    //MAP_SHARED 代表进程共享区间
    user_1 = (people *) mmap(NULL, sizeof(people)* 10, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (user_1 == (void *)-1) {
        fprintf(stderr, "mmap:%s \n", strerror(errno));

        close(fd);
        return -1;
    }
    close(fd);
    for (int i = 0; i < 10; i++) {
        temp = temp + 1;
        (*(user_1 + i)).name[1] = '\0';
        memcpy( (*(user_1 + i)).name, &temp, 1);
        (*(user_1 + i)).age = 30;
    }
    printf("Initialize. \n");
    sleep(20);
    munmap(user_1, sizeof(people) * 10);
    printf("UMA OK.\n");
    return 0;

}