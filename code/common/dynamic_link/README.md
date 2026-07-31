# dlopen 动态加载 .so 示例

对应文章：【内核札记-33】SO文件与动态链接——dlopen、readelf 和 patchelf 入门 (2026-08-01)

演示使用 `dlopen()` / `dlsym()` / `dlclose()` 在运行时动态加载共享库：

- `libdemo.c` / `libdemo.h` — 共享库源码，提供 `hello_from_so()` 和 `add()` 两个函数
- `dlopen_demo.c` — 调用者，运行时加载 `libdemo.so` 并调用其中的函数
- 对比编译时链接（`NEEDED` 段写死依赖）和运行时加载（NEEDED 段不体现）

## 编译和运行

```bash
make
./dlopen_demo

# 用 readelf 查看依赖关系
readelf -d dlopen_demo | grep NEEDED
readelf -l dlopen_demo | grep INTERP
```

## 知识点

- `dlopen()` — 运行时加载 .so 到进程地址空间（内部使用 mmap）
- `dlsym()` — 按字符串查找符号（函数/变量）
- `dlclose()` — 卸载 .so（引用计数为 0 时真正卸载）
- `-fPIC` — 位置无关代码，.so 编译必须启用
- `-ldl` — 链接 libdl.so（提供 dlopen/dlsym/dlclose）
- READELF `-l` — 查看 INTERP 段（动态链接器路径）
- READELF `-d` — 查看 NEEDED（编译时依赖）和 RUNPATH
- `patchelf` — 修改已编译 ELF 的 INTERP、NEEDED、RUNPATH
