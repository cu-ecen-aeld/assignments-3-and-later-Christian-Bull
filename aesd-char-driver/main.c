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
#include <linux/slab.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"

#define AESD_BUFFER_SIZE 1024

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Christian Bull");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
struct aesd_dev *dev = &aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;

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
    struct aesd_dev *aesd_device = filp->private_data;
    size_t entry_offset = 0;
    size_t bytes_to_copy;
    struct aesd_buffer_entry *entry;

    PDEBUG("read %zu bytes with f_pos %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    if (mutex_lock_interruptible(&aesd_device->lock))
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circ_buffer, *f_pos, &entry_offset);

    if (!entry) {
        retval = 0;
        goto out;
    }

    bytes_to_copy = entry->size - entry_offset;
    if (count < bytes_to_copy) {
        bytes_to_copy = count;
    }

    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_copy;
    retval = bytes_to_copy;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = count;
    char *kbuf;
    size_t copied = 0, remaining = count;
    const char *newline;
    struct aesd_buffer_entry new_entry;

    PDEBUG("write %zu bytes with f_pos %lld",count,*f_pos);

    pr_info("aesd_write: added entry: size=%zu in_offs=%u out_offs=%u full=%d\n",
        new_entry.size,
        dev->circ_buffer.in_offs,
        dev->circ_buffer.out_offs,
        dev->circ_buffer.full);

    /**
     * TODO: handle write
     * Handle write operation, newline signals complete write
     * save most recent 10 write commands - use circular buffer we already developed
     */


    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        retval = -ENOMEM;
        goto out;
    }

    if (copy_from_user(kbuf, buf, count)) {
        retval = -EFAULT;
        goto free_buf;
    }

    // main loop to append data into device, wait until new line then append, uses buffer from circ-buff
    while (remaining) {
        size_t chunk = remaining;

        dev->working_entry = krealloc(dev->working_entry, dev->working_size + chunk, GFP_KERNEL);

        if (!dev->working_entry) {
            retval = -ENOMEM;
            goto free_buf;
        }

        memcpy(dev->working_entry + dev->working_size, kbuf + copied, chunk);

        dev->working_size += chunk;
        copied += chunk;
        remaining -= chunk;

        newline = memchr(dev->working_entry, '\n', dev->working_size);
        if (newline) {
            size_t entry_size = (newline - dev->working_entry) +1;

            char *entry_buf = kmalloc(entry_size, GFP_KERNEL);
            if(!entry_buf) {
                retval = -ENOMEM;
                goto free_buf;
            }

            memcpy(entry_buf, dev->working_entry, entry_size);

            new_entry.buffptr = entry_buf;
            new_entry.size = entry_size;

            aesd_circular_buffer_add_entry(&dev->circ_buffer, &new_entry);

            {
                size_t leftover = dev->working_size - entry_size;
                memmove(dev->working_entry,
                        dev->working_entry + entry_size,
                        leftover);
                dev->working_size = leftover;
                dev->working_entry = krealloc(dev->working_entry,
                                              dev->working_size,
                                              GFP_KERNEL);
            }
        }
    }

free_buf:
    kfree(kbuf);
out:
    mutex_unlock(&dev->lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
    struct aesd_dev *aesd_device = filp->private_data;
    loff_t newpos;

    mutex_lock(&aesd_device->lock);

    switch (whence) {
        case 0: /* SEEK_SET */
            newpos = off;
            break;
        case 1: /* SEEK_CUR */
            newpos = filp->f_pos + off;
            break;
        case 2: /* SEEK_END */
            newpos = aesd_device->total_size + off; // total bytes written so far
            break;
        default:
            mutex_unlock(&aesd_device->lock);
            return -EINVAL;
    }

    // if newpos is out of range
    if (newpos < 0 || newpos > aesd_device->total_size) {
        mutex_unlock(&aesd_device->lock);
        return -EINVAL;
    }

    filp->f_pos = newpos;
    mutex_unlock(&aesd_device->lock);
    
    return newpos;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    
    size_t i;
    unsigned int buf_index;
    struct aesd_buffer_entry *entry;
    struct aesd_buffer_entry *found_entry;

    size_t total_bytes = 0;
    struct aesd_seekto seekto;
    size_t tmp_offset;

    // switch is best practice despite only having one command
    switch(cmd) {
        case AESDCHAR_IOCSEEKTO:
            // valid command
            PDEBUG("IOCSEEKTO cmd called");

            if (copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto))) {
                return -EFAULT;
            }

            // if it's out of range
            if (seekto.write_cmd >= AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED) {
                return -EINVAL;
            }

            if (mutex_lock_interruptible(&dev->lock)) {
                return -ERESTARTSYS;
            }

            buf_index = dev->circ_buffer.out_offs;

            //
            for (i=0; i < seekto.write_cmd; i++) {
                entry = &dev->circ_buffer.entry[buf_index];
                total_bytes += entry->size;
                buf_index = (buf_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
            }

            entry = &dev->circ_buffer.entry[buf_index];
            if (seekto.write_cmd_offset >= entry->size) {
                mutex_unlock(&dev->lock);
                return -EINVAL;
            }

            total_bytes += seekto.write_cmd_offset;

            found_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circ_buffer, total_bytes, &tmp_offset);

            // no position found
            if (!found_entry) {
                    mutex_unlock(&dev->lock);
                    return -EINVAL;
            }

            filp->f_pos = total_bytes;

            mutex_unlock(&dev->lock);

            return 0;

        default:
            PDEBUG("Incorrect command called");
            return -ENOTTY;
    }
}

// my server app calls fsync but it isn't required for the driver
static int aesd_fsync(struct file *filp, loff_t start, loff_t end, int datasync)
{
    return 0;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .fsync = aesd_fsync,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_ioctl,
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

    // log our device number
    pr_info("aesdchar: registered correctly with major %d minor %d\n",
        MAJOR(dev), MINOR(dev));

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
    
    // aesd specific setup
    mutex_init(&aesd_device.lock);
    aesd_device.buffer = kmalloc(AESD_BUFFER_SIZE, GFP_KERNEL);
    aesd_device.buffer_size = 0;

    // buffer setup
    aesd_circular_buffer_init(&aesd_device.circ_buffer);
    aesd_device.working_entry = NULL;
    aesd_device.working_size = 0;

    return result;
}

void aesd_cleanup_module(void)
{

    struct aesd_dev *dev = &aesd_device;
    struct aesd_buffer_entry *entry;
    uint8_t index;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    pr_info("aesdchar: device cleanup starting\n");

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &dev->circ_buffer, index) {
        if (entry->buffptr) {
            kfree(entry->buffptr);
            entry->buffptr = NULL;
            entry->size = 0;
        }
    }

    if (dev->working_entry) {
        kfree(dev->working_entry);
        dev->working_entry = NULL;
        dev->working_size = 0;
    }

    // cleanup actions
    device_destroy(aesd_device.class, devno);
    class_destroy(aesd_device.class);
    cdev_del(&aesd_device.cdev);
    unregister_chrdev_region(MKDEV(aesd_major, aesd_minor), 1);
    
    pr_info("aesdchar: device cleanup finished\n");
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
