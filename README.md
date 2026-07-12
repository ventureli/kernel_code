# 🐧 From Kernel to Intelligence — Companion Code Repository

Welcome to the companion code repository for the WeChat public account **[从内核到智能] (From Kernel to Intelligence)**.

This repository contains all the example code, experimental demos, and project templates referenced in the articles. From Linux kernel fundamentals to systems programming, from data structures to concurrency mechanisms — every piece of code is designed to be **compilable, runnable, and verifiable**, helping you turn concepts from the articles into hands-on practice.

---

## 📁 Repository Structure

```
.
├── README.md              # English (this file)
├── README_zh.md           # 中文版
└── code/
    ├── algorithm/            # Kernel data structures & algorithms
    ├── gcc_makefile/         # GCC compilation & Makefile project templates
    ├── hello_world/          # Kernel module basics (Hello World series)
    └── kernel_component/     # Core kernel components in action
```

---

## 🗂️ Contents

### 1. Kernel Data Structures & Algorithms (`code/algorithm/`)

| Example | Description |
|---------|-------------|
| [`list/`](code/algorithm/list/) | Kernel linked list (`list_head`) — basic usage and traversal macros |
| [`hlist_demo/`](code/algorithm/hlist_demo/) | Kernel hash-linked list (`hlist`) demo |
| [`hlist_bl_demo/`](code/algorithm/hlist_bl_demo/) | Bit-lock-protected hash-linked list (`hlist_bl`) |
| [`rb_tree/`](code/algorithm/rb_tree/) | Kernel red-black tree (`rbtree`) — insert, search, and traversal |
| [`maple_tree/`](code/algorithm/maple_tree/) | Maple Tree usage example |

### 2. GCC & Makefile Project Templates (`code/gcc_makefile/`)

| Example | Description |
|---------|-------------|
| [`mk_project/`](code/gcc_makefile/mk_project/) | Standard C project Makefile template (separate `src/` + `include/`) |
| [`mk_with_a/`](code/gcc_makefile/mk_with_a/) | Static library (`.a`) compilation & linking |
| [`mk_with_so/`](code/gcc_makefile/mk_with_so/) | Shared library (`.so`) compilation & linking |

### 3. Kernel Module Hello World (`code/hello_world/`)

| Example | Description |
|---------|-------------|
| [`hello_world_simple/`](code/hello_world/hello_world_simple/) | Minimal kernel module: `insmod` / `rmmod` 101 |
| [`hello_world_param/`](code/hello_world/hello_world_param/) | Kernel module with module parameters |
| [`hello_world_param_sig/`](code/hello_world/hello_world_param_sig/) | Module parameters + kernel signal communication (includes userspace demo) |

### 4. Core Kernel Components (`code/kernel_component/`)

| Example | Description |
|---------|-------------|
| [`mutex_demo/`](code/kernel_component/mutex_demo/) | Kernel mutex lock (`mutex`) demo |
| [`lock/spinlock_demo/`](code/kernel_component/lock/spinlock_demo/) | Spinlock (`spin_lock`) demo |
| [`lock/futex/`](code/kernel_component/lock/futex/) | Fast userspace mutex (`futex`) |
| [`rcu/`](code/kernel_component/rcu/) | RCU (Read-Copy-Update) mechanism examples |
| [`per_cpu/`](code/kernel_component/per_cpu/) | Per-CPU variable demo |
| [`kworker_demo/`](code/kernel_component/kworker_demo/) | Kernel workqueue (`kworker`) — multiple usage patterns |

---

## 🚀 Quick Start

### Prerequisites

- **A Linux system** (kernel module examples require a Linux environment)
- **GCC** and **GNU Make**
- **Linux kernel headers** (needed for building kernel modules; typically located at `/lib/modules/$(uname -r)/build/`)

### Build & Run

Using the Hello World kernel module as an example:

```bash
# Enter the example directory
cd code/hello_world/hello_world_simple

# Compile
make

# Load the module
sudo insmod hello_world.ko

# Check kernel log output
dmesg | tail -5

# Unload the module
sudo rmmod hello_world
```

For userspace programs (e.g., examples under `algorithm/` or `gcc_makefile/`), simply run `make && ./<executable>`.

---

## 📖 Companion Blog

All code in this repository is paired with articles from the WeChat public account **[从内核到智能] (From Kernel to Intelligence)**. Each article specifies the corresponding code path. It is recommended to **read the article first to understand the principles, then run the code to verify**.

> Follow the account for in-depth analysis of the Linux kernel, systems programming, and low-level technologies.

---

## 🔄 Ongoing Updates

This repository will be continuously updated. Planned content includes but is not limited to:

- More kernel subsystem deep-dives (filesystems, network stack, memory management, etc.)
- eBPF and kernel observability
- Userspace–kernel interaction programming
- Performance analysis and tuning toolchains
- Embedded Linux development

**Watch** or **Star** this repo to stay in the loop!

---

## 📄 License

MIT License
