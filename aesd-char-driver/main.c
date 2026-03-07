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

#include "aesdchar.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joseph Hargrave");
MODULE_DESCRIPTION("AESD character driver");

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    // Assign dev pointer into private data for other methods
    struct aesd_dev *aesd_dev_ptr = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = aesd_dev_ptr;

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    struct aesd_dev *aesd_dev_ptr = filp->private_data;
    struct aesd_buffer_entry *read_pointer = NULL;
    size_t char_returned = 0;
    size_t bytes_read = 0;
    size_t bytes_to_copy = 0;
    size_t entry_bytes = 0;
    unsigned long bytes_not_copied;

    // Get lock
    int get_lock_rv = get_lock(&aesd_dev_ptr->buffer_lock);
    if (get_lock_rv != 0)
    {
        PDEBUG("Error: failed to get lock!\n\r");
        return -ERESTARTSYS;
    }

    // Read the contents from the circular buffer
    while(bytes_read < count)
    {
        read_pointer = aesd_circular_buffer_find_entry_offset_for_fpos(&aesd_dev_ptr->buffer, (size_t)(*f_pos + bytes_read), &char_returned);
        
        // If there are no more entries
        if (read_pointer == NULL)
        {
            break;
        }

        // Number of bytes in entry starting from offset
        entry_bytes = read_pointer->size - char_returned;

        // We can read the amount requested in this loop
        if (entry_bytes >= count - bytes_read)
        {
            bytes_to_copy = count - bytes_read;
        }
        // Read as many bytes that are in the entry
        else
        {
            bytes_to_copy = entry_bytes;
        }

        // Copy memory from kernal space to user space
        bytes_not_copied = copy_to_user(buf + bytes_read, read_pointer->buffptr + char_returned, bytes_to_copy);
        if (bytes_not_copied > 0)
        {
            PDEBUG("Error: failed to copy bytes (%lu/%zu) to the user space!\n\r", bytes_not_copied, count);
            release_lock(&aesd_dev_ptr->buffer_lock);
            return -EFAULT;
        }

        // Update bytes read count
        bytes_read += bytes_to_copy;
    }

    // Update the offset
    *f_pos += bytes_read;

    // Release lock
    release_lock(&aesd_dev_ptr->buffer_lock);
    
    // Return bytes read
    return bytes_read;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    struct aesd_dev *aesd_dev_ptr = filp->private_data;
    char* new_buffer = NULL;
    char* overwritten_entry = NULL;
    unsigned long bytes_not_copied = 0;

    // Get lock
    int get_lock_rv = get_lock(&aesd_dev_ptr->buffer_lock);
    if (get_lock_rv != 0)
    {
        PDEBUG("Error: failed to get lock!\n\r");
        return -ERESTARTSYS;
    }

    // Allocate memory for char buffer
    if (aesd_dev_ptr->entry_temp.buffptr == NULL)
    {
        new_buffer = ALLOC(count, GFP_KERNEL);
    }
    else
    {
        new_buffer = ALLOC(aesd_dev_ptr->entry_temp.size + count, GFP_KERNEL);
    }

    // Failed to allocate memory
    if (!new_buffer)
    {
        PDEBUG("Error: Memory allocation failed for kernal memory buffer!\n\r");
        release_lock(&aesd_dev_ptr->buffer_lock);
        return -ENOMEM;
    }
    
    // Copy old buffer if there is content already in entry_temp
    if (aesd_dev_ptr->entry_temp.buffptr != NULL)
    {
        memcpy(new_buffer, aesd_dev_ptr->entry_temp.buffptr, aesd_dev_ptr->entry_temp.size);
    }

    // Copy memory from user space to kernal space
    bytes_not_copied = copy_from_user((void*)(new_buffer + aesd_dev_ptr->entry_temp.size), buf, count);
    if (bytes_not_copied > 0)
    {
        PDEBUG("Error: failed to copy bytes (%lu/%zu) from the user space!\n\r", bytes_not_copied, count);
        release_lock(&aesd_dev_ptr->buffer_lock);
        FREE(new_buffer);
        return -EFAULT;
    }

    // Free old buffer
    if (aesd_dev_ptr->entry_temp.buffptr != NULL)
    {
        FREE(aesd_dev_ptr->entry_temp.buffptr);
    }

    // Set to the new buffer
    aesd_dev_ptr->entry_temp.buffptr = new_buffer;

    // Set entry size post copy
    aesd_dev_ptr->entry_temp.size += count;

    // Check if the entry is complete
    if (aesd_dev_ptr->entry_temp.buffptr[aesd_dev_ptr->entry_temp.size - 1] == END_WRITE_CHAR)
    {
        // Add entry
        overwritten_entry = aesd_circular_buffer_add_entry(&aesd_dev_ptr->buffer, &aesd_dev_ptr->entry_temp);
        aesd_dev_ptr->entry_temp.buffptr = NULL;
        aesd_dev_ptr->entry_temp.size = 0;

        // The write overwrote a entry so we need to free it
        if (overwritten_entry != NULL)
        {
            FREE(overwritten_entry);
        }
    }

    // Release lock
    release_lock(&aesd_dev_ptr->buffer_lock);
    
    return count;
}

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

    // Reset Circular Buffer
    aesd_circular_buffer_init(&aesd_device.buffer);
    aesd_device.entry_temp.buffptr = NULL;
    aesd_device.entry_temp.size = 0;

    // Mutex init
    mutex_init(&aesd_device.buffer_lock);

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

    // De-allocate all the entrys in the buffer
    while(!aesd_device.buffer.empty)
    {
        FREE(aesd_circular_buffer_remove_entry(&aesd_device.buffer)->buffptr);
    }

    // Reset Circular Buffer
    aesd_circular_buffer_init(&aesd_device.buffer);

    // Reset working entry
    FREE(aesd_device.entry_temp.buffptr);
    aesd_device.entry_temp.buffptr = NULL;
    aesd_device.entry_temp.size = 0;

    unregister_chrdev_region(devno, 1);
}


int get_lock(struct mutex* m)
{
    // Get lock
    int return_value = mutex_lock_interruptible(m);
    if (return_value)
    {
        return -ERESTARTSYS;
    }
    return 0;
}

void release_lock(struct mutex* m)
{
    // Release lock
    mutex_unlock(m);
}


module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
