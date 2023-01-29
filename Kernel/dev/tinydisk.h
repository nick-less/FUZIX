/*
 *	Tiny disk driver layer for small systems. Provides the basic blk functionality
 *	but in a lot less space and with limtiations
 */

#ifndef _TINYDISK_H
#define _TINYDISK_H

int td_open(uint8_t minor, uint16_t flag);
int td_read(uint8_t minor, uint8_t rawflag, uint8_t flag);
int td_write(uint8_t minor, uint8_t rawflag, uint8_t flag);

typedef int (*td_xfer)(uint_fast8_t unit, bool is_read, uint32_t block, uint8_t * dptr);

extern uint8_t td_page;
extern uint8_t td_raw;

#ifdef _TINYDISK_PRIVATE

#define MAX_PART	4

extern uint32_t td_lba[CONFIG_TD_NUM][MAX_PART + 1];
extern td_xfer td_op[CONFIG_TD_NUM];
#endif

/* Setup/discard time */
int td_register(td_xfer rwop, uint_fast8_t parts);

#endif
