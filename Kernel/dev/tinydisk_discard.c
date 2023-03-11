/*
 *	A minimal implementation of MBR parsing for small systems. Only looks
 *	for primary partitions but does deal with swap finding.
 */

#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#define _TINYDISK_PRIVATE
#include <tinydisk.h>
#include "mbr.h"

#ifdef CONFIG_DYNAMIC_SWAP
static void swap_found(uint_fast8_t minor, partition_table_entry_t * pe)
{
	uint32_t off;
	uint16_t n = 0;
	if (swap_dev != 0xFFFF)
		return;
	kputs("(swap) ");
	swap_dev = minor;	/* major is 0 */
	off = le32_to_cpu(pe->lba_count);

	while (off > SWAP_SIZE && n < MAX_SWAPS) {
		off -= SWAP_SIZE;
		n++;
	}
	while (n)
		swapmap_init(--n);
}
#endif

#ifdef CONFIG_DYNAMIC_PAGE
static void swap_found(uint_fast8_t minor, partition_table_entry_t * pe)
{
	uint32_t off;

	if (swap_dev != 0xFFFF)
		return;
	kputs("(page) ");
	swap_dev = minor;	/* major is 0 */
	off = le32_to_cpu(pe->lba_count);

	pagefile_add_blocks(off);
}
#endif

static uint_fast8_t tinydisk_setup(uint16_t dev)
{
	uint32_t *lba = td_lba[dev];
	uint_fast8_t n = 0;
	uint_fast8_t c = 0;
	boot_record_t *br = (boot_record_t *) tmpbuf();
	partition_table_entry_t *pe = br->partition;
	udata.u_block = 0;
	udata.u_nblock = 1;
	udata.u_dptr = (void *) br;
	if (td_read(dev << 4, 0, 0) != BLKSIZE) {
		tmpfree(br);
		return 0;
	}
	kprintf("hd%c: ", 'a' + dev);

	if (le16_to_cpu(br->signature) == MBR_SIGNATURE) {
		while (n < 4) {
			if (pe->type_chs_last[0]) {
				kprintf("hd%c%d ", 'a' + dev, ++c);
				*++lba = le32_to_cpu(pe->lba_first);
			}
#if defined(CONFIG_DYNAMIC_SWAP) || defined(CONFIG_DYNAMIC_PAGE)
			if (pe->type_chs_last[0] == FUZIX_SWAP)
				swap_found((dev << 4) | c, pe);
#endif
			n++;
			pe++;
		}
	}
	tmpfree(br);
	kputchar('\n');
	return 1;
}

static uint8_t ntd;

int td_register(td_xfer rwop, uint_fast8_t parts)
{
	if (ntd == CONFIG_TD_NUM)
		return -2;
	td_op[ntd] = rwop;
	if (parts) {
		if (!tinydisk_setup(ntd)) {
			td_op[ntd] = NULL;
			return -1;
		}
	}
	return ntd++;
}
