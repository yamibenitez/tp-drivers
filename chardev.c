#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <asm/errno.h>

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

#define SUCCESS 0
#define DEVICE_NAME "char_dev"
#define BUF_LEN 80

static int major;

enum {
        CDEV_NOT_USED = 0,
        CDEV_EXCLUSIVE_OPEN = 1,
};

static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static char msg[BUF_LEN + 1];

static struct class *cls;

static struct file_operations chardev_fops = {
        .read = device_read,
        .write = device_write,
        .open = device_open,
        .release = device_release,
};

static int __init chardev_init(void) {
        major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

        if (major < 0) {
        	pr_alert("Registro del char device fallido con %d\n", major);
                return major;
        }

        cls = class_create(THIS_MODULE, DEVICE_NAME);
        device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
	pr_info("EL numero mayor asignado al controlador es %d.\n", major);
        pr_info("Para comunicarse con el char device, crear el archivo");
        pr_info("mknod %s c %d 0\n", DEVICE_NAME, major);
        return 0;
}

static void __exit chardev_exit(void) {
        device_destroy(cls, MKDEV(major, 0));
        class_destroy(cls);
        unregister_chrdev(major, DEVICE_NAME);
}

static int device_open(struct inode *inode, struct file *file) {
        pr_info("device_open(%p)\n", file);
	
	if(atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
		return -EBUSY;
		
        try_module_get(THIS_MODULE);
        return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file) { 
        atomic_set(&already_open, CDEV_NOT_USED);
        module_put(THIS_MODULE);
        return SUCCESS;
}

static ssize_t device_read(struct file *file, char __user *buffer, size_t length, loff_t *offset) {
	int bytes_read = 0;
	int msg_len = strlen(msg);

        if (*offset >= msg_len || !*(msg + *offset)) {
                *offset = 0;
                return 0;
        }

        int start_pos = msg_len - 1 - *offset;

        while (length && start_pos >= 0) {
                put_user(msg[start_pos--], buffer++); 
                length--;
		bytes_read++;
        }
        pr_info("Read %d bytes, %ld left\n", bytes_read, length);
        *offset += bytes_read;

        return bytes_read;
}

static ssize_t device_write(struct file *file, const char __user *buffer, size_t length, loff_t *offset) {
        int i;

        for (i = 0; i < length && i < BUF_LEN; i++) {
                get_user(msg[i], buffer + i);
	}

        pr_info("Mensaje: %s\n", msg);
        return i;
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");

