#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <stdbool.h>
#include <devtty.h>
#include <device.h>
#include <vt.h>
#include <tty.h>

#define kbd_read ((volatile uint8_t *)0xE810)
#define kbd_strobe ((volatile uint8_t *)0xE812)
#define irq_check ((volatile uint8_t *)0xE840)
#define irq_reset ((volatile uint8_t *)0xE840)

#define NUM_COLS 80

static char tbuf1[TTYSIZ];
PTY_BUFFERS;

uint8_t vtattr_cap = 0;

static uint8_t *vtmap = 0x8000;

struct s_queue ttyinq[NUM_DEV_TTY + 1] = {	/* ttyinq[0] is never used */
	{NULL, NULL, NULL, 0, 0, 0},
	{tbuf1, tbuf1, tbuf1, TTYSIZ, 0, TTYSIZ / 2},
	PTY_QUEUES
};

tcflag_t termios_mask[NUM_DEV_TTY + 1] = {
	0,
	_CSYS	/* Nothing configurable */
};

/* tty1 is the screen  */

/* Output for the system console (kprintf etc) */
void kputchar(char c)
{
	if (c == '\n')
		tty_putc(1, '\r');
	tty_putc(1, c);
}

unsigned char vt_map_petscii(unsigned char c)
{

	if ((c >= 'A') && (c<='Z')) {
	 	return c + 32;
	}
	if ((c >= 'a') && (c<='z')) {
	 	return c ;
	}
	if (c == 8) {
		return 20;
	}
	if (c == 10) {
		return 13;
	}
	return c;
}


ttyready_t tty_writeready(uint_fast8_t minor)
{
        return TTY_READY_NOW;
}

void tty_putc(uint_fast8_t minor, uint_fast8_t c)
{
        vtoutput(&c,1);
}

void tty_setup(uint_fast8_t minor, uint_fast8_t flags)
{
        minor;
}

void tty_sleeping(uint_fast8_t minor)
{
        minor;
}

int tty_carrier(uint_fast8_t minor)
{
        minor;
        return 1;
}

void tty_data_consumed(uint_fast8_t minor)
{
}
/* Beware - this kbd access also disables 80store */
void tty_poll(void)
{
        uint8_t x;
		// IO_PEEK_ENABLE;
        // x = *kbd_read;
		// IO_PEEK_DISABLE;
//        if (x & 0x80) {
//		tty_inproc(1, 0);
//		IO_PEEK_ENABLE;
//		x = *kbd_strobe;
//		IO_PEEK_DISABLE;
//	}
}

// uint8_t check_timer(void)
// {
// 	/* For now asume mouse card IIc - hack. Once we have proper IRQ
// 	   handling in place we can key this appropriately */
// 	if (*irq_check & 0x80) {
// 		*irq_reset;
// 		return 1;
// 	}
// 	return 0;
// }

// void platform_interrupt(void)
// {
// 	tty_poll();
// 	if (check_timer())
// 		timer_interrupt();
// }


