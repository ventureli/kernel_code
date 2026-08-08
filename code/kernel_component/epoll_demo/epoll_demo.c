
#define pr_fmt(fmt) "WQTK: " fmt
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/version.h>

#define DEVICE_NAME "epoll_char_dev"

#define BUFFER_SIZE 1024

static dev_t dev_num;  //设备号  
static struct cdev cdev;  //字符设备
static struct class *my_class;  //字符设备分类
static char buffer[BUFFER_SIZE] = {0};  //缓冲区
static int buffer_len = 0;   //buffer 长度
static int read_pos = 0;     //读取位置
static int write_pos = 0;    //写入位置

static DECLARE_WAIT_QUEUE_HEAD(read_wait);
static DECLARE_WAIT_QUEUE_HEAD(write_wait);

static unsigned int read_aviailable = 0;

// #define pr_fmt(fmt) "WQTK" fmt


static void wqtk_pr_info(const char * fmt, ...) {
  
    va_list args;
    char buf[1024];
    va_start(args, fmt);

    vsnprintf(buf , sizeof(buf),fmt,args);
    pr_info("WQTK:%s",buf);
    
    va_end(args);
}
 

//read 函数
ssize_t char_dev_read(struct file *flip, char __user *buf, size_t count, loff_t * f_pos) {
    int bytes_read = 0;
    wqtk_pr_info("begin read over{%d, %d}", buffer_len, read_aviailable);
    wait_event_interruptible(read_wait, read_aviailable > 0); //第一个是等待队列，第二个是等待的条件 
    while(read_aviailable > 0 && bytes_read < count) { //这个开始多线程
        buffer_len --;
        read_aviailable --;
        char c  = buffer[read_pos ++];
        read_pos =  read_pos % BUFFER_SIZE;  //这里个处理
        if (put_user(c, buf + bytes_read)) {
            return -EFAULT;
        }
        bytes_read ++;
    }
    wqtk_pr_info("read over{%d, %d}", buffer_len, read_aviailable);
    return bytes_read;
}
//write 函数

//这个可以做一个先入先出的管道
ssize_t char_dev_write(struct file *flip, const char __user *buf, size_t count, loff_t *f_pos) {
    int bytes_written = 0;
    while(bytes_written < count) {
        if(buffer_len >= BUFFER_SIZE) {
            if(flip->f_flags & O_NONBLOCK) { //如果是非租车的直接返回
                return -EAGAIN;
            }
            //这个函数非常的灵活
            
            wqtk_pr_info("begin to wait write");
            wait_event_interruptible(write_wait, buffer_len < BUFFER_SIZE);  //这里事一个字节，一个自己的拷贝。
        }
        char c;
        if (get_user(c, buf + bytes_written)) {
            return -EFAULT;
        }
        buffer[write_pos ++] = c;
        write_pos %= BUFFER_SIZE; //做一个循环队列
        buffer_len ++;
        read_aviailable ++;
        bytes_written ++;
        wake_up_interruptible(&read_wait);
    }
    wqtk_pr_info("write over{%d, %d}", buffer_len, read_aviailable);
    return bytes_written;
}

//poll函数

unsigned int char_dev_poll(struct file *flip, poll_table *wait) {
    unsigned int mask = 0;
    if (flip->f_mode & FMODE_READ) {
      poll_wait(flip,&read_wait, wait);
    }
	if (flip->f_mode & FMODE_WRITE){
     poll_wait(flip,&write_wait, wait);  
    }

		
    //一个读，一个写
    if (read_aviailable > 0) {
        mask |= POLLIN | POLLRDNORM;  //刻度

    }
    if (buffer_len < BUFFER_SIZE) {
        mask |= POLLOUT | POLLWRNORM;
    }
    wqtk_pr_info("char_dev_poll called %d", mask);
    return mask;
}


// 文件操作结构体
static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = char_dev_read,
    .write = char_dev_write,
    .poll = char_dev_poll,  //这个很重要，注册epoll函数
};

// 模块初始化
static int __init char_dev_init(void)
{
    int ret;
    // 注册字符设备
    
    printk(KERN_INFO "WQTK char_dev_init------------");
    printk(KERN_INFO "WQTK char_dev_init------------");
    printk(KERN_INFO "WQTK char_dev_init------------");
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate char device region\n");
        return ret;
    }

    cdev_init(&cdev, &fops);
    ret = cdev_add(&cdev, dev_num, 1);
    if (ret < 0) {
        pr_err("Failed to add cdev\n");
        goto error_cdev;
    }
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)
        my_class = class_create("char_dev_class");
    #else 
         my_class = class_create(THIS_MODULE,"char_dev_class");
    #endif 
    if (IS_ERR(my_class)) {
        pr_err("Failed to create class\n");
        goto error_class;
    }

    device_create(my_class, NULL, dev_num, NULL, DEVICE_NAME);

    wqtk_pr_info("Char device module loaded. Device: /dev/%s\n", DEVICE_NAME);
    return 0;

error_class:
    cdev_del(&cdev);
error_cdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

// 模块卸载
static void __exit char_dev_exit(void)
{
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&cdev);
    unregister_chrdev_region(dev_num, 1);
    wqtk_pr_info("Char device module unloaded /dev/epoll_char_dev \n");
}

module_init(char_dev_init);
module_exit(char_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A char device with read/write and epoll support");

