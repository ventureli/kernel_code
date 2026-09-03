NAME := mycontainer
CC   := gcc

CFLAGS  := -Wall -Wextra -O2
LDFLAGS :=

SRC := container.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean rootfs

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c container.h
	$(CC) $(CFLAGS) -c -o $@ $<

# 下载最小的 alpine rootfs（aarch64）
rootfs:
	@echo "==> 下载 Alpine minirootfs (aarch64) ..."
	wget -O /tmp/alpine-rootfs.tar.gz \
		https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/aarch64/alpine-minirootfs-3.21.3-aarch64.tar.gz
	@echo "==> 解压到 ./alpine-rootfs/"
	mkdir -p ./alpine-rootfs
	sudo tar xf /tmp/alpine-rootfs.tar.gz -C ./alpine-rootfs
	@echo "==> 完成。rootfs 在 ./alpine-rootfs/"

clean:
	rm -f $(NAME) $(OBJ)

distclean: clean
	sudo rm -rf ./alpine-rootfs /tmp/alpine-rootfs.tar.gz
