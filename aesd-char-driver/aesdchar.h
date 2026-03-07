/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#include "aesdconfig.h"
#include "aesd-circular-buffer.h"

struct aesd_dev
{
     struct cdev cdev;                              // Char device structure
     struct aesd_circular_buffer buffer;            // Buffer
     struct aesd_buffer_entry entry_temp;           // Stores working entry until its complete
     //pthread_mutex_t write_mutex;                 // Write lock
};


#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
