# Makefile 动态库链接示例

对应文章：【内核札记-04】Makefile 极简入门：从手写 gcc 到多工程管理 (2026-07-01)

演示如何在 Makefile 中链接动态库（`.so` 文件）：
- `libfoo/` — 动态库源码
- 编译动态库 `libfoo.so`
- 链接主程序时指定 `-L. -lfoo` 和 `LD_LIBRARY_PATH`

## 编译和运行

```bash
make
LD_LIBRARY_PATH=./libfoo ./main
make clean
```
