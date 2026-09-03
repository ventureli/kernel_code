# 运行结果与解析

## 编译运行
安装模块
```sh
# 编译
$ make
make -C /lib/modules/5.15.0-190-generic/build M=/media/psf/kernel_learn/code/task_schedule/mmap_leak_probe modules
make[1]: Entering directory '/usr/src/linux-headers-5.15.0-190-generic'
  CC [M]  /media/psf/kernel_learn/code/task_schedule/mmap_leak_probe/mmap_leak_probe.o
  MODPOST /media/psf/kernel_learn/code/task_schedule/mmap_leak_probe/Module.symvers
  CC [M]  /media/psf/kernel_learn/code/task_schedule/mmap_leak_probe/mmap_leak_probe.mod.o
  LD [M]  /media/psf/kernel_learn/code/task_schedule/mmap_leak_probe/mmap_leak_probe.ko
  BTF [M] /media/psf/kernel_learn/code/task_schedule/mmap_leak_probe/mmap_leak_probe.ko
Skipping BTF generation for /media/psf/kernel_learn/code/task_schedule/mmap_leak_probe/mmap_leak_probe.ko due to unavailability of vmlinux
make[1]: Leaving directory '/usr/src/linux-headers-5.15.0-190-generic'
gcc -O2 -Wall -o leak_demo leak_demo.c

$ sudo insmod mmap_leak_probe.ko target=leak_demo
$ ./leak_demo &
[2] 7519
```


可以看到，安装模块后，通过模块参数传入了进程的名字，然后执行leak_demo程序。打印了进程号7519
我们知道这时候我们的进程已经偷偷的再隔一会就回泄漏20M内存了。




## 查找问题
现在假设我们是一个真实的用户场景，可能很快就会有问题上报上来，这个应用占用内存过大，出现oom的大批量上报。那么如何调试呢？
一般思路如下：
**第一步：确定问题**
1. 如果在生产环境，我们通过top或者一写用户层软件就可以看到了这个程序出现了泄漏。这里我们用watch指令就行监测
`watch -n 1 'grep -E "VmSize|VmRSS|VmData" /proc/7519/status'`
这条指令每个1s刷新一次。
![alt text](<截屏2026-09-03 23.01.51-1.png>)
![alt text](<截屏2026-09-03 23.05.37.png>) 
通过采样我们可以看到，确实出现了内存持续上升单是不下降的问题。

多测试几次基本确定内存泄漏。

