#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

int main(int argc, char *argv[]) {
    int fd;
    struct input_event ev;
    if (argc < 2) {
        printf("usage:%s /dev/input/eventX\n",argv[0]);
        return 0;
    }
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    printf("read_event: opend %s, waiting...\n",argv[1]);
    while(read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
        // if (ev.type == EV_KEY && ev.code == KEY_A) {
            printf("PID %d:value:%d code:%d type:%d\n", getpid(), ev.value, ev.code, ev.type );
        // }
    }
    close(fd);
    return 0;
}
