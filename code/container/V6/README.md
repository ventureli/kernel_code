# mycontainer V6 —— OverlayFS 分层镜像

「从零手写容器」系列的最终版，在 V5 的基础上加入**镜像管理 + OverlayFS 分层存储**：

- V0~V5：`run -r <rootfs>` 直接跑一个解压好的根文件系统目录，容器对 rootfs 的写操作直接落盘、多个容器互相污染。
- V6：支持 `import` 导入镜像 → `run --image <name>` 以 overlay 方式启动，镜像（lowerdir）只读，容器写操作进独立的 upper 层，多个容器共享同一镜像、互不可见。

核心依赖仍是 `clone(2) + pivot_root(2) + cgroup + mount`，没有中间层。

## 编译

```sh
make
```

## 用法

```sh
sudo ./mycontainer import <tar.gz> <name>      # 导入镜像
sudo ./mycontainer images                       # 列出镜像
sudo ./mycontainer rmi <name>                   # 删除镜像
sudo ./mycontainer run --image <name>           # 从镜像启动（overlay 模式）
sudo ./mycontainer run -r <rootfs>              # 保留原有直接跑目录模式
sudo ./mycontainer commit <id> <name>           # 把容器运行时视图导出为新镜像
sudo ./mycontainer rm <id>                      # 删除容器 + 清理 overlay
```

仓库附带的 `alpine-minirootfs.tar.gz`（aarch64）可直接 `import` 成镜像；也可以 `make rootfs` 从官方源重新下载。

```sh
sudo ./mycontainer import ./alpine-minirootfs.tar.gz alpine
sudo ./mycontainer run --image alpine
```

`run` 支持的其它参数（V5 起沿用）：`-m/--memory`、`-c/--cpu` 资源限制，`--name` 命名，`-d/--detach` 后台，`-v/--verbose` 详细输出。

## overlay 存储结构

```
/var/lib/mycontainer/
├── images/
│   └── <name>/rootfs/          ← import 解压到这里（lowerdir，只读）
└── containers/
    └── <id>/
        ├── upper/              ← overlay 可写层
        ├── work/               ← overlay workdir
        ├── merged/             ← overlay 挂载点
        └── image               ← 记录用的哪个镜像
```

## overlay 生命周期

- **run --image**：父进程建 `upper/work/merged` 目录 → 子进程在 mount namespace 里 `mount("overlay", merged, "lowerdir=...,upperdir=...,workdir=...")` → `pivot_root(merged)`。容器内 `/` 就是 merged 视图，写操作进 upper，镜像本身只读。
- **stop**：父进程 `umount2(merged, MNT_DETACH)` 卸载 overlay，upper 保留。
- **commit**：重新挂 overlay → `tar` 整个 merged 视图 → 解压成新镜像 → 卸载。导出后就是一个标准的 Linux rootfs，可用 `run -r` 直接跑，也可再作为 `--image` 给别的容器用。
- **rm**：卸载 overlay + 删 `upper/work/merged` + 清理状态。

架构图见源码头注释（`clone` + cgroup + user ns + capability 裁剪 + pivot_root + overlay）。
