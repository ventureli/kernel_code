CC := gcc
CFLAGS := -Wall -g
#运行时查找库的路径（相对于可执行文件）
LDFLAGS := -L./libfoo -Wl,-rpath='$$ORIGIN/libfoo'
LDLIBS := -lfoo

SUBDIR := libfoo
TARGET := app

all : subdir $(TARGET)

subdir:
	$(MAKE) -C $(SUBDIR)

$(TARGET): main.o
	$(CC) $< -o $@ $(LDFLAGS) $(LDLIBS)

main.o : main.c
	$(CC) $(CFLAGS) -c $<

clean:
	$(MAKE) -C $(SUBDIR) clean
	rm -f *.o $(TARGET)
