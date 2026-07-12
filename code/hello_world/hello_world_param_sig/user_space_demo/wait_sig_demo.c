

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SIG_CUSTOM_MSG 42

void sig_handler(int sig, siginfo_t *info, void *ucontext)
{
    printf("收到自定义信号：%d\n", sig);
    printf("发送进程PID：%d\n", info->si_pid);
    printf("发送进程UID：%d\n", info->si_uid);
    printf("信号来源类型：%d\n", info->si_code);
}  

int main()
{
    struct sigaction act;
    //清空
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_SIGINFO; //开启siginfo附加信息
    act.sa_sigaction = sig_handler; //使用带有siginfo的回调
    //绑定回调
    if (sigaction(SIG_CUSTOM_MSG,&act, NULL) < 0) {
        perror("sigaction fail!");
        return -1;
    }
    printf("process %d is waiting signal:%d...\n", getpid(), SIG_CUSTOM_MSG);
    while (1) {
        pause(); //阻塞等待信号
    }
    return 0;
}








/**简单版 回调 示例如下
 * 
 * 
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

void handler(int signo) {
    printf("\nCaught signal %d: ", signo);
    exit(0);
}

int main() {
    signal(SIGINT, handler);  // 捕获 Ctrl+C 信号
    while (1) {
        printf("Running...\n");
        sleep(1);
    }
    return 0;
}
 * 
 */