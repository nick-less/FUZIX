#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <timer.h>
#include <tty.h>
#include <devtty.h>
#include <rtc.h>

struct blkbuf *bufpool_end = bufpool + NBUFS;	/* minimal for boot -- expanded after we're done with _DISCARD */
uint16_t swap_dev = 0xFFFF;
uint16_t ramtop = 0xE000;
uint8_t need_resched = 0;

uint8_t plt_tick_present;

void plt_discard(void)
{
	while (bufpool_end < (struct blkbuf *) (KERNTOP - sizeof(struct blkbuf))) {
		memset(bufpool_end, 0, sizeof(struct blkbuf));
#if BF_FREE != 0
		bufpool_end->bf_busy = BF_FREE;	/* redundant when BF_FREE == 0 */
#endif
		bufpool_end->bf_dev = NO_DEVICE;
		bufpool_end++;
	}
}

extern uint8_t keycheck(void);

void plt_idle(void)
{
	static uint8_t k;
	uint8_t nk;
	irqflags_t irq = di();
	sync_clock();
	nk = keycheck();
	/* Key up */
	if (nk == 0 && k) {
		tty_inproc(1, k);
		k = 0;
	/* Key down - save the code and wait for it to go back up */
	} else if (k == 0 && nk)
		k = nk;
	irqrestore(irq);
}

uint8_t plt_param(unsigned char *p)
{
	used(p);
	return 0;
}

/* TODO: PIO timer option */
void plt_interrupt(void)
{
}

void do_beep(void)
{
	/* for now */
}