# 用户态 mempool 实现

对应文章：【内核札记-27】内存池：给内核留一条"最后的活路" (2026-07-26)

从零实现的用户态版本内存池，帮助理解 mempool 的底层原理：
- 核心三张表管理内存块
- 预分配 + 动态分配的双层机制
- 理解 mempool 的预留池（emergency pool）如何保证在内存紧张时仍能分配

## 编译和运行

```bash
gcc -o mempool mempool.c
./mempool
```
