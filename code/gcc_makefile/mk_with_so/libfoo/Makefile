CC := gcc
#fPIC 位置无关代码，比如加入才能编译出来.so
CFLAGS := -Wall -fPIC -I.
TARGET := libfoo.so
SRCS := foo.c
OBJS := $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f *.o $(TARGET)