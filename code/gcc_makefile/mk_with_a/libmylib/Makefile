CC := gcc
AR := ar
CFLAGS := -Wall -g -I.

 #lib name
LIB := libmylib.a

## C
# SRCS := $(wildcard $(SRC_DIR)/*.c)
SRCS := foo.c bar.c
#替换语句

OBJS := $(SRCS:.c=.o)

.PHONY: all info clean
all : $(LIB)


$(LIB):$(OBJS)
	@echo "Source list:"$(SRCS)
	@echo "Object list:"$(OBJS)
	$(AR) rcs $@ $^
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf *.o $(LIB)


info:
	@echo "Source list:"$(SRCS)
	@echo "Object list:"$(OBJS)
	
