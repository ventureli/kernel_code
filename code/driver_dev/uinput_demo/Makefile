BIN_NAME := uinput_demo

# ===== 按你的编译机改这三行（默认跟你 code/ 下其它 demo 一致）=====
# OH 源码根目录
OHOS_ROOT ?= /home/dell/code/6.0
# 目标设备（taihang2102 / taihang1011）
PRODUCT ?= taihang2102
# hdc 命令：Windows 下是 hdc.exe，Linux 下是 hdc
HDC ?= hdc.exe
# ============================

# 交叉工具链：OH 用户态用 clang + musl（不是 gcc + glibc）
CLANG_CC ?= $(OHOS_ROOT)/prebuilts/clang/ohos/linux-x86_64/llvm/bin/aarch64-unknown-linux-ohos-clang

# musl sysroot
SYS_ROOT ?= $(OHOS_ROOT)/out/$(PRODUCT)/obj/third_party/musl

# 内核 uapi 头：提供 linux/input.h / linux/uinput.h（musl 里没有）
KDIR ?= $(OHOS_ROOT)/out/$(PRODUCT)/kernel/linux-5.10

for_platform ?= phone

SRC_DIR := .
OBJ_DIR := obj
BIN_DIR := bin
TARGET  := $(BIN_DIR)/$(BIN_NAME)

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

CC      := gcc
CFLAGS  := -Wall -g
LDFLAGS :=

ifeq ($(for_platform), phone)
	CC := $(CLANG_CC)
	CFLAGS += --sysroot=$(SYS_ROOT) --target=aarch64-linux-ohos \
	          -I$(KDIR)/include/uapi -I$(KDIR)/arch/arm64/include/uapi
	LDFLAGS += --sysroot=$(SYS_ROOT) -L$(SYS_ROOT)/usr/lib \
	           -Wl,-rpath=/usr/lib -Wl,--dynamic-linker=/lib/ld-musl-aarch64.so.1
endif

.PHONY: all clean send run info

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 推送到设备
send: all
	$(HDC) shell mkdir -p /data/test_bin
	$(HDC) file send $(TARGET) /data/test_bin
	$(HDC) shell "chmod +x /data/test_bin/$(BIN_NAME)"

# 推送到设备并运行（uinput 需要 root，OH 工程版 hdc shell 默认就是 root）
run: send
	$(HDC) shell "/data/test_bin/$(BIN_NAME)"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

info:
	@echo "CC       = $(CC)"
	@echo "SYS_ROOT = $(SYS_ROOT)"
	@echo "KDIR     = $(KDIR)"
	@echo "HDC      = $(HDC)"
	@echo "SRCS     = $(SRCS)"
