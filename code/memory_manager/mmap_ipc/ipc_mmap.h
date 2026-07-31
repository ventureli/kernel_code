
#ifndef IPC_MAP_H
#define IPC_MAP_H

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/mman.h>
#include<sys/ipc.h>
#include<sys/sem.h>
#include<string.h>

#define SHARED_FILE "shared_data.dt"
#define DATA_SIZE 256

typedef struct {
    char msg[DATA_SIZE];
    int flag;
}ShareData;

//信号量操作：P，V
union semun{
    int val;
    struct semid_ds *buffer;
    unsigned short *array;
};

static int sem_init(key_t key){
    int sem_id = semget(key,1,IPC_CREAT| 0666);
    if(sem_id == -1)
    {
        perror("semget failed");
        exit(1);
    }
    union semun sem_union;
    sem_union.val = 1;
    semctl(sem_id ,0, SETVAL,sem_union );
    return sem_id;
}

//P操作
static void  sem_p(int sem_id){
    struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = -1;
    sem_b.sem_flg  = SEM_UNDO;
    semop(sem_id, &sem_b, 1);
}
//V操作
static void sem_v(int sem_id){
     struct sembuf sem_b;
    sem_b.sem_num = 0;
    sem_b.sem_op = 1;
    sem_b.sem_flg  = SEM_UNDO;
    semop(sem_id, &sem_b, 1);
}

#endif

