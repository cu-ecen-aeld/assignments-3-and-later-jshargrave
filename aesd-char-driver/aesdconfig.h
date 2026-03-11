/*
    Author: Joseph Hargrave'
    Date: 3/6/2026
*/
#ifndef AESD_CONFIG_DRIVER_AESDCHAR_H_
#define AESD_CONFIG_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1  //Remove comment on this line to enable debug
#define END_WRITE_CHAR '\n'

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#    define ALLOC(size, flags) kmalloc(size, flags);
#    define FREE(ptr) kfree(ptr);
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#    define ALLOC(size, flags) malloc(size, flags);
#    define FREE(ptr) free(ptr);
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#endif /* AESD_CONFIG_DRIVER_AESDCHAR_H_ */