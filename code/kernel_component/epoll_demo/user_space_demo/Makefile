CC := gcc
CFLAGS := -Wall -g -Iinclude  #静态链接会很大
#不加 -static 会很小
#CFLAGS := -Wall -g -Iinclude  
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
TARGET := $(BIN_DIR)/epoll_user_space
EXF := $(notdir $(TARGET))


SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all info clean

#for crossfile 
KDIR ?= /home/dell/code/6.0/out/taihang2102/kernel/linux-5.10
CROSS_COMPILE ?= /home/dell/code/6.0/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-7.5.0-2019.12-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
CLANG_CC ?= /home/dell/code/6.0/prebuilts/clang/ohos/linux-x86_64/llvm/bin/aarch64-unknown-linux-ohos-clang
CLANG_CXX ?= /home/dell/code/6.0/prebuilts/clang/ohos/linux-x86_64/llvm/bin/aarch64-unknown-linux-ohos-clang++
for_platform ?= phone
SYS_ROOT ?= /home/dell/code/6.0/out/taihang2102/obj/third_party/musl


ifeq ($(for_platform), phone)
	DSTARCH=arm64
	CFLAGS += -I/$(KDIR)/arch/arm64/include -I/$(SYS_ROOT)/usr/include --sysroot=$(SYS_ROOT) --target=aarch64-linux-ohos
	LDFLAGS += --sysroot=$(SYS_ROOT) -L$(SYS_ROOT)/usr/lib -Wl,-rpath=/usr/lib -Wl,--dynamic-linker=/lib/ld-musl-aarch64.so.1
	CC := $(CLANG_CC)
else 
	CC := gcc
endif
info:
	@echo "for_platform:"$(for_platform)
	@echo "Source list:"$(SRCS)
	@echo "Object list:"$(OBJS)

all:$(TARGET)

$(TARGET): $(OBJS)
	@echo "Source list:"$(SRCS)
	@echo "Object list:"$(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


send:
	hdc.exe shell mkdir  /data/test_bin
	hdc.exe file send   $(TARGET) /data/test_bin
	hdc.exe shell "chmod +x /data/test_bin/$(TARGET)"
	@echo "----------------  $(TARGET)  --------------------"

test:
	@make
	hdc.exe shell "/data/test_bin/$(TARGET)"
	@echo "------------------------------------"

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

