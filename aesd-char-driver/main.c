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
#include <linux/slab.h>
#include "aesdchar.h"

int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Joseph Hargrave");
struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static int aesd_setup_cdev(struct aesd_dev *dev);
int aesd_init_module(void);
void aesd_cleanup_module(void);

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */

    inode->i_cdev;
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

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    struct aesd_dev *aesd_dev_ptr = (struct aesd_dev *)filp->private_data;
    struct aesd_buffer_entry *read_pointer = NULL;
    size_t char_returned;
    size_t bytes_read = 0;
    size_t bytes_to_copy;
    unsigned long bytes_not_copied;

    if (!access_ok(buf, count))
    {
        PDEBUG("Error: Memory access (%p) to the user space is not accessible!\n\r", buf);
        return -1;
    }

    // Read the contents from the circular buffer
    while(bytes_read < count)
    {
        read_pointer = aesd_circular_buffer_find_entry_offset_for_fpos(aesd_dev_ptr->buffer, (size_t)(f_pos + bytes_read), &char_returned);
        
        // If there are no more entries
        if (read_pointer == NULL)
        {
            break;
        }

        // We can read the amount requested in this loop
        if (read_pointer->size <= count - bytes_read)
        {
            bytes_to_copy = count - bytes_read;
        }
        // Read as many bytes that are in the entry
        else
        {
            bytes_to_copy = read_pointer->size;
        }

        // Copy memory from user space to kernal space
        bytes_not_copied = copy_to_user(buf + bytes_read, read_pointer->buffptr, bytes_to_copy);
        if (bytes_not_copied > 0)
        {
            PDEBUG("Error: failed to copy bytes (%ld/%ld) to the user space!\n\r", bytes_not_copied, count);
            return -ENOMEM;
        }

        // Update bytes read count
        bytes_read += bytes_to_copy;
    }

    // Update the offset
    *f_pos += bytes_read;
    
    // Return bytes read
    return bytes_read;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    struct aesd_dev *aesd_dev_ptr = (struct aesd_dev *)filp->private_data;

    // TODO: implement lock

    char* r_buffer_new = NULL;
    struct aesd_buffer_entry *overwritten_entry = NULL;

    if (!access_ok(buf, count))
    {
        PDEBUG("Error: Memory access (%p) from the user space is not accessible!\n\r", buf);
        return -1;
    }

    // Allocate memory for entry_temp
    if (aesd_dev_ptr->entry_temp == NULL)
    {
        aesd_dev_ptr->entry_temp = ALLOC(sizeof(struct aesd_buffer_entry), GFP_KERNEL);

        // Failed to allocate memory
        if (aesd_dev_ptr->entry_temp == NULL)
        {
            PDEBUG("Error: Memory allocation failed for kernal memory entry!\n\r");
            return -ENOMEM;
        }

        aesd_dev_ptr->entry_temp->buffptr = NULL;
        aesd_dev_ptr->entry_temp->size = 0;
    }

    // Allocate memory for char buffer
    if (aesd_dev_ptr->entry_temp->buffptr == NULL)
    {
        r_buffer_new = ALLOC(count, GFP_KERNEL);
    }
    else
    {
        r_buffer_new = ALLOC(aesd_dev_ptr->entry_temp->size + count, GFP_KERNEL);
    }

    // Failed to allocate memory
    if (!r_buffer_new)
    {
        PDEBUG("Error: Memory allocation failed for kernal memory buffer!\n\r");
        FREE(aesd_dev_ptr->entry_temp->buffptr);
        FREE(aesd_dev_ptr->entry_temp);
        return -ENOMEM;
    }
    
    // Free old buffer and set new char buffer pointer
    FREE(aesd_dev_ptr->entry_temp->buffptr);
    aesd_dev_ptr->entry_temp->buffptr = r_buffer_new;

    // Copy memory from user space to kernal space
    unsigned long bytes_not_copied = copy_from_user((void*)(aesd_dev_ptr->entry_temp->buffptr + aesd_dev_ptr->entry_temp->size), buf, count);
    if (bytes_not_copied > 0)
    {
        PDEBUG("Error: failed to copy bytes (%ld/%ld) from the user space!\n\r", bytes_not_copied, count);
        FREE(aesd_dev_ptr->entry_temp->buffptr);
        FREE(aesd_dev_ptr->entry_temp);
        return -ENOMEM;
    }

    // Set entry size post copy
    aesd_dev_ptr->entry_temp->size += count;

    // Check if the entry is complete
    if (aesd_dev_ptr->entry_temp->buffptr[aesd_dev_ptr->entry_temp->size] == END_WRITE_CHAR)
    {
        // Add entry
        overwritten_entry = aesd_circular_buffer_add_entry(aesd_dev_ptr->buffer, aesd_dev_ptr->entry_temp);
        aesd_dev_ptr->entry_temp = NULL;

        // The write overwrote a entry so we need to free it
        if (overwritten_entry != NULL)
        {
            FREE(overwritten_entry);
        }
    }
    
    return count;
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

    // Allocate memory for circular buffer
    if (aesd_device.buffer == NULL)
    {
        aesd_device.buffer = ALLOC(sizeof(struct aesd_circular_buffer), GFP_KERNEL);

        // Failed to allocate memory
        if (aesd_device.buffer == NULL)
        {
            PDEBUG("Error: Memory allocation failed for kernal memory circular buffer!\n\r");
            return -ENOMEM;
        }
    }

    // Clear circular buffer entrys
    aesd_circular_buffer_init(aesd_device.buffer);

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

    // De-allocate memory for circular buffer
    if (aesd_device.buffer != NULL)
    {
        // De-allocate all the entrys in the buffer
        while(!aesd_device.buffer->empty)
        {
            FREE(aesd_circular_buffer_remove_entry(aesd_device.buffer));
        }

        // De-allocte the circular buffer and 
        FREE(aesd_device.buffer);
    }

    // De-allocate memory for working entry
    if (aesd_device.entry_temp != NULL)
    {
        FREE(aesd_device.entry_temp);
    }

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
