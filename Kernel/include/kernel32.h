/*
 *	Functionality only used in the 32bit ports
 */

#ifndef __FUZIX__KERNEL_32_H
#define __FUZIX__KERNEL_32_H

/* flat allocators */
extern void *kmalloc(size_t, uint8_t owner);
extern void kfree_s(void *, size_t);
extern unsigned long kmemavail(void);
extern unsigned long kmemused(void);

/* buddymem.c */
extern void buddy_pin(uint16_t node, uint8_t owner);
extern void buddy_init(uint16_t start, uint16_t end);

/* malloc.c */
extern void kmemaddblk(void *, size_t);

/* flat.c */
extern void pagemap_switch(ptptr p, int death);
#if !defined PROGLOAD
extern uaddr_t pagemap_base(void);
#define PROGLOAD udata.u_codebase
#endif

/* On a minimal system these are macros */
#ifndef ugetl
extern uint32_t ugetl(void *uaddr);
extern int uputl(uint32_t val, void *uaddr);
#endif

extern void install_vdso(void);

#endif
