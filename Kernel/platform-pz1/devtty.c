#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <stdbool.h>
#include <devtty.h>
#include <device.h>
#include <vt.h>
#include <tty.h>
#include <ports.h>

static char tbuf1[TTYSIZ];
static char tbuf2[TTYSIZ];
PTY_BUFFERS;

struct s_queue ttyinq[NUM_DEV_TTY + 1] = {	/* ttyinq[0] is never used */
	{ NULL, NULL, NULL, 0, 0, 0 },
	{ tbuf1, tbuf1, tbuf1, TTYSIZ, 0, TTYSIZ / 2 },
	{ tbuf2, tbuf2, tbuf2, TTYSIZ, 0, TTYSIZ / 2 },
	PTY_QUEUES
};

/* Currently there is no firmware interface for serial port config */
tcflag_t termios_mask[NUM_DEV_TTY + 1] = {
	0,
	_CSYS,
	_CSYS
};

/* Output for the system console (kprintf etc) */
void kputchar(uint8_t c)
{
	if (c == '\n')
		tty_putc(1, '\r');
	tty_putc(1, c);
}

ttyready_t tty_writeready(uint8_t minor)
{
	if (minor == 1) {
		if ((port_serial0_flags & SERIAL_FLAGS_OUT_FULL) == 0)
			return TTY_READY_NOW;
		else
			return TTY_READY_SOON;
	} else {
		if ((port_serial0_flags & SERIAL_FLAGS_OUT_FULL) == 0)
			return TTY_READY_NOW;
		else
			return TTY_READY_SOON;
	}
}

void tty_putc(uint8_t minor, unsigned char c)
{
	if (minor == 1) {
		while ((port_serial0_flags & SERIAL_FLAGS_OUT_FULL) != 0);
		// do nothing
		port_serial0_out = c;
	} else {
		while ((port_serial1_flags & SERIAL_FLAGS_OUT_FULL) != 0);
		// do nothing
		port_serial1_out = c;
	}


}

void tty_setup(uint8_t minor, uint8_t flag)
{
}

void tty_sleeping(uint8_t minor)
{
}

/* For the moment */
int tty_carrier(uint8_t minor)
{
	return 1;
}

void tty_data_consumed(uint8_t minor)
{
}

void tty_poll(void)
{
	while (port_serial0_flags & SERIAL_FLAGS_IN_AVAIL)
		tty_inproc(1, port_serial0_in);
	while (port_serial1_flags & SERIAL_FLAGS_IN_AVAIL)
		tty_inproc(2, port_serial1_in);
}
