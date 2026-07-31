

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SIG_CUSTOM_MSG 42

 

int main()
{
    
    static int count = 0;
    while (1) {
        printf("now count is :%d\n", count++);
        usleep(100000);
    }
    return 0;
}






 