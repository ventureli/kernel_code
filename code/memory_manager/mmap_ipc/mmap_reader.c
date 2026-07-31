#include "ipc_mmap.h"

int main(){
    
 
    int fd =  open(SHARED_FILE , O_RDWR, 0666);
    if (fd == -1) {
        perror("open failed");
        exit(1);
    }

    //映射文件到进程虚拟地址空间
    ShareData *shared_data = (ShareData *)mmap(
        NULL,sizeof(ShareData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fd, 0
    );
    

    if (shared_data == MAP_FAILED) {
        perror("mmap failed 1");
        close(fd);
        exit(1);
    }
    key_t sem_key = ftok(SHARED_FILE, 1);
    int sem_id  =  semget(sem_key, 1, 0666);
    if (sem_id == -1) {
        perror("get sem_id failed");
        close(fd);
        exit(1);
    }
    close(fd);
    
    //4.循环读数据
    while (1) {
        //申请信号量
        sem_p(sem_id);
        //检查数据
        if (shared_data->flag == 1) {
            //get 
            printf("receive message:%s\n",shared_data->msg);
            if (strcmp(shared_data->msg ,"q") == 0 ) {
                sem_v(sem_id);
                break;
            }
            shared_data->flag = 0;
        }
        sem_v(sem_id);
        usleep(100000);  //100ms query一次
    }
    munmap(shared_data,sizeof(ShareData));
    return 0;
}