

## 模块参数确认
1. 查看参数
```sh
# ls -al /sys/module/hello_world_with_param_sig/parameters
total 0
drwxr-xr-x 2 root root    0 2008-01-01 11:27 .
drwxr-xr-x 6 root root    0 2008-01-01 11:27 ..
-rw-rw-r-- 1 root root 4096 2008-01-01 11:27 pid
```
2. 读写

```sh
# cat pid
0
# echo 1 >>  pid
#
```
3. 通过dmesg查看日志

```sh
[12505.016714] WQTK: Param pid changed !
[12505.016735] WQTK: Old=0
[12505.016749] WQTK: New=1
```
以上说明模块参数已经建立

## 用户空间测试

1. 编译用户空间程序

```sh
#gcc -o wait_sig_demo wait_sig_demo.c
```
2. 运行用户空间程序

```sh
#cd /data/test_bin/bin/
#./wait_sig_demo
process 19087 is waiting signal:42...
```
3. 拿到用户进程的pid

把pid通过命令行设置到内核参数里

```sh
# cd /sys/module/hello_world_with_param_sig/parameters
# echo 19087 >> pid
```
命令行输出如下：
```sh
收到自定义信号：42
发送进程PID：18875
发送进程UID：0
信号来源类型：128
```
可以看到能够接收到内核发出来的信号。这是一个最简单的demo

# 扩展

## 手动传递参数可以改成自动
这里我们是通过人工把参数传入到内核模块的参数的，完全可以在用户空间的程序启动后就自己把pid写入到内核参数里。

## 参数类型转换程字符串后可以自定义命令
我们这个模块注册的参数是一个int类型，想想空间优先，完全可以编程字符串，然后就可以通过对字符串进行命令格式定义，比如：
```
"add-pid 19911"
"del-pid 19911"
```
之类的,用户空间和内核空间就可以完成更丰富的功能了。

## 信号数值为什么用42？
这个linux 5.10的内核include/api/asm-generic/signal.h的代码片段 
```C
#ifndef _UAPI__ASM_GENERIC_SIGNAL_H
#define _UAPI__ASM_GENERIC_SIGNAL_H

#include <linux/types.h>

#define _NSIG		64
#define _NSIG_BPW	__BITS_PER_LONG
#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

#define SIGHUP		 1
#define SIGINT		 2
#define SIGQUIT		 3
#define SIGILL		 4
#define SIGTRAP		 5
#define SIGABRT		 6
#define SIGIOT		 6
#define SIGBUS		 7
#define SIGFPE		 8
#define SIGKILL		 9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		SIGIO
/*
#define SIGLOST		29
*/
#define SIGPWR		30
#define SIGSYS		31
#define	SIGUNUSED	31

/* These should not be considered constants from userland.  */
#define SIGRTMIN	32
#ifndef SIGRTMAX
#define SIGRTMAX	_NSIG
#endif
```
可以看到 32以前的信号都被用了，SIGRTMIN(32) 到 SIGRTMAX(64) 是我们可以自定义使用，这两个数值在不同的系统版本上不一样的，所有我们在定义一个自定义信号的时候，最好使用 #define xxx (SIGRTMIN + yyy) 这种方式进行，比较安全，本文为了简化，直接硬编码为42
