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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/syslog.h>
#endif

#include "aesd-circular-buffer.h"

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
    /**
    * TODO: implement per description
    */

    // this searchs for an item in a buffer and returns a pointer to the buffer entry

    // if buffer or search offset is empty
    if (!buffer || !entry_offset_byte_rtn) {
        return NULL;
    }

    size_t bytes_searched = 0;
    size_t idx = buffer->out_offs;

    // get valid entries in the buffer
    size_t entries;
    if (buffer->full) {
        entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } else if (buffer->in_offs >= buffer->out_offs) {
        entries = buffer->in_offs - buffer->out_offs;
    } else {
        entries = (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED - buffer->out_offs) + buffer->in_offs;
    }
    
    // search for the entry
    size_t i;
    for (i = 0; i < entries; i++) {
        struct aesd_buffer_entry *entry = &buffer->entry[idx];

        if (char_offset < bytes_searched + entry->size) {
            *entry_offset_byte_rtn = char_offset - bytes_searched;
            return entry;
        }

        bytes_searched += entry->size;
        idx = (idx + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
* @returns NULL or, if an existing entry at out_offs was replaced, the value of buffptr for the entry which was
* replaced (for use with dynamic memory allocation)
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
    
    // overwrite oldest entry and update out_offs
    if (buffer->full) {
        buffer->out_offs = (buffer->out_offs +1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // add buffer entry
    buffer->entry[buffer->in_offs] = *add_entry;
    
    buffer->in_offs = (buffer->in_offs +1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
