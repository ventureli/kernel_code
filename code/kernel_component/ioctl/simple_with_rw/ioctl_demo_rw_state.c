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
#define BUF_SIZE 4096

static dev_t dev_num;  //主设备号
static struct cdev my_cdev;
static struct class *my_class;

static int enable_debug_log = 0;
static uint64_t update_timestamp =  0;

struct ioctl_msg {
    int value;
    char msg[64];
    int flag;
};

// static char kernel_buf[BUF_SIZE + 1] = {0};

static ssize_t demo_read(
    struct file *file ,
    char __user *buf, 
    size_t count, 
    loff_t *ppos 
){
    char message[256];
    size_t message_len = 0;
    snprintf(message, sizeof(message), "(enable_debug:%d update_timestamp:%llu)\n", enable_debug_log, update_timestamp);
	message_len = strlen(message);
	if (*ppos >= message_len) {
		return 0;
	}
	if (count > message_len - *ppos) {
		count = message_len - *ppos;
	}
	if (copy_to_user(buf, message + *ppos, count)) {
		pr_err("copy to user error\n");
		return -EFAULT;
	}
	*ppos += count;
	return count; 
 
}
//write 使用
static ssize_t demo_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos) {
     char cmd_buf[64] = {0};
    if (size >= sizeof(cmd_buf)) { 
        pr_info("input cmd  too long max:63 bytes");
        return -EINVAL;
    }
    if (copy_from_user(&cmd_buf, buf, size)) {
        pr_err("copy_error");
        return -EINVAL;
    }
    //remove '\n','\r'
    while (size > 0 && (cmd_buf[size -1] == '\n'  || cmd_buf[size -1] == '\r')){
        cmd_buf[size -1] = '\0';
        size --;
    }
    if (strcmp(cmd_buf, "enable log") == 0) {
        enable_debug_log = 1;
        update_timestamp = jiffies;
    } else if (strcmp(cmd_buf, "disable log") == 0) {
        enable_debug_log = 0;
        update_timestamp = jiffies;
    }else{
        pr_info("unknown cmd");
        return -EINVAL;
    }
    *ppos += size;
    return size;
}  



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
    .read   = demo_read,
    .write  = demo_write,
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
