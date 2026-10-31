/*
 * uinput_demo.c —— 用户态用 uinput 造一个虚拟输入设备并注入事件
 *
 * 作用：验证分布式硬件「反控」的底层机制（distributed_input 的 virtual_device.cpp 也是这么干的）
 * 效果：运行后本机多出一个 /dev/input/eventX，并注入一次「触摸点击」+「音量键按下抬起」
 *
 * 验证：运行期间另开一个终端执行  getevent -l /dev/input/eventX  看事件上报
 *       （eventX 具体编号看本程序打印，或 cat /proc/bus/input/devices）
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#define VIRT_DEV_NAME "uinput-demo-touch"
#define SCREEN_W      1080
#define SCREEN_H      2340

/* ioctl 带错误检查：失败打印并退出 */
static void ioctl_or_die(int fd, unsigned long req, unsigned long arg, const char *what)
{
    if (ioctl(fd, req, arg) < 0) {
        fprintf(stderr, "ioctl %s failed: %s\n", what, strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/* 写入一个 input 事件（必须 memset 清零，让内核自己盖时间戳） */
static void emit(int fd, __u16 type, __u16 code, __s32 value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev)) {
        fprintf(stderr, "write event failed: %s\n", strerror(errno));
    }
}

int main(void)
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "open /dev/uinput failed: %s\n", strerror(errno));
        fprintf(stderr, "提示：需要 root 权限，或 /dev/uinput 有写权限\n");
        return EXIT_FAILURE;
    }

    /* ① 声明能力：绝对坐标（触摸）+ 触摸键 + 音量键 */
    ioctl_or_die(fd, UI_SET_EVBIT, EV_ABS, "UI_SET_EVBIT(EV_ABS)");
    ioctl_or_die(fd, UI_SET_ABSBIT, ABS_X, "UI_SET_ABSBIT(ABS_X)");
    ioctl_or_die(fd, UI_SET_ABSBIT, ABS_Y, "UI_SET_ABSBIT(ABS_Y)");
    ioctl_or_die(fd, UI_SET_EVBIT, EV_KEY, "UI_SET_EVBIT(EV_KEY)");
    ioctl_or_die(fd, UI_SET_KEYBIT, BTN_TOUCH, "UI_SET_KEYBIT(BTN_TOUCH)");
    ioctl_or_die(fd, UI_SET_KEYBIT, KEY_VOLUMEUP, "UI_SET_KEYBIT(KEY_VOLUMEUP)");

    /* ② 写设备描述：名字、厂商/产品 ID、坐标范围 */
    struct uinput_user_dev udev;
    memset(&udev, 0, sizeof(udev));
    snprintf(udev.name, UINPUT_MAX_NAME_SIZE, "%s", VIRT_DEV_NAME);
    udev.id.bustype = BUS_VIRTUAL; /* 虚拟总线，一眼认出是 uinput 设备 */
    udev.id.vendor  = 0x1234;
    udev.id.product = 0x5678;
    udev.id.version = 1;
    udev.absmin[ABS_X] = 0;
    udev.absmax[ABS_X] = SCREEN_W;
    udev.absmin[ABS_Y] = 0;
    udev.absmax[ABS_Y] = SCREEN_H;
    if (write(fd, &udev, sizeof(udev)) != (ssize_t)sizeof(udev)) {
        fprintf(stderr, "write uinput_user_dev failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /* ③ 创建设备：内核此刻生成 /dev/input/eventX + sysfs 条目 */
    ioctl_or_die(fd, UI_DEV_CREATE, 0, "UI_DEV_CREATE");

    /* ④ 取回内核分配的设备名 */
    char sysname[64];
    memset(sysname, 0, sizeof(sysname));
    if (ioctl(fd, UI_GET_SYSNAME(sizeof(sysname)), sysname) == 0) {
        printf("已创建虚拟设备：/dev/input/%s\n", sysname);
        printf("另开终端执行：getevent -l /dev/input/%s\n", sysname);
    }

    printf("开始注入事件（触摸点击 + 音量键）...\n");
    sleep(1);

    /* ⑤ 注入一次触摸点击：按下 */
    emit(fd, EV_KEY, BTN_TOUCH, 1);
    emit(fd, EV_ABS, ABS_X, SCREEN_W / 2);
    emit(fd, EV_ABS, ABS_Y, SCREEN_H / 2);
    emit(fd, EV_SYN, SYN_REPORT, 0); /* 一帧结束，必须 */

    usleep(100 * 1000);

    /* 触摸抬起 */
    emit(fd, EV_KEY, BTN_TOUCH, 0);
    emit(fd, EV_SYN, SYN_REPORT, 0);

    /* 注入一次音量键：按下 → 抬起 */
    emit(fd, EV_KEY, KEY_VOLUMEUP, 1);
    emit(fd, EV_SYN, SYN_REPORT, 0);
    emit(fd, EV_KEY, KEY_VOLUMEUP, 0);
    emit(fd, EV_SYN, SYN_REPORT, 0);

    sleep(10); /* 给 getevent 观察留时间 */

    /* ⑥ 销毁设备 */
    ioctl_or_die(fd, UI_DEV_DESTROY, 0, "UI_DEV_DESTROY");
    close(fd);
    printf("已销毁虚拟设备，退出。\n");
    return EXIT_SUCCESS;
}
