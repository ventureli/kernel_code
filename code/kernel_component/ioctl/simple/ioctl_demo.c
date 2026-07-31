#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/ioctl.h>

#include <linux/version.h>
#define MY_IOCTL_MAGIC 'k'
#define MY_IOCTL_HELLO _IO(MY_IOCTL_MAGIC, 0)
#define MY_IOCTL_ECHO _IOWR(MY_IOCTL_MAGIC, 1, int)


static dev_t dev_num;  //主设备号
static struct cdev my_cdev;
static struct class *my_class;

struct ioctl_msg {
    int value;
    char msg[64];
    int flag;
};


//处理函数
long my_ioctl(struct file *flip, unsigned int cmd, unsigned long arg)
{
    struct ioctl_msg value;
    switch (cmd)
    {
        case  MY_IOCTL_HELLO:
            pr_info("Hello from kernel module !\n");
            break;
            return 0;
        case MY_IOCTL_ECHO:
            if(copy_from_user(&value,(struct ioctl_msg __user *)arg, sizeof(value))) {
                return -EFAULT;
            }
            pr_info("kernel return : {%d %s %d}\n",value.value, value.msg, value.flag);
            value.value += 10;
            snprintf(value.msg, sizeof(value.msg), "%s(%d)",value.msg ,value.value);
            value.flag = 1;
            if(copy_to_user((struct ioctl_msg __user *)arg , &value, sizeof(value))){
                return -EFAULT;
            }
            return 0;
        default:
            return -ENOTTY;
    } 
    return 0;
}

//文件操作结构体
static const struct file_operations fops ={
    .owner = THIS_MODULE,
    .unlocked_ioctl = my_ioctl,
};

static int __init ioctl_module_init(void) 
{
    int ret;
    //注册字符设备
    ret = alloc_chrdev_region(&dev_num,0, 1, "ioctl_module");
    if(ret < 0) {
        pr_err("Failed to allocate char device region\n");
        return ret;
    }
    cdev_init(&my_cdev, &fops);
    ret = cdev_add(&my_cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto error_cdev;
    }
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0) 
        my_class = class_create("ioctl_module_class");
    #else  
        my_class = class_create(THIS_MODULE, "ioctl_module_class");
    #endif 
    if (IS_ERR(my_class)) {
        pr_err("Failed to create \n");
        goto error_class;
    }
    device_create(my_class, NULL, dev_num, NULL, "ioctl_dev");
    pr_info("IOCTL module loaded. Device:/dev/ioctl_dev\n");
    return 0;
    error_class:
        cdev_del(&my_cdev);
    error_cdev:
        unregister_chrdev_region(dev_num, 1);
        return ret;
}
static void __exit ioctl_module_exit(void)
{
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("IOCTL module unloaded\n");
}

module_init(ioctl_module_init);
module_exit(ioctl_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("YOUR NAME");
MODULE_DESCRIPTION("IOCTL module without modifying the kernel");
