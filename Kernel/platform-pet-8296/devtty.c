#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <stdbool.h>
#include <devtty.h>
#include <device.h>
#include <vt.h>
#include <tty.h>
#include "config.h"

#define kbd_read ((volatile uint8_t *)0xE812)
#define kbd_strobe ((volatile uint8_t *)0xE810)
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

static uint8_t kbd_timer;
uint8_t keyboard[11][8];
uint8_t shiftkeyboard[11][8];
uint8_t keymap[11];
struct vt_repeat keyrepeat;
uint8_t vtattr_cap;

static uint8_t keyin[11];
static uint8_t keybyte, keybit;
static uint8_t newkey;
static int keysdown = 0;
static uint8_t shiftmask[11] = {
	0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0
};

static void keyproc(void)
{
	int i;
	uint8_t key;

	for (i = 0; i < 11; i++) {
		key = keyin[i] ^ keymap[i];
		if (key) {
			int n;
			int m = 1;
			for (n = 0; n < 8; n++) {
				if ((key & m) && (keymap[i] & m)) {
					if (!(shiftmask[i] & m))
						keysdown--;
				}
				if ((key & m) && !(keymap[i] & m)) {
					if (!(shiftmask[i] & m)) {
						keysdown++;
						newkey = 1;
						keybyte = i;
						keybit = n;
					}
				}
				m += m;
			}
		}
		keymap[i] = keyin[i];
	}
}

static uint8_t capslock = 0;

static void keydecode(void)
{
	uint8_t c;

	if (keybyte == 6 && keybit == 3) {
		capslock = 1 - capslock;
		return;
	}

	if (keymap[6] & 3 ) {	/* shift or control */
		c = shiftkeyboard[keybyte][keybit];
		/* VT switcher */
#if 0		
		if (c == KEY_F1 || c == KEY_F2 || c == KEY_F3 || c == KEY_F4) {
			if (inputtty != c - KEY_F1) {
				inputtty = c - KEY_F1;
				vtexchange();	/* Exchange the video and backing buffer */
			}
			return;
		}
#endif			
	} else
		c = keyboard[keybyte][keybit];

	if (keymap[6] & 2) {	/* control */
		if (c > 31 && c < 127)
			c &= 31;
	}

	if (capslock && c >= 'a' && c <= 'z')
		c -= 'a' - 'A';

	/* TODO: function keys (F1-F10), graph, code */

	vt_inproc(/*inputtty +*/1, c);
}


void update_keyboard(void)
{
	int n;
	uint8_t r,t;
	/*
	IO_PEEK_ENABLE;
    t= *kbd_read;
	// encode keyboard row in bits 0-3 pia1 port a, then read status from pia1 port b
	for (n =0; n < 11; n++) {
		r = (t & 0xf0) | n;
		*kbd_strobe = r;
		keyin[n] = ~*kbd_read;
	}
	IO_PEEK_DISABLE;
	*/
}

void tty_poll(void)
{
        uint8_t x;

	keyproc();

	if (keysdown && keysdown < 3) {
		if (newkey) {
			keydecode();
			kbd_timer = keyrepeat.first * ticks_per_dsecond;
		} else if (! --kbd_timer) {
			keydecode();
			kbd_timer = keyrepeat.continual * ticks_per_dsecond;
		}
	}	
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


