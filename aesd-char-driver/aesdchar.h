/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/mutex.h>

#include "aesdconfig.h"
#include "aesd-circular-buffer.h"

struct aesd_dev
{
     struct cdev cdev;                             // Char device structure
     struct aesd_circular_buffer buffer;           // Buffer
     struct aesd_buffer_entry entry_temp;          // Stores working entry until its complete
     struct mutex buffer_lock;                     // Buffer lock
};

// Global variables
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;
struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
static int aesd_setup_cdev(struct aesd_dev *dev);
int aesd_init_module(void);
void aesd_cleanup_module(void);
int get_lock(struct mutex* m);
void release_lock(struct mutex* m);

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
