#include <kernel.h>
#include <tinydisk.h>
#include <tinysd.h>

static const volatile uint8_t *iobase = (uint8_t *)IOBASE;

/*
 * Map handling: allocate 3 banks per process for now
 */

void pagemap_init(void)
{
    uint8_t i = 0x18;
    while (i < 0x80) {
        pagemap_add(i);
        i += 0x10;
    }
    if (iobase[0x3F] & 3)
        panic("bad CONFIG");
}

void map_init(void)
{
}

uint8_t plt_param(char *p)
{
    return 0;
}


/*
 *	Do the device initialization
 */

void device_init(void)
{
    uint8_t t = sd_init();
    if (t == 0)
        return;
    if (t & CT_BLOCK)
        sd_shift[0] = 0;
    else
        sd_shift[0] = 9;
    kputs("sd0: ");
    td_register(sd_xfer, 1);
}
