/* Enable to make ^Z dump the inode table for debug */
#undef CONFIG_IDUMP
/* Enable to make ^A drop back into the monitor */
#undef CONFIG_MONITOR
/* Profil syscall support (not yet complete) */
#undef CONFIG_PROFIL
/* Acct syscall support */
#undef CONFIG_ACCT
/* Multiple processes in memory at once */
#define CONFIG_MULTI
/* Use fixed banks for now. It's simplest and we've got so much memory ! */
#define CONFIG_BANKS	1
/* Permit large I/O requests to bypass cache and go direct to userspace */
#define CONFIG_LARGE_IO_DIRECT(x)	1

#define CONFIG_CALL_R2L		/* Runtime stacks arguments backwards */

/*
 *	128 RAM (swap yet to do )
 *  common is bottom on this platform
 */
#define CONFIG_BANK_FIXED
#define MAX_MAPS 	3   /* 2 x 32K */
#define MAP_SIZE    0x8000

#define TICKSPERSEC 60	    /* Ticks per second */

/* We've not yet made the rest of the code - eg tricks match this ! */
#define MAPBASE	    0x8000  /* We map from 0x8000 */
#define PROGBASE    0x8000  /* also data base */
#define PROGLOAD    0x8000
#define PROGTOP     0xFE00

#define CONFIG_IDE
#define MAX_BLKDEV 1

/* FIXME: swap */

#define CONFIG_VT

#define BOOT_TTY 513        /* Set this to default device for stdio, stderr */

/* We need a tidier way to do this from the loader */
#define CMDLINE	NULL	  /* Location of root dev name */

/* Device parameters */
#define NUM_DEV_TTY 1
#define TTYDEV   BOOT_TTY /* Device used by kernel for messages, panics */

#define NBUFS    5        /* Number of block buffers */
#define NMOUNTS	 2	  /* Number of mounts at a time */

extern void *memmove(void *dest, const void *src, size_t n);

// we have a 80x25 text mode display at 0x8000
#define CONFIG_VT_SIMPLE
#define VT_BASE ((uint8_t *)0x8000)
#define VT_WIDTH 80
#define VT_RIGHT 80
#define VT_BOTTOM 25

#define platform_discard()
#define platform_copyright() kprintf("plattform copyright")

#define BOOTDEVICENAMES "hd#"
