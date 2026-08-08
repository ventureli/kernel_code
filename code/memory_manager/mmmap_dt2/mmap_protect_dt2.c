#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <malloc.h>
#include <errno.h>
#include<fcntl.h>



#define handle_error(msg) do{ perror(msg);exit(EXIT_FAILURE);}while(0)

static char *buffer;
static void handler(int sig, siginfo_t *si, void *unused) {
    printf("Get SIIGSEGV  at address;%p\n", si->si_addr);
    exit(EXIT_FAILURE);
}

 

int main(int argc, char *argv[]) {

    int page_size;
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = handler;

    if(sigaction(SIGSEGV, &sa, NULL) == -1) {
        handle_error("sigaction");
    }

    page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size == -1) {
        handle_error("sysconf");
    }
    //分配一个页面大小
    buffer  = memalign(page_size, 4*page_size);

    if (buffer == NULL) {
        handle_error("memalign");
    }
    printf("start of page_size:%d region:%p\n",page_size,buffer);
    if (mprotect(buffer + page_size *2, page_size, PROT_READ) == -1)  {
        handle_error("mprotect");
    }
    for (char *p = buffer;; ) {
        *(p++) = 'A';
      printf("start of page_size:%d start:%p p:%p\n",page_size, buffer, p);
        
    }
    printf("for complete");
    exit(EXIT_SUCCESS);

}