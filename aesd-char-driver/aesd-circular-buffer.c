/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

void aesd_buffer_print(struct aesd_circular_buffer *buffer)
{
    PDEBUG("Printing Buffer\n\r");
    size_t index = 0;
    struct aesd_buffer_entry* temp_ptr = NULL;
    
    AESD_CIRCULAR_BUFFER_FOREACH(temp_ptr, buffer, index)
    {
        if (temp_ptr->buffptr != NULL)
        {
            PDEBUG("index %ld: %s", index, temp_ptr->buffptr);
        }
        else
        {
            PDEBUG("index %ld: (none)\n\r", index);
        }
    }
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    size_t char_offset_left = char_offset;
    struct aesd_buffer_entry* temp_ptr = NULL;
    for (size_t count = 0, index = buffer->out_offs; count < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; count++)
    {
        temp_ptr = &buffer->entry[index];
    
        // Found the entry
        if (char_offset_left < temp_ptr->size)
        {
            *entry_offset_byte_rtn = char_offset_left;
            return temp_ptr;
            break;
        }

        // Decrement by the offset in entry
        char_offset_left -= temp_ptr->size;

        // Check if its time for wrap around
        if (index == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
        {
            index = 0;
        }
        else
        {
            index++;
        }

    }

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
struct aesd_buffer_entry* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    struct aesd_buffer_entry *overwritten_entry =  &buffer->entry[buffer->out_offs];

    // Write to buffer
    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;
    buffer->empty = false;

    // Check if its time for wrap around
    if (buffer->in_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
    {
        buffer->in_offs = 0;
    }
    else
    {
        buffer->in_offs++;
    }

    // If the buffer is full then increment read offset to point to the oldest entry after overwrite
    if (buffer->full)
    {
         // Check if its time for wrap around
        if (buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
        {
            buffer->out_offs = 0;
        }
        else
        {
            buffer->out_offs++;
        }
        return overwritten_entry;
    }

    // If write offset matches read offset then the buffer is full
    else if (buffer->in_offs == buffer->out_offs)
    {
        buffer->full = true;
    }

    return NULL;
}

struct aesd_buffer_entry* aesd_circular_buffer_remove_entry(struct aesd_circular_buffer *buffer)
{
    struct aesd_buffer_entry* return_ptr = NULL;

    // Return null if buffer is empty
    if (buffer->empty == false)
    {
        return NULL;
    }

    // Entry to return
    return_ptr = &buffer->entry[buffer->out_offs];

    // Check if its time for wrap around
    if (buffer->out_offs == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - 1)
    {
        buffer->out_offs = 0;
    }
    else
    {
        buffer->out_offs++;
    }

    // If read offset matches write offset then the buffer is empty
    if (buffer->in_offs == buffer->out_offs)
    {
        buffer->empty = true;
    }

    return return_ptr;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
