#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdbool.h>
int futex_var = 0;

void lock() {
    int expected;
    while(1) {
        // 竞争失败，等待
        expected = 0;
        if (__atomic_compare_exchange_n(&futex_var, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            return;
        }
        syscall(SYS_futex, &futex_var, FUTEX_WAIT, 1, NULL, NULL, 0);
    }

}

void unlock() {
    atomic_store(&futex_var, 0);
    syscall(SYS_futex, &futex_var, FUTEX_WAKE, 1, NULL, NULL, 0);
}

void* thread_function(void* arg){
    char *prefix = (char *)arg;
    for (int i = 0; i < 1000; i++) {
        lock();
        printf("%s --- begin ---%d \n", prefix, i);
        usleep(10000); //1ms
        printf("%s --- end ---%d \n\n", prefix, i);
        unlock();
        usleep(1000); //1ms
    }
    return NULL;
}


int main() {
    pthread_t thread_id1 , thread_id2;
    char *arg = "T1";
    //create
    if (pthread_create(&thread_id1, NULL, thread_function,arg) != 0) {
        perror("Failed To Create T1");
        return 1;
    }
    arg = "T2";
    //create
    if (pthread_create(&thread_id2, NULL, thread_function,arg) != 0) {
        perror("Failed To Create T2");
        return 1;
    }

    pthread_join(thread_id1,NULL); //pthread_detech //分离线程
    pthread_join(thread_id2,NULL);
    printf("Main Thread OK");
    return 0;
}
