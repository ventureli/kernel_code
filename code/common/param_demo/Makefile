CC      = gcc
CFLAGS  = -Wall -g -O2
LDFLAGS = -lrt    # shm_open 在老 glibc 需要 -lrt（新 glibc 已并入 libc，带着也不碍事）

all: my_param my_param_server

my_param_server: my_param_server.c my_param_common.h
	$(CC) $(CFLAGS) -o $@ my_param_server.c $(LDFLAGS)

my_param: my_param.c my_param_common.h
	$(CC) $(CFLAGS) -o $@ my_param.c $(LDFLAGS)

clean:
	rm -f my_param my_param_server

.PHONY: all clean