**第二步 确定泄漏区域**  
通过pmap查看进程内存分布。 
```sh 
---------------- ------- ------- ------- 
total kB         1867936 1866140 1865816
7519:   ./leak_demo
Address           Kbytes     RSS   Dirty Mode  Mapping
0000aaaab2580000       4       4       0 r-x-- leak_demo
0000aaaab2590000       4       4       4 r---- leak_demo
0000aaaab2591000       4       4       4 rw--- leak_demo
0000aaaabc791000     132       4       4 rw---   [ anon ]
0000ffff2e300000 1927168 1927168 1927168 rw---   [ anon ]   
0000ffffa3d00000    1592     316       0 r-x-- libc.so.6
0000ffffa3e8e000      60       0       0 ----- libc.so.6
0000ffffa3e9d000      16      16      16 r---- libc.so.6
0000ffffa3ea1000       8       8       8 rw--- libc.so.6
0000ffffa3ea3000      48      16      16 rw---   [ anon ]
0000ffffa3ec7000     172       0       0 r-x-- ld-linux-aarch64.so.1
0000ffffa3efc000       8       8       8 rw---   [ anon ]
0000ffffa3efe000       8       0       0 r----   [ anon ]
0000ffffa3f00000       4       4       0 r-x--   [ anon ]
0000ffffa3f01000       8       8       8 r---- ld-linux-aarch64.so.1
0000ffffa3f03000       8       8       8 rw--- ld-linux-aarch64.so.1
0000fffff58bd000     132      12      12 rw---   [ stack ]
---------------- ------- ------- ------- 
total kB         1929376 1927580 1927256
7519:   ./leak_demo
Address           Kbytes     RSS   Dirty Mode  Mapping
0000aaaab2580000       4       4       0 r-x-- leak_demo
0000aaaab2590000       4       4       4 r---- leak_demo
0000aaaab2591000       4       4       4 rw--- leak_demo
0000aaaabc791000     132       4       4 rw---   [ anon ]
0000ffff2a700000 1988608 1988608 1988608 rw---   [ anon ]
0000ffffa3d00000    1592     316       0 r-x-- libc.so.6
0000ffffa3e8e000      60       0       0 ----- libc.so.6
0000ffffa3e9d000      16      16      16 r---- libc.so.6
0000ffffa3ea1000       8       8       8 rw--- libc.so.6
0000ffffa3ea3000      48      16      16 rw---   [ anon ]
0000ffffa3ec7000     172       0       0 r-x-- ld-linux-aarch64.so.1
0000ffffa3efc000       8       8       8 rw---   [ anon ]
0000ffffa3efe000       8       0       0 r----   [ anon ]
0000ffffa3f00000       4       4       0 r-x--   [ anon ]
0000ffffa3f01000       8       8       8 r---- ld-linux-aarch64.so.1
0000ffffa3f03000       8       8       8 rw--- ld-linux-aarch64.so.1
0000fffff58bd000     132      12      12 rw---   [ stack ]
---------------- ------- ------- ------- 
total kB         1990816 1989020 1988696
7519:   ./leak_demo
Address           Kbytes     RSS   Dirty Mode  Mapping
0000aaaab2580000       4       4       0 r-x-- leak_demo
0000aaaab2590000       4       4       4 r---- leak_demo
0000aaaab2591000       4       4       4 rw--- leak_demo
0000aaaabc791000     132       4       4 rw---   [ anon ]
0000ffff26d00000 2048000 2029840 2029840 rw---   [ anon ] 
0000ffffa3d00000    1592     316       0 r-x-- libc.so.6
0000ffffa3e8e000      60       0       0 ----- libc.so.6
0000ffffa3e9d000      16      16      16 r---- libc.so.6
0000ffffa3ea1000       8       8       8 rw--- libc.so.6
0000ffffa3ea3000      48      16      16 rw---   [ anon ]
0000ffffa3ec7000     172       0       0 r-x-- ld-linux-aarch64.so.1
0000ffffa3efc000       8       8       8 rw---   [ anon ]
0000ffffa3efe000       8       0       0 r----   [ anon ]
0000ffffa3f00000       4       4       0 r-x--   [ anon ]
0000ffffa3f01000       8       8       8 r---- ld-linux-aarch64.so.1
0000ffffa3f03000       8       8       8 rw--- ld-linux-aarch64.so.1
0000fffff58bd000     132      12      12 rw---   [ stack ]
---------------- ------- ------- ------- 
total kB         2050208 2030252 2029928

```
从几次pmap的输出基本可以确定：基本上可以确定：


```sh
# 第一次 pmap
0000ffff2a700000 1988608 1988608 1988608 rw---   [ anon ]

# 第二次 pmap
0000ffff2e300000 1927168 1927168 1927168 rw---   [ anon ]  
# 第三次 pmap
0000ffff26d00000 2048000 2029840 2029840 rw---   [ anon ]
```
这里区域出现了泄漏，从这里的信息能看到如下问题

* mode rw---（没有s） 这是一个MAP_PRIVATE 匿名映射（私有），不是共享。如果是如果是 mmap(..., MAP_SHARED|MAP_ANONYMOUS, ...)，mode 会带 s（rws--）
* [ anon ] 只说明「这个 VMA 没有文件 backing 文件，
  
产生类似这种类型内存区域的 一般有如下几类情况。

|来源	| 说明	|  pmap 里长啥样| 
|-------|------|--------------|
|直接 mmap| (MAP_PRIVATE\|MAP_ANONYMOUS) | rw--- [ anon ]
|malloc/calloc 大块分配|glibc 超过 MMAP_THRESHOLD（默认 128KB）的 malloc 内部就走 mmap 匿名映射	|rw--- [ anon ]|
|线程栈	pthread_create| 每个线程默认分 8MB 匿名映射做栈，不 join 就会越积越多	|rw--- [ anon ]|
|匿名共享映射	MAP_SHARED\|MAP_ANONYMOUS	|rws-- [ anon ] ← 带 s(不是这个)|

>小 malloc 走 brk，但会标成 `rw--- [ heap ] ` 不是 anon 着就是为什表里没有列malloc/calloc小块内存 


