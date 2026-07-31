# Makefile 静态库链接示例

对应文章：【内核札记-04】Makefile 极简入门：从手写 gcc 到多工程管理 (2026-07-01)

演示如何在 Makefile 中链接静态库（`.a` 文件）：
- `libmylib/` — 静态库源码（`foo.c`, `bar.c`）
- 先编译静态库 `libmylib.a`
- 再链接主程序和静态库

## 编译和运行

```bash
make
./main
make clean
```
