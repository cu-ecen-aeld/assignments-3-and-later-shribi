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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include <linux/uaccess.h>
#include <linux/slab.h>

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

#define __KERNEL__ 1
#define ALLOC_SIZE 1024 

MODULE_AUTHOR("Srinidhi Bhat"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    // Doing this is not needed since, we are supporting only a single device instance
    // But following the standard pattern for character devices, we still retrieve the device structure from the inode.
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    PDEBUG("private_data set to aesd_device");
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    filp->private_data = NULL;
    PDEBUG("private_data cleared");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev;
    dev = filp->private_data;
    ssize_t retval = 0;
    
    // lock the device mutex
    mutex_lock(&dev->lock);
    
    struct aesd_buffer_entry *entry;
    size_t entry_offset_byte_rtn;
    size_t entry_offset = *f_pos;
    size_t bytes_to_copy;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, entry_offset, &entry_offset_byte_rtn);
    if (entry == NULL) {
        mutex_unlock(&dev->lock);
        return 0;
    }
    else if (entry_offset_byte_rtn < entry->size) {
        bytes_to_copy = min(count, entry->size - entry_offset_byte_rtn);
        if (copy_to_user(buf, entry->buffptr + entry_offset_byte_rtn, bytes_to_copy)) {
            mutex_unlock(&dev->lock);
            return -EFAULT;
        }
        *f_pos += bytes_to_copy;
        retval = bytes_to_copy;
    }
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    struct aesd_dev *dev;
    dev = filp->private_data;
    ssize_t retval = -ENOMEM;

    // lock the device mutex
    mutex_lock(&dev->lock);
    dev->can_write = false;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    dev->pending_buffer_size += count;
    dev->buffer = krealloc(dev->buffer, dev->pending_buffer_size, GFP_KERNEL);
    if (dev->buffer == NULL) {
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }

    // copy from user buffer to device buffer
    if (copy_from_user(dev->buffer + dev->pending_buffer_size - count, buf, count)) {
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }

    // check if the data is ending with a newline character
    if (dev->buffer[dev->pending_buffer_size - 1] == '\n') {
        dev->can_write = true;
    }
    else {
        dev->can_write = false;
    }

    retval = count;

    if (dev->can_write) {
        struct aesd_buffer_entry new_entry;
        const char *to_free = NULL;

        if (dev->circular_buffer.full) {
            to_free = dev->circular_buffer.entry[dev->circular_buffer.out_offs].buffptr;
        }

        new_entry.buffptr = dev->buffer;
        new_entry.size = dev->pending_buffer_size;
        aesd_circular_buffer_add_entry(&dev->circular_buffer, &new_entry);

        // reset the pending buffer since it has been added to the circular buffer
        retval = dev->pending_buffer_size;
        dev->pending_buffer_size = 0;
        dev->buffer = NULL;
        kfree(to_free);
    }

    // unlock the device mutex
    mutex_unlock(&dev->lock);
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
    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.circular_buffer);
    aesd_device.buffer = kmalloc(ALLOC_SIZE, GFP_KERNEL);
    if (aesd_device.buffer == NULL) {
        return -ENOMEM;
    }
    aesd_device.pending_buffer_size = 0;
    
    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    cdev_del(&aesd_device.cdev);

    aesd_circular_buffer_cleanup(&aesd_device.circular_buffer);
    if (aesd_device.buffer) {
        kfree(aesd_device.buffer);
        aesd_device.buffer = NULL;
    }
    aesd_device.pending_buffer_size = 0;
    mutex_destroy(&aesd_device.lock);
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