**所以**看到 `rw--- [ anon ]` 大概率是 mmap，但不一定是程序手写的 mmap——更常见的是「大块 malloc 泄漏」或「线程栈泄漏」，它们底层也是 mmap 出来的。 

**不过**：这三者最终都经过 __arm64_sys_mmap，所以我们的探针全部抓得住。

看三次快照里那条大 [ anon ]：
```
0000ffff2e300000 1927168 ... rw--- [ anon ]
0000ffff2a700000 1988608 ... rw--- [ anon ]
0000ffff26d00000 2048000 ... rw--- [ anon ]
```
三个点一起看：
* 地址在往下走（ffff2e30 → ffff26d0），说明内核是「top-down」从高地址往下给新的匿名映射排地址；
* 大小在涨、且地址下移的量 ≈ 大小涨的量——这是相邻匿名 VMA 被内核合并成一个 VMA 的特征； 
注意这里出现了**合并**
### 合并的条件
**合并的条件（VMA merge）**：pmap 里每一行对应内核的一个 struct vm_area_struct（VMA），而 mmap 的落点并不会无条件新开一行——mm/mmap.c 里的 vma_merge() 会先试着把新区域并进相邻的 VMA。合并有硬性条件：
* 地址必须首尾相连、中间不能有空洞；
* vm_flags 必须完全相同（读写执行权限、MAP_PRIVATE/MAP_SHARED，以及内部 VM_ 标志）；
* vm_file 必须一致（匿名映射两边都得是 NULL，文件映射得是同一个 struct file）；
* vm_pgoff 相同（匿名映射两边都是 0）；
* vm_ops、anon_vma 还要兼容，且不能是 VM_SPECIAL、hugepage、mlock 这类特殊映射。只要有一条不满足，内核就会拆成独立的 VMA，pmap 里表现为多行。

**反推泄漏的形状**：所以对内核有了解的人，看到 pmap 里那一条「连续、干净、不断变大的单条 [ anon ]」，就已经能反推——泄漏的块是等大小、连续分配的，而且中间没被打断。因为只要块大小不一、或者中间夹着正常的 munmap 打出空洞，内核就不会合并，pmap 一定会显示成很多条零散的 [ anon ]，而不是这一条严丝合缝的大段。 

这里看泄漏的大小其实就可以排除`pthread_create` 这种泄漏场景了


## 写内核模块开始探查
现在我们基本可以确定应该是MMAP出现的问题,那么我们就通过probe，和kretprobe配合使用这个探针可以拿到返回值addr。然后输出用proc文件系统

* kretprobe 挂 __x64_sys_mmap：ret_handler 拿返回值 addr，把 {addr, len, flags, comm} 塞进 hashtable（key = addr）；
* kprobe 挂 __x64_sys_munmap：pre_handler 拿 addr+len，从 hashtable 删掉对应条目；
* 通过 hash_table 来记录和删除 mmap和munmap的操作，这样剩下的就是泄漏的地址。
* /proc 文件用于对用户态输出。dump 剩下的条目 = 没配对上的泄漏映射。
  计算逻辑如下（这个在第15篇文章里：《/proc 文件系统：内核的"状态面板"——从 `cat /proc/cpuinfo` 到自己写 proc 文件》有介绍）


## 查看结果

```
=== mmap 了但没 munmap 的映射（泄漏）===
[leak_demo] addr=0xffff0b500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffed88f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffffa3d00000  len=0x1aee68 (1723 KB)  flags=0x812  (文件映射)
[leak_demo] addr=0xfffe72d00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffed3100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffea04f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff6b900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff33500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff008f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffe9ad00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffec84f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff93900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffefb100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff5b500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffec2d00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffef04f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffffa3e9d000  len=0x6000 (24 KB)  flags=0x812  (文件映射)
[leak_demo] addr=0xfffe8a900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff23100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffeead00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffeb80f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff83500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffeb2900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff4b100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffe7a500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff12d00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffee00f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffeda900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffea7cf0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff73100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffea2500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff3ad00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff080f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff02900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffecfcf0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff9b100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff62d00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffeca500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff2a900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffef7cf0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffe92100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffebf8f0000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff8ad00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffef2500000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffeba100000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xffff52900000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射)
[leak_demo] addr=0xfffe81d00000  len=0x1400000 (20480 KB)  flags=0x22  (匿名映射) 
```