CC := gcc
CFLAGS := -Wall -g
LIB_DIR := libmylib
LIB := $(LIB_DIR)/libmylib.a
TARGET := app

all : $(LIB) $(TARGET)

$(LIB):
	$(MAKE)  -C $(LIB_DIR)
$(TARGET): main.o $(LIB)
	$(CC) $^ -o $@

main.o: main.c
	$(CC) $(CFLAGS) -c $<

clean:
	$(MAKE) -C $(LIB_DIR) clean
	rm -f *.o $(TARGET)