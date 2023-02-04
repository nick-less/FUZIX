/*-----------------------------------------------------------------------*/
/* Fuzix SD card driver                                                  */
/* 2014-12-28 Will Sowerbutts                                            */
/* 2015-01-04 WRS updated to new blkdev API                              */
/* 2015-01-25 WRS updated to newer blkdev API                            */
/*                                                                       */
/* Based on UZI-socz80 SD card driver, which was itself based on:        */
/*   MMCv3/SDv1/SDv2 (in SPI mode) control module  (C)ChaN, 2007         */
/*  (from http://www.cl.cam.ac.uk/teaching/1011/P31/lib/diskio.c)        */
/* and http://elm-chan.org/docs/mmc/mmc_e.html                           */
/*-----------------------------------------------------------------------*/

#define _SD_PRIVATE

#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <timer.h>
#include <devsd.h>
#include <stdbool.h>
#include <blkdev.h>

#ifdef CONFIG_SD

/* for platforms with multiple SD card slots, this variable contains
 * the current operation's drive number */
uint_fast8_t sd_drive = SD_DRIVE_NONE;

/* A write is being executed in the background; when set, we have to wait for
 * the card to finish before issuing the next command. We won't unassert CS
 * until this is done. */
static bool deferredwrite;

uint_fast8_t devsd_transfer_sector(void)
{
    uint8_t attempt;
    bool success;

    sd_drive = blk_op.blkdev->driver_data & DRIVE_NR_MASK;

    /* Ensure that sd_drive is visible to any IRQ handler checking for SD_DRIVE_NONE before
       we touch the bus */
    barrier();

    for(attempt=0; attempt<8; attempt++){
	if(sd_send_command(blk_op.is_read ? CMD17 : CMD24, 
                    /* for byte addressed cards, shift LBA to convert to byte address */
                    (blk_op.blkdev->driver_data & CT_BLOCK) ? blk_op.lba : (blk_op.lba << 9)
                    ) == 0){
	    if(blk_op.is_read){
                success = (sd_spi_wait(false) == 0xFE);
                if(success)
                    sd_spi_receive_sector();
            }else{
                success = false;
                if(sd_spi_wait(true) == 0xFF){
                    sd_spi_transmit_byte(0xFE);
                    sd_spi_transmit_sector();
                    sd_spi_transmit_byte(0xFF); /* dummy CRC */
                    sd_spi_transmit_byte(0xFF);
                    /* Was the data accepted ? */
                    success = ((sd_spi_wait(false) & 0x1F) == 0x05);
                }
            }
	} else
	    success = false;

	if (blk_op.is_read) {
		sd_spi_release();
		deferredwrite = 0;
	} else {
		/* Leave CS asserted so the write can complete */
		deferredwrite = 1;
	}
	
	if(success) {
            sd_drive = SD_DRIVE_NONE;
	    return 1;
        }

	kputs("sd: failed, retrying.\n");
    }

    udata.u_error = EIO;
    sd_drive = SD_DRIVE_NONE;
    return 0;
}

void sd_spi_release(void)
{
	if (deferredwrite) {
		for (;;) {
			uint_fast8_t b = sd_spi_receive_byte();
			if (b != 0x00)
				break;
		}
		deferredwrite = 0;
	}
	sd_spi_raise_cs();
	sd_spi_receive_byte();
}

#ifdef CONFIG_SPI_SHARED

/*
 *	On a system where the SPI bus is shared with another device this routine allows an IRQ level
 *	caller to check if the SD card is still sitting on the bus, even for a writeback and if so
 *	to prod it to see if it can have the bus back.
 *
 *	It can also be used in the timer loops of platforms where it is desirable to spot completion
 *	and raise CS sooner (e.g. to provide the expected LED indication to the user).
 */
uint_fast8_t sd_spi_try_release(void)
{
    uint8_t b;
    /* An action transaction is occurring - busy */
    if (sd_drive != SD_DRIVE_NONE)
        return 1;
    /* There is no transaction and no delayed write - free */
    if (deferredwrite == 0)
        return 0;
    /* Fetch a byte and see if the card is done */
    b = sd_spi_receive_byte();
    /* 0 indicates it is not complete - busy */
    if (b == 0x00)
        return 1;
    /* The card finished, free the bus */
    deferredwrite = 0;
    sd_spi_raise_cs();
    sd_spi_receive_byte();
    /* bus is free */
    return 0;
}
#endif

uint_fast8_t sd_spi_wait(bool want_ff)
{
    unsigned int timer;
    uint_fast8_t b;

    timer = set_timer_ms(500);

    while(true){
        b = sd_spi_receive_byte();
        if(want_ff){
            if(b == 0xFF)
                break;
        }else{
            if(b != 0xFF)
                break;
        }
        if(timer_expired(timer)){
            kputs("sd: timeout\n");
            break;
        }

    }

    return b;
}

int sd_send_command(uint_fast8_t cmd, uint32_t arg)
{
    uint_fast8_t n, res;

    /* Ensure that any deferred writes have finished. */
    sd_spi_release();

    if (cmd & 0x80) {   /* ACMD<n> is the command sequense of CMD55-CMD<n> */
        cmd &= 0x7F;
        res = sd_send_command(CMD55, 0);
        if (res > 1) 
            return res;
    }

    /* Select the card and wait for ready */
    sd_spi_release(); /* raise CS, then sends 8 clocks (some cards require this) */
    sd_spi_lower_cs();
    if(cmd != CMD0 && sd_spi_wait(true) != 0xFF)
        return 0xFF;

    /* Send command packet */
    sd_spi_transmit_byte(cmd);                        /* Start + Command index */
#if !defined(__SDCC) && !defined(SDCC)
    sd_spi_transmit_byte((unsigned char)(arg >> 24)); /* Argument[31..24] */
    sd_spi_transmit_byte((unsigned char)(arg >> 16)); /* Argument[23..16] */
    sd_spi_transmit_byte((unsigned char)(arg >> 8));  /* Argument[15..8] */
    sd_spi_transmit_byte((unsigned char)arg);         /* Argument[7..0] */
#else
    {
        unsigned char *p;
        /* sdcc sadly unable to figure this out for itself yet */
        p = ((unsigned char *)&arg)+3;
        sd_spi_transmit_byte(*(p--));                     /* Argument[31..24] */
        sd_spi_transmit_byte(*(p--));                     /* Argument[23..16] */
        sd_spi_transmit_byte(*(p--));                     /* Argument[15..8] */
        sd_spi_transmit_byte(*p);                         /* Argument[7..0] */
    }
#endif
    /* there's only a few commands (in native mode) that need correct CRCs */
    n = 0x01;                                                /* Dummy CRC + Stop */
    if (cmd == CMD0) n = 0x95;                               /* Valid CRC for CMD0(0) */
    if (cmd == CMD8) n = 0x87;                               /* Valid CRC for CMD8(0x1AA) */
    sd_spi_transmit_byte(n);

    /* Receive command response */
/*    if (cmd == CMD12)  - ignore first reply byte anyway because it may
      be floating bus */
    sd_spi_receive_byte();     /* Skip a stuff byte when stop reading */
    n = 20;                             /* Wait for a valid response */
    do {
        res = sd_spi_receive_byte();
    } while ((res & 0x80) && --n);

    return res;         /* Return with the response value */
}

#endif
