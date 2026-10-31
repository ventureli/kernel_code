# uinput_demo：用户态造虚拟输入设备

验证分布式硬件「反控」的底层机制——`distributed_input` 的 `virtual_device.cpp` 用的就是这套 uinput 接口。

## 编译 & 运行

```bash
# 交叉编译（OH 用户态：clang + musl）
make                # 产物在 bin/uinput_demo

# 推送 + 运行
make send           # 推到 /data/test_bin
make run            # 推 + 运行

# 看当前生效的路径/工具链
make info
```

运行期间另开一个终端，观察多出来的虚拟设备事件：

```bash
# 看虚拟设备（bustype = BUS_VIRTUAL，name = uinput-demo-touch）
cat /proc/bus/input/devices | grep -A5 -i uinput-demo

# 实时看事件上报
getevent -l /dev/input/eventX    # eventX 用程序打印的那个编号
```

## 关键点

| 步骤 | 代码 |
|---|---|
| 打开节点 | `open("/dev/uinput", O_WRONLY)` |
| 声明能力 | `UI_SET_EVBIT` / `UI_SET_ABSBIT` / `UI_SET_KEYBIT` |
| 写描述 | `write(uinput_user_dev)`（name/id/坐标范围） |
| 创建设备 | `UI_DEV_CREATE` —— 此刻才出现 `eventX` |
| 注入事件 | `write(struct input_event)`，一帧以 `EV_SYN/SYN_REPORT` 结尾 |
| 销毁 | `UI_DEV_DESTROY` |

## 注意

1. **路径**：`Makefile` 里 `OHOS_ROOT` / `PRODUCT` / `HDC` 三个变量按你的编译机改（默认 `/home/dell/code/6.0` + `taihang2102` + `hdc.exe`，跟你 `code/` 下其它 demo 一致）。Windows 下 hdc 是 `hdc.exe`，Linux 下是 `hdc`。
2. **权限**：`/dev/uinput` 需要 root 或 `input` 组。OH 工程版 `hdc shell` 默认 root，能直接跑；跑不通先 `hdc shell ls -l /dev/uinput` 看权限。
3. **内核头**：musl sysroot 没有 `linux/uinput.h`，所以 Makefile 从内核 `include/uapi` 引（`-I$(KDIR)/include/uapi -I$(KDIR)/arch/arm64/include/uapi`）。
4. **对应读侧**：`code/driver_dev/input_dev_core/` 是 evdev 读事件那一侧（uinput 的镜像），可对照看。

## 真机运行记录（实测）

跑起来后 `cat /proc/bus/input/devices | grep -A5 uinput-demo`：

```
N: Name="uinput-demo-touch"
P: Phys=
S: Sysfs=/devices/virtual/input/input8
U: Uniq=
H: Handlers=kbd mouse0 event3
B: PROP=0
```

`ls -al /dev/input/` 前后对比：

```
# 运行前
event0  event1  event2  mice

# 运行中（多了 event3 和 mouse0）
event0  event1  event2  event3  mice  mouse0
```

### 三个值得记住的现象

1. **`Handlers=kbd mouse0 event3` —— 一个设备挂出三个入口**
   - `event3`：evdev 主节点（`/dev/input/event3`），应用 / `getevent` 走这里
   - `kbd`：因为声明了 `KEY_VOLUMEUP`，input 核心给它挂了键盘 handler
   - `mouse0`：因为声明了 `BTN_TOUCH` + `EV_ABS`，mousedev 把 `BTN_TOUCH` 当鼠标左键，自动生成了 `/dev/input/mouse0`

   所以「音量键」（EV_KEY）和「触摸」（EV_ABS + BTN_TOUCH）都能从这一个设备报出来，真机上音量键事件直接生效了。

2. **`Sysfs=.../input8`，上次跑是 `input7`** —— 每次 `UI_DEV_CREATE` 都分配一个新的 input 设备号（inputN 递增），程序退出走 `UI_DEV_DESTROY` 才回收。

3. **`P: Phys=` 是空的** —— 因为我们没调 `UI_SET_PHYS`。真实反控里 `distributed_input` 会 `UI_SET_PHYS` 填物理标识；这里留空不影响事件上报。
