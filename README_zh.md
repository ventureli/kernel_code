# 🐧 从内核到智能 — 配套代码仓库

欢迎来到公众号 **[从内核到智能]** 的配套代码仓库。

这里存放公众号文章中涉及的所有示例代码、实验 Demo 和工程模板。从 Linux 内核基础到系统编程，从数据结构到并发机制，每一份代码都力求**可编译、可运行、可验证**，帮助你将文章中的概念落地为真实的动手实践。

---

## 📁 仓库结构

```
.
├── README.md              # 英文版
├── README_zh.md           # 中文版（当前文件）
└── code/
    ├── algorithm/            # 内核数据结构与算法
    ├── gcc_makefile/         # GCC 编译 & Makefile 工程模板
    ├── hello_world/          # 内核模块入门（Hello World 系列）
    └── kernel_component/     # 内核核心组件实战
```

---

## 🗂️ 内容概览

### 1. 内核数据结构与算法 (`code/algorithm/`)

| 示例 | 说明 |
|------|------|
| [`list/`](code/algorithm/list/) | 内核链表（`list_head`）的基本用法与遍历宏 |
| [`hlist_demo/`](code/algorithm/hlist_demo/) | 内核哈希链表（`hlist`）使用演示 |
| [`hlist_bl_demo/`](code/algorithm/hlist_bl_demo/) | 位锁保护的哈希链表（`hlist_bl`） |
| [`rb_tree/`](code/algorithm/rb_tree/) | 内核红黑树（`rbtree`）的插入、查找与遍历 |
| [`maple_tree/`](code/algorithm/maple_tree/) | Maple Tree 的使用示例 |

### 2. GCC & Makefile 工程模板 (`code/gcc_makefile/`)

| 示例 | 说明 |
|------|------|
| [`mk_project/`](code/gcc_makefile/mk_project/) | 标准 C 项目 Makefile 模板（`src/` + `include/` 分离） |
| [`mk_with_a/`](code/gcc_makefile/mk_with_a/) | 静态库（`.a`）的编译与链接 |
| [`mk_with_so/`](code/gcc_makefile/mk_with_so/) | 动态库（`.so`）的编译与链接 |

### 3. 内核模块 Hello World (`code/hello_world/`)

| 示例 | 说明 |
|------|------|
| [`hello_world_simple/`](code/hello_world/hello_world_simple/) | 最简内核模块：`insmod` / `rmmod` 入门 |
| [`hello_world_param/`](code/hello_world/hello_world_param/) | 带模块参数的内核模块 |
| [`hello_world_param_sig/`](code/hello_world/hello_world_param_sig/) | 带参数 + 内核信号通信（含用户态 Demo） |

### 4. 内核核心组件 (`code/kernel_component/`)

| 示例 | 说明 |
|------|------|
| [`mutex_demo/`](code/kernel_component/mutex_demo/) | 内核互斥锁（`mutex`）使用演示 |
| [`lock/spinlock_demo/`](code/kernel_component/lock/spinlock_demo/) | 自旋锁（`spin_lock`）使用演示 |
| [`lock/futex/`](code/kernel_component/lock/futex/) | 快速用户态锁（`futex`） |
| [`rcu/`](code/kernel_component/rcu/) | RCU（Read-Copy-Update）机制示例 |
| [`per_cpu/`](code/kernel_component/per_cpu/) | Per-CPU 变量使用演示 |
| [`kworker_demo/`](code/kernel_component/kworker_demo/) | 内核工作队列（`kworker`）多种用法 |

---

## 🚀 快速开始

### 环境要求

- **Linux 系统**（内核模块示例需要运行在 Linux 环境下）
- **GCC** 和 **GNU Make**
- **Linux 内核头文件**（编译内核模块时需要，通常位于 `/lib/modules/$(uname -r)/build/`）

### 编译 & 运行示例

以内核模块 Hello World 为例：

```bash
# 进入示例目录
cd code/hello_world/hello_world_simple

# 编译
make

# 加载模块
sudo insmod hello_world.ko

# 查看内核日志输出
dmesg | tail -5

# 卸载模块
sudo rmmod hello_world
```

对于用户态程序（如 `algorithm/`、`gcc_makefile/` 下的示例），直接 `make && ./<可执行文件>` 即可。

---

## 📖 配套公众号

本仓库所有代码均配合公众号 **[从内核到智能]** 的文章使用。每篇文章会指明对应的代码路径，建议**先读文章理解原理，再动手跑代码验证**。

> 关注公众号，第一时间获取 Linux 内核、系统编程与底层技术的深度解析。

---

## 🔄 持续更新

本仓库会持续更新，新增内容将包括但不限于：

- 更多内核子系统实战（文件系统、网络协议栈、内存管理等）
- eBPF 与内核可观测性
- 用户态与内核态交互编程
- 性能分析与调优工具链
- 嵌入式 Linux 开发

欢迎 **Watch** 或 **Star** 本仓库，不错过每一次更新！

---

## 📄 License

MIT License
