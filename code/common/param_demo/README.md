# my_param demo —— 系统参数的极简实现

复刻 OpenHarmony 系统参数（system parameter）机制的骨架，用来理解三件事：**共享内存、单一写者、监听通知**。

## 文件

| 文件 | 作用 |
|------|------|
| `my_param_common.h` | 共享内存布局 + 协议（服务端/客户端共用） |
| `my_param_server.c` | 服务端：唯一写者 + 监听 socket + epoll + watcher 列表 |
| `my_param.c` | 客户端命令：get / set / watch |
| `Makefile` | 编译 |

## 编译 & 运行

```bash
make
./my_param_server &          # 先起服务端
./my_param set my_param hello
./my_param get my_param       # → hello
```

**watch 实测**（开两个终端）：

```bash
# 终端 A
./my_param watch my_param newvalue 30    # 阻塞等

# 终端 B
./my_param set my_param newvalue

# 终端 A 立刻返回
Wait parameter my_param success
```

## 真机实测输出（已验证）

```text
$ ./my_param_server &

$ ./my_param set parm1 hello
Set parameter parm1 hello success
$ ./my_param get parm1
hello

$ ./my_param set my_param newvalue
Set parameter my_param newvalue success

$ ./my_param watch my_param newvalue 30
Wait parameter my_param success
```

> 注意最后那个 `watch` 是**立即返回**的——因为上一步已经把 `my_param` 设成了 `newvalue`，
> 命中了服务端的「立即通知」逻辑（注册时发现当前值已满足条件，直接回 NOTIFY）。
> 要观察「阻塞等待 → 后来 set → 被唤醒」，得开两个终端：终端 A 先 `watch`（阻塞），终端 B 再 `set`。

**超时情况**（没人 set，poll 超时）：

```text
$ ./my_param watch parm2 something 10
（等 10 秒，期间没人 set parm2 something）
Wait parameter parm2 timeout
```

## 三条链路

| 命令 | 走哪条路 |
|------|---------|
| `get` | mmap 共享内存直接读（零 socket / 零 IPC） |
| `set` | 连 Unix 域套接字发给服务端（单一写者） |
| `watch` | 连 socket 注册，poll 阻塞等 `NOTIFY` 通知 |

## 版本号 + 内存屏障：`set` 的发布协议

服务端 `set` 写共享内存时的三行核心代码：

```c
strncpy(e->value, value, MAX_VALUE - 1);              // ① 写值
__sync_synchronize();                                  // ② 内存屏障
__sync_add_and_fetch(&e->commit_id, 1);                // ③ 参数版本号 +1
__sync_add_and_fetch(&g_area->global_commit_id, 1);    // ④ 全局版本号 +1
```

这三行实现「**无锁的单写多读发布协议**」：写者先写值、再发布版本号，读者靠版本号判断值变没变。

- **`__sync_synchronize()`（内存屏障）**：保证它之前的写（value）对其它进程可见于它之后的写（版本号）之前。没有屏障，CPU 可能把「版本号 +1」重排到「写 value」前面，读者就可能看到「版本号是新的、value 还是旧的/半截」的不一致。
- **`__sync_add_and_fetch(&e->commit_id, 1)`**：原子「读-改-写」，把参数版本号 +1。编译成 x86 的 `LOCK xadd` / ARM 的 `LDADD`，并发下不被打断。读者靠它判断「这个参数变没变」。
- **`__sync_add_and_fetch(&g_area->global_commit_id, 1)`**：全局版本号 +1，任何参数变化都加，用于「有没有任何参数变」（比如遍历所有参数）。

两个版本号对应第 2 篇的 `commitId`：

| demo | 第 2 篇 | 作用 |
|------|---------|------|
| `e->commit_id` | `ParamNode.commitId` | 单个参数变没变 |
| `g_area->global_commit_id` | `ParamTrieHeader.commitId` | 有没有任何参数变 |

一句话：**写值 → 屏障 → 原子自增版本号**，读者先读版本号、变了才读值，就永远看不到半截数据。

## 简化点（对照真实 OpenHarmony）

- 共享内存用**数组**，真实实现是 Trie（见 `oh-kernel/system_param/` 第 2 篇）；
- **没有 DAC/SELinux 权限**，任何进程都能读所有参数；
- **通知直接走 socket**，没有 `param_watcher` 那一层 binder 转发；
- 读消息假设一次 `read` 拿到一整条（没做粘包处理）；
- 版本号用 `__sync_*` 原子操作，没做严格的内存序/缓存校验（真实实现见第 2 篇 `commitId`）。
- **超时用客户端 `poll` 的超时参数实现**（`poll(fd, 1, timeout*1000)`）；真实 OH 是服务端用 timer 统一管理（见 `oh-kernel/system_param/` 第 1 篇 3.4）。
