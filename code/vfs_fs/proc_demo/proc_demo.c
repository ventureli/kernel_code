#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

static struct proc_dir_entry *mytest_dir;
static struct proc_dir_entry *hello_file;

static char msg_buffer[128] =  "hello from /proc/mytest/hell0\n";

static ssize_t  my_read(struct file *flip, char __user *buf, size_t count, loff_t *ppos) {
    size_t len = strlen(msg_buffer);
    if (*ppos >= len) {
        return 0;
    }
    if (copy_to_user(buf, msg_buffer, len)) {
        return -EFAULT;
    }
    *ppos  =  len;
    return len;
}

/*
static ssize_t my_write(struct file *flip, const char __user *buf, size_t count, loff_t *ppos) {
    if (count >= sizeof(msg_buffer)) {
        return -EFAULT;
    }
    if (copy_from_user(msg_buffer, buf, count)) {
        return -EFAULT;
    }
    msg_buffer[count] = '\0';
    return count;
}
*/
static const struct proc_ops proc_fops = {
    .proc_read = my_read,
    // .proc_write = my_write,
};

static int __init mytest_init(void) {
    mytest_dir =  proc_mkdir("mytest", NULL);
    if (!mytest_dir) {
        return -ENOMEM;
    }
    hello_file = proc_create("hello", 0666, mytest_dir, &proc_fops);
    if(!hello_file) {
        proc_remove(mytest_dir);
        return -ENOMEM;
    }
    pr_info("Create /proc/mytest/hello success\n");
    return 0;
}
static void __exit mytest_exit(void) {
    proc_remove(hello_file);
    proc_remove(mytest_dir);
    pr_info("Remove /proc/mytest/hello success");
}
module_init(mytest_init);
module_exit(mytest_exit);
MODULE_LICENSE("GPL");
