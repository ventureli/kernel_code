# Hello World 内核模块

对应文章：【内核札记-01】手写第一个内核模块：Hello World 从编译到运行 (2026-06-29)

最简单的内核模块示例，演示了：
- 编写一个基本的内核模块（`module_init` / `module_exit`）
- 使用 `printk` 在内核日志中输出信息
- Makefile 的编写和模块编译流程
- `insmod` / `rmmod` / `dmesg` 的基本使用

## 编译和运行

```bash
make
sudo insmod hello_world.ko
dmesg | tail -5
sudo rmmod hello_world
```
