#include "ipc_mmap.h"
#include <pthread.h>
#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include <assert.h>
 
 int gExit = 0;
 typedef struct {
    ShareData * shared_data;
    int sem_id;
    int self_flag;
 }ThreadParam;

 void writeFun(void *arg) {
    ThreadParam *param = (ThreadParam *)arg;
    ShareData * shared_data = param->shared_data;
    int sem_id = param->sem_id;
    int self_flag = param->self_flag;
    char input[DATA_SIZE];
    printf("begin now flag is[%d]:%s \n", shared_data->flag, shared_data->msg);
    while (1 && gExit == 0 ) {
       
        //读数据
        sem_p(sem_id);
        if(shared_data->flag != 0){ 
        //当前进程的数据
            if (shared_data->flag != self_flag) {
                //get 
                //not print
                printf("revice message[%d]:%s \n", shared_data->flag, shared_data->msg);
                //读取到数据显示开始写
                shared_data->flag = 0;
            } else {
                // printf("self message %d \n", shared_data->flag);
                sem_v(sem_id);
                usleep(100000);
                continue; //继续等待别人读取后
            }  
        } //==0 说明谁都没写对吧，直接写
         
        printf("please enter message(enter q to exit):");
        fgets(input, DATA_SIZE, stdin);
        input[strcspn(input, "\n")] = '\0'; //去掉换行符
        //申请信号量所
        strcpy(shared_data->msg,input);
        shared_data->flag = self_flag; //标记数据就绪
        printf("waiting for response ...\n");
        sem_v(sem_id); //   
        
        usleep(100000);//继续探测  
    }
}
 
int main(){
    int fd =  open(SHARED_FILE , O_RDWR | O_CREAT , 0666);
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

    pid_t pid  = getpid();
    
    
    pthread_t ppid_read, ppid2_write;
    struct sched_param sParam;
    pthread_attr_t pattr_read,pattr_write;
    pthread_attr_init(&pattr_read);
    pthread_attr_init(&pattr_write); 
 
 //构造参数
    ThreadParam param;
    param.self_flag = pid;
    param.sem_id = sem_id;
    param.shared_data = shared_data;

    // pthread_create(&ppid_read,&pattr_read,(void*)readFun,&param);
    pthread_create(&ppid2_write,&pattr_write,(void*)writeFun,&param);

    // pthread_join(ppid_read,NULL);
    pthread_join(ppid2_write,NULL);
 

    munmap(shared_data,sizeof(ShareData));
    //删除信号量
    semctl(sem_id, 0, IPC_RMID);
    return 0;
}