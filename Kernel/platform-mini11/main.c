#include <kernel.h>
#include <timer.h>
#include <kdata.h>
#include <printf.h>
#include <tinydisk.h>
#include <devtty.h>

uint8_t kernel_flag = 1;
uint8_t need_resched;
uint16_t swap_dev = 0xFFFF;

void plt_idle(void)
{
    irqflags_t flags = di();
//    tty_poll();
    irqrestore(flags);
}

void do_beep(void)
{
}


/* Our E clock is 2MHz, the prescaler divides it by 16 so the
   timr counts at 125Khz giving us 2500 pulses per 50th */

#define CLK_PER_TICK	2500

uint16_t timer_step = CLK_PER_TICK;

/* This is incremented at 50Hz by the asm code */
uint8_t timer_ticks;

/* Each timer event we get called after the asm code has set up the timer
   again. ticks will usually be incremented by one each time, but if we
   miss a tick the overflow logic may make ticks larger */
void plt_event(void)
{
	tty_poll();

	/* Turn the 50Hz counting into a 50Hz tty poll and a 10Hz
	   timer processing */
	while(timer_ticks >= 5) {
		timer_interrupt();
		timer_ticks -= 5;
	}
}
