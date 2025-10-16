/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */

#include "aesd-circular-buffer.h"
#include "aesd_ioctl.h"

#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1  //Remove comment on this line to enable debug

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

struct aesd_dev
{
    /**
     * TODO: Add structure(s) and locks needed to complete assignment requirements
     */
     struct cdev cdev;     /* Char device structure      */
     char *buffer;
     ssize_t buffer_size;
     struct mutex lock;
     struct device *device;
     struct class *class;

     // for buffer
     struct aesd_circular_buffer circ_buffer;
     char *working_entry;
     size_t working_size;

     // for llseek
     size_t total_size;
};


#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
