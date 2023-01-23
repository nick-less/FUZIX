#include <kernel.h>
#include <timer.h>
#include <kdata.h>
#include <printf.h>
#include <device.h>
#include <devtty.h>
#include <sd.h>

uint8_t membanks;
uint16_t swap_dev;

void plt_idle(void)
{
    irqflags_t irq = di();
    poll_keyboard();
    irqrestore(irq);
}

uint8_t plt_param(char *p)
{
    return 0;
}

void do_beep(void)
{
}

void plt_discard(void)
{
}

uint8_t vtattr_cap;

/* SD glue */


uint_fast8_t sd_type = 0;

void sd_spi_transmit_byte(uint8_t b)
{
//    kprintf(">%2x", b);
    switch(sd_type) {
    case SDIF_SDDRIVE:
        sddrive_transmit_byte(b);
        break;
    case SDIF_SDMOTO:
        sdmoto_transmit_byte(b);
        break;
    case SDIF_SDMO:
        sdmo_transmit_byte(b);
        break;
    }
}

uint8_t sd_spi_receive_byte(void)
{
    uint8_t r;
    switch(sd_type) {
    case SDIF_SDDRIVE:
        r = sddrive_receive_byte();
        break;
    case SDIF_SDMOTO:
        r = sdmoto_receive_byte();
        break;
    case SDIF_SDMO:
        r = sdmo_receive_byte();
        break;
    }
//    kprintf("<%2x", r);
    return r;
}

void sd_spi_transmit_sector(uint8_t *ptr)
{
    switch(sd_type) {
    case SDIF_SDDRIVE:
        sddrive_transmit_sector(ptr);
        break;
    case SDIF_SDMOTO:
        sdmoto_transmit_sector(ptr);
        break;
    case SDIF_SDMO:
        sdmo_transmit_sector(ptr);
        break;
    }
}

void sd_spi_receive_sector(uint8_t *ptr)
{
    uint16_t n;
    switch(sd_type) {
    case SDIF_SDDRIVE:
        sddrive_receive_sector(ptr);
        break;
    case SDIF_SDMOTO:
        sdmoto_receive_sector(ptr);
        break;
    case SDIF_SDMO:
        sdmo_receive_sector(ptr);
        break;
    }
#if 0
    for (n = 0; n < 512; n++) {
        kprintf("%2x ", *ptr++);
        if ((n & 7) == 7)
            kprintf("\n");
    }
#endif
}

void sd_spi_fast(void)
{
}

void sd_spi_slow(void)
{
}

void sd_spi_raise_cs(void)
{
}

void sd_spi_lower_cs(void)
{
}
