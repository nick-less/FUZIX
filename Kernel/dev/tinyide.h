#ifndef TINYIDE_H
#define TINYIDE_H

/* SDCC does I/O space weirdly. An __sfr __at x is a reference to the space
   not a pointer */
#ifdef CONFIG_TINYIDE_SDCCPIO
#define ide_read(x)	(x)
#define ide_write(x,y)	(x) = (y)
#else
#define	ide_read(x)	(*(x))
#define ide_write(x,y)	(*(x) = (y))
#endif

/* Assembler glue */
extern void devide_read_data(uint8_t *p);
extern void devide_write_data(uint8_t *p);
int ide_xfer(uint_fast8_t unit, bool is_read, uint32_t lba, uint8_t * dptr);
extern uint8_t ide_master;
extern uint8_t ide_slave;

void ide_probe(void);

#endif
