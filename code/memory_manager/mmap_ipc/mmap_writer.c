#include "ipc_mmap.h"

 
int main(){
    int fd =  open(SHARED_FILE , O_RDWR | O_CREAT | O_TRUNC , 0666);
    if (fd == -1) {
        perror("open failed");
        exit(1);
    }
    ShareData init_data = {0};
    if (write(fd,&init_data , sizeof(ShareData)) != sizeof(ShareData))
    {
        perror("write failed");
        close(fd);
        exit(1);
    }
    //3. 映射文件到进程虚拟地址空间
    ShareData * shared_data = (ShareData *)mmap(
        NULL, sizeof(ShareData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED, fd,0
    );
    if (shared_data == MAP_FAILED)
    {
        perror("mmap failed");
        close(fd);
        exit(1);
    }
    close(fd); //映射后文件描述符可以关闭了。
    //初始化信号量操作

    key_t sem_key = ftok(SHARED_FILE, 1);
    int sem_id  =  sem_init(sem_key);
    char input[DATA_SIZE];
    while (1) {
        printf("please enter message(enter q to exit):");
        fgets(input, DATA_SIZE, stdin);
        input[strcspn(input, "\n")] = '\0'; //去掉换行符
        //申请信号量所
        sem_p(sem_id);
        strcpy(shared_data->msg,input);
        shared_data->flag = 1; //标记数据就绪
        sem_v(sem_id);
        if(strcmp(input ,"q") == 0)
        {
            break;
        }
        usleep(100000);//模拟延迟 500ms
        // sleep(1);
    }

    munmap(shared_data,sizeof(ShareData));
    //删除信号量
    semctl(sem_id, 0, IPC_RMID);
    return 0;
}