# Makefile 多文件工程示例

对应文章：【内核札记-04】Makefile 极简入门：从手写 gcc 到多工程管理 (2026-07-01)

演示常见生产环境下的 Makefile 工程结构：
- `src/` — 源文件目录（`main.c`, `hello.c`）
- `include/` — 头文件目录（`hello.h`）
- `obj/` — 编译中间产物
- `bin/` — 最终可执行文件
- 使用 `wildcard`、`patsubst` 等 Makefile 关键字自动收集源文件

## 编译和运行

```bash
make
./bin/app
make clean
```
