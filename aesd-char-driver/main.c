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
#include <linux/fs.h> // file_operations // fs.inode
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "aesdchar.h"
#include "aesd-circular-buffer.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Mcrey Fonacier"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    struct cdev *cdev_ptr = inode->i_cdev;
    //                 for conveniunce          end - start = address       address of aesd_dev + cdev like 0 + 64 gives the maximum meaning end
    struct aesd_dev *device = (struct aesd_dev *)((char *)cdev_ptr - offsetof(struct aesd_dev, cdev));
    filp->private_data = device;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
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

    int rc = 0;
    struct aesd_dev *device = filp->private_data;
    rc = mutex_lock_interruptible(&device->mtx);
    if (rc != 0) { return rc; }

    size_t offset = 0;
    struct aesd_buffer_entry *entry = NULL;
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&device->cb, *f_pos, &offset);
    if (entry != NULL)
    {
        size_t characters = entry->size - offset;
        if (characters > count) { characters = count; }
        rc = copy_to_user(buf, entry->buffptr + offset, characters);
        if (rc != 0)
        {
            mutex_unlock(&device->mtx);
            return rc;
        }
        *f_pos += characters;
        retval = characters;
    }
    mutex_unlock(&device->mtx);

    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */

    int rc = 0;
    struct aesd_dev *device = filp->private_data;
    rc = mutex_lock_interruptible(&device->mtx);
    if(rc != 0){return -EFAULT;}

    size_t characters = device->be.size + count;
    char *buffer =  kmalloc(characters, GFP_KERNEL);

    if(buffer == NULL){
        mutex_unlock(&device->mtx);
        // return rc;
        return -EFAULT; 
    }

    if( device->be.buffptr != NULL ){
        memcpy(buffer, device->be.buffptr, device->be.size);
        kfree(device->be.buffptr);
    }

    device->be.buffptr = buffer;

    rc = copy_from_user(device->be.buffptr + device->be.size, buf, count);

    if( rc != 0 ) {
        mutex_unlock(&device->mtx);
        return -EFAULT;
    }

    device->be.size = characters;

    if (buffer[characters - 1] == '\n'){
        if(device->cb.full) {kfree(device->cb.entry[device->cb.in_offs].buffptr); }
        aesd_circular_buffer_add_entry(&device->cb, &device->be);
        memset(&device->be, 0, sizeof(struct aesd_buffer_entry));
    }

    retval = count;
    mutex_unlock(&device->mtx);
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
    mutex_init(&aesd_device.mtx);
    aesd_circular_buffer_init(&aesd_device.cb);

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

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
    int index = 0;
    struct aesd_buffer_entry *entry = NULL;
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.cb, index)
    {
        if(entry->buffptr != NULL){
            kfree(entry->buffptr);
        }
    }
    mutex_destroy(&aesd_device.mtx);


    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
