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
#include <linux/device.h>
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic majorr
int aesd_minor =   0;

MODULE_AUTHOR("Mcrey Fonacier"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static struct class *aesd_class;
static struct device *aesd_device_obj;


int aesd_open(struct inode *inode, struct file *filp)
{
    struct cdev *cdev_ptr;
    struct aesd_dev *device;
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    cdev_ptr = inode->i_cdev;
    //                 for conveniunce          end - start = address       address of aesd_dev + cdev like 0 + 64 gives the maximum meaning end
    device = (struct aesd_dev *)((char *)cdev_ptr - offsetof(struct aesd_dev, cdev));
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
    int rc;
    struct aesd_dev *device;
    size_t offset;
    struct aesd_buffer_entry *entry;
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */

    rc = 0;
    device = filp->private_data;



    rc = mutex_lock_interruptible(&device->mtx);
    if (rc != 0) { return rc; }

    offset = 0;
    entry = NULL;
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
    int rc;
    struct aesd_dev *device;
    size_t characters;
    char *buffer;
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */


    device = filp->private_data;
    rc = mutex_lock_interruptible(&device->mtx);
    if(rc != 0){return -EFAULT;}


    characters = device->be.size + count;
    buffer =  kmalloc(characters, GFP_KERNEL);

    if(buffer == NULL){
        mutex_unlock(&device->mtx);
        // return rc;
        return -ENOMEM; 
    }

    if( device->be.buffptr != NULL ){
        memcpy(buffer, device->be.buffptr, device->be.size);
        kfree(device->be.buffptr);
    }

    device->be.buffptr = buffer;

    rc = copy_from_user((void *)(device->be.buffptr + device->be.size), (const void __user *)buf, count);

    if( rc != 0 ) {
        kfree(buffer);
        mutex_unlock(&device->mtx);
        return -EFAULT;
    }

    device->be.size = characters;

    if (buffer[characters - 1] == '\n'){

        if(device->cb.full) {kfree(device->cb.entry[device->cb.in_offs].buffptr); }
        aesd_circular_buffer_add_entry(&device->cb, &device->be);




        device->be.buffptr = NULL; 
        device->be.size = 0;
    }

    retval = count;
    mutex_unlock(&device->mtx);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *device;
    loff_t fpos;
    device = filp->private_data;
    fpos = 0;

    if( mutex_lock_interruptible(&device->mtx) != 0 ){
        return -EINTR;
    }

    switch (whence){
        case SEEK_SET: {
        fpos = offset;
        } break;
        case SEEK_CUR: {
            fpos = filp->f_pos + offset;
        } break;
        case SEEK_END: {
            uint8_t index;
            size_t size = 0;
            uint8_t indexes = device->cb.full ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED : (device->cb.in_offs - device->cb.out_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
            
            for ( index = 0; index < indexes; index += 1){
                size += device->cb.entry[(device->cb.out_offs + index) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED].size;
            }
            fpos = size + offset;
        } break;
        default: {
            return -SEEK_END;
        } break;
    }

    if (fpos >= 0)
    {
        filp->f_pos = fpos;
    }

    mutex_unlock(&device->mtx);
    return fpos;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg){
    struct aesd_dev *device;
    struct aesd_seekto st;
    long fpos;
    device = filp->private_data;
    fpos = 0;

    switch (cmd) {
        case AESDCHAR_IOCSEEKTO: {
            uint8_t indexes;
            uint8_t i;
            uint8_t minuser;
            uint8_t cur_out_offs;
            uint8_t cur_in_offs;
            if( copy_from_user(&st, (const void __user *)arg, sizeof(struct aesd_seekto) ) != 0 ){
                return -EFAULT;
            }
            if( mutex_lock_interruptible(&device->mtx) != 0 ){
                return -EINTR;
            }

            if( device->cb.full ){
                indexes = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
            }
            else{
                indexes = (device->cb.in_offs - device->cb.out_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
            }

            if( st.write_cmd >= indexes ){
                mutex_unlock(&device->mtx);
                return -EINVAL;
            }


            minuser = 0;
            cur_out_offs = device->cb.out_offs;
            cur_in_offs = device->cb.in_offs;

            for( i = 0; i < st.write_cmd; i+=1 ){

                if( (i + cur_out_offs) >= st.write_cmd){
                    cur_out_offs = 0;
                    indexes = i;
                }
                fpos += device->cb.entry[i + cur_out_offs - (minuser)].size;
            }

            if( st.write_cmd_offset >= device->cb.entry[i].size ){
                mutex_unlock(&device->mtx);
                return -EINVAL;
            }
            fpos += st.write_cmd_offset;

            // i dont know what the hell is this
            filp->f_pos = fpos;



            mutex_unlock(&device->mtx);


        } break;
        default: {
            fpos = -ENOTTY;
            return fpos;
        } break;
    }


    return fpos;
}


struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
    .llseek = aesd_llseek,
    .unlocked_ioctl = aesd_unlocked_ioctl,
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
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        pr_warn(KERN_WARNING "Can't get major %d\n", aesd_major);
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
        return result;
    }

    aesd_class = class_create(THIS_MODULE, "aesd");
    if (IS_ERR(aesd_class)){
        pr_err("aesdchar: class_create failed\n");
        cdev_del(&aesd_device.cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(aesd_class);
    }

    aesd_device_obj = device_create(aesd_class, NULL, dev, NULL, "aesdchar");
    if (IS_ERR(aesd_device_obj)) {
        pr_err("aesdchar: device_create failed\n");
        class_destroy(aesd_class);
        cdev_del(&aesd_device.cdev);
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(aesd_device_obj);
    }

    pr_info("aesdchar: registered device /dev/aesdchar major=%d\n", MAJOR(dev));
    return 0;

}

void aesd_cleanup_module(void)
{
    int index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);
    if (aesd_device_obj) 
        device_destroy(aesd_class, devno); 
    if (aesd_class) 
        class_destroy(aesd_class);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */

    cdev_del(&aesd_device.cdev);
    if(aesd_device.be.buffptr != NULL){
        kfree(aesd_device.be.buffptr); 
        aesd_device.be.buffptr = NULL; 
        aesd_device.be.size = 0;
    }

    index = 0;
    entry = NULL;
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
