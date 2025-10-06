/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Christian Bull");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    
    struct aesd_dev *dev;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    file->private_data = dev;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    
    // nothing is needed here

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    struct aesd_dev *aesd_device = filp->private_data;

    if (mutex_lock_interruptible(&aesd_device->lock))
        retvalurn -ERESTARTSYS;

    if (*offset >= aesd_device->buffer_size) {
        retval = 0;
        goto out;
    }

    if (len > aesd_device->buffer_size - *offset)
        len = aesd_device->buffer_size - *offset;

    if (copy_to_user(buf, aesd_device->buffer + *offset, len)) {
        retval = -EFAULT;
        goto out;
    }

    *offset += len;
    retval = len;

    out:
        mutex_unlock(&aesd_device->lock);
        return retval;

}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     * Handle write operation, newline signals complete write
     * save most recent 10 write commands - use circular buffer we already developed
     */
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */
    result = aesd_setup_cdev(&aesd_device);

    if ( result ) {
        printk(KERN_WARNING "Error setting up aesd dev %d\n", result);
        unregister_chrdev_region(dev, 1);
        return result;
    }

    // create class
    aesd_device.class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(aesd_device.class)) {
        cdev_del(&aesd_device);
        unregister_chrdev_region(dev, 1);
        printk(KERN_WARNING "Error creating class");
        return PTR_ERR(aesd_device->class);
    }

    // create device node in /dev
    aesd_device.device = device_create(my_class, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(aesd_device.device)) {
        class_destroy(aesd_device.class);
        cdev_del(&aesd_device);
        unregister_chrdev_region(dev, 1);
        printk(KERN_WARNING "Failed to create device");
        return PTR_ERR(&aesd_device);
    }

    return result;
}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    // cleanup actions
    cdev_del(&aesd_device.cdev);
    unregister_chrdev_region(devno, 1);
    device_destroy(aesd_device.class, devno);
    kfree(aesd_device.buffer);
    class_destroy(aesd_device.class);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
