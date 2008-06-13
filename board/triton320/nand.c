/*
 * (C) Copyright 2007 KA-Ro electronics GmbH
 *
 * based on:
 * board/zylonite/nand.c (C) Copyright 2006 DENX Software Engineering
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#define NAND_RESET_COMMAND 0x80000000

#if (CONFIG_COMMANDS & CFG_CMD_NAND)
#ifndef CONFIG_NAND_LEGACY

#include <nand.h>
#include <asm/arch/pxa-regs.h>

#ifdef CFG_DFC_DEBUG1
# define DFC_DEBUG1(fmt, args...) printf(fmt, ##args)
#else
# define DFC_DEBUG1(fmt, args...)
#endif

#ifdef CFG_DFC_DEBUG2
# define DFC_DEBUG2(fmt, args...) printf(fmt, ##args)
#else
# define DFC_DEBUG2(fmt, args...)
#endif

#ifdef CFG_DFC_DEBUG3
# define DFC_DEBUG3(fmt, args...) printf(fmt, ##args)
#else
# define DFC_DEBUG3(fmt, args...)
#endif

#define MIN(x, y)		((x < y) ? x : y)

/* These really don't belong here, as they are specific to the NAND Model */
static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

uint8_t		partial_buffer[2048+64];	/* used to handle partial writes in case of nand erase clean */
int			partial_page;
int			partial_pointer;		

static struct nand_bbt_descr triton320_bbt_descr = {
	.options = 0,
	.offs = 0,
	.len = 2,
	.pattern = scan_ff_pattern
};

static struct nand_oobinfo triton320_oob = {
	.useecc = MTD_NANDECC_AUTOPL_USR, /* MTD_NANDECC_PLACEONLY, */
	.eccbytes = 24,
	.eccpos = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37},
	.oobfree = { {0x20, 32}, {0, 0} }
};


/*
 * not required for Monahans DFC
 */
static void dfc_hwcontrol(struct mtd_info *mtdinfo, int cmd)
{
	return;
}

#if 0
/* read device ready pin */
static int dfc_device_ready(struct mtd_info *mtdinfo)
{
	if(NDSR & NDSR_RDY)
		return 1;
	else
		return 0;
	return 0;
}
#endif

/*
 * Write buf to the DFC Controller Data Buffer
 */
static void dfc_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	unsigned long bytes_multi = len & 0xfffffffc;
	unsigned long rest = len & 0x3;
	unsigned long *long_buf;
	int i;

	if (!partial_page) {
		DFC_DEBUG2("dfc_write_buf: writing %d bytes starting with 0x%x .\n",
			   len, *((unsigned long*)buf));
		if (bytes_multi) {
			for (i = 0; i < bytes_multi; i += 4) {
				long_buf = (unsigned long*)&buf[i];
				NDDB = *long_buf;
			}
		}
	
		if (rest) {
			printf("dfc_write_buf: ERROR, writing non 4-byte aligned data.\n");
		}
		return;
	} else {
		DFC_DEBUG2("dfc_write_buf: writing %d bytes starting with 0x%x to local buffer.\n",
			   len, *((unsigned long*)buf));
		for (i = 0; i < len; i++) {
			partial_buffer[partial_pointer + i] = buf[i];
		}
	}
}


/*
 * These functions are quite problematic for the DFC. Luckily they are
 * not used in the current nand code, except for nand_command, which
 * we've defined our own anyway. The problem is, that we always need
 * to write 4 bytes to the DFC Data Buffer, but in these functions we
 * don't know if to buffer the bytes/half words until we've gathered 4
 * bytes or if to send them straight away.
 *
 * Solution: Don't use these with Mona's DFC and complain loudly.
 */
static void dfc_write_word(struct mtd_info *mtd, u16 word)
{
	printf("dfc_write_word: WARNING, this function does not work with the Monahans DFC!\n");
}
static void dfc_write_byte(struct mtd_info *mtd, u_char byte)
{
	printf("dfc_write_byte: WARNING, this function does not work with the Monahans DFC!\n");
}

/* The original:
 * static void dfc_read_buf(struct mtd_info *mtd, const u_char *buf, int len)
 *
 * Shouldn't this be "u_char * const buf" ?
 */
static void dfc_read_buf(struct mtd_info *mtd, u_char* const buf, int len)
{
	int i=0, j;

	/* we have to be carefull not to overflow the buffer if len is
	 * not a multiple of 4 */
	unsigned long bytes_multi = len & 0xfffffffc;
	unsigned long rest = len & 0x3;
	unsigned long *long_buf;

	DFC_DEBUG3("dfc_read_buf: reading %d bytes.\n", len);
	/* if there are any, first copy multiple of 4 bytes */
	if (bytes_multi) {
		for (i=0; i<bytes_multi; i+=4) {
			long_buf = (unsigned long*) &buf[i];
			*long_buf = NDDB;
		}
	}

	/* ...then the rest */
	if (rest) {
		unsigned long rest_data = NDDB;
		for (j=0;j<rest; j++)
			buf[i+j] = (u_char) ((rest_data>>j) & 0xff);
	}

	return;
}

/*
 * read a word. Not implemented as not used in NAND code.
 */
static u16 dfc_read_word(struct mtd_info *mtd)
{
	printf("dfc_write_byte: UNIMPLEMENTED.\n");
	return 0;
}

/* global var, too bad: mk@tbd: move to ->priv pointer */
static unsigned long read_buf = 0;
static int bytes_read = -1;

/*
 * read a byte from NDDB Because we can only read 4 bytes from NDDB at
 * a time, we buffer the remaining bytes. The buffer is reset when a
 * new command is sent to the chip.
 *
 * WARNING:
 * This function is currently only used to read status and id
 * bytes. For these commands always 8 bytes need to be read from
 * NDDB. So we read and discard these bytes right now. In case this
 * function is used for anything else in the future, we must check
 * what was the last command issued and read the appropriate amount of
 * bytes respectively.
 */
static u_char dfc_read_byte(struct mtd_info *mtd)
{
	unsigned char byte;
	unsigned long dummy;

	if(bytes_read < 0) {
		read_buf = NDDB;
		dummy = NDDB;
		bytes_read = 0;
	}
	byte = (unsigned char) (read_buf>>(8 * bytes_read++));
	DFC_DEBUG2("dfc_read_byte: byte %u: 0x%x of (0x%x).\n", bytes_read - 1, byte, read_buf);
	
	if(bytes_read >= 4)
		bytes_read = -1;

	return byte;
}

/* delay function, this doesn't belong here */
static void wait_us(unsigned long us)
{
	unsigned long start = OSCR;

	us = US_TO_OSCR(us);
	while ((OSCR - start) < us) {
		/* do nothing */
	}
}

static void dfc_clear_nddb(void)
{
	NDCR &= ~NDCR_ND_RUN;
	wait_us(CFG_NAND_OTHER_TO);
}

/* wait_event with timeout */
static unsigned long dfc_wait_event(unsigned long event)
{
	unsigned long ndsr, timeout, start = OSCR;

	if (!event)
		return 0xff000000;
	else if (event & (NDSR_CS0_CMDD | NDSR_CS0_BBD))
		timeout = US_TO_OSCR(CFG_NAND_PROG_ERASE_TO);
	else
		timeout = US_TO_OSCR(CFG_NAND_OTHER_TO);

	while(1) {
		ndsr = NDSR;
		if (ndsr & event) {
			NDSR |= event;
			break;
		}
		if ((OSCR - start) > timeout) {
			DFC_DEBUG1("dfc_wait_event: TIMEOUT waiting for event: 0x%x      timeout: %d ticks   status register: 0x%08X  control register: 0x%08X.\n", event, timeout, ndsr, NDCR);
			return 0xff000000;
		}

	}
	return ndsr;
}

/* we don't always wan't to do this */
static void dfc_new_cmd(void)
{
	int retry = 0;
	unsigned long status;

	while(retry++ <= CFG_NAND_SENDCMD_RETRY) {
		/* Clear NDSR */
		NDSR = 0xFFF;

		/* set NDCR[NDRUN] */
		if(!(NDCR & NDCR_ND_RUN))
			NDCR |= NDCR_ND_RUN;

		status = dfc_wait_event(NDSR_WRCMDREQ);

		if(status & NDSR_WRCMDREQ)
			return;

		DFC_DEBUG2("dfc_new_cmd: FAILED to get WRITECMDREQ, retry: %d.\n", retry);
		dfc_clear_nddb();
	}
	DFC_DEBUG1("dfc_new_cmd: giving up after %d retries.\n", retry);
}

/* this function is called after Programm and Erase Operations to
 * check for success or failure */
static int dfc_wait(struct mtd_info *mtd, struct nand_chip *this, int state)
{
	unsigned long ndsr=0, event=0;

	if(state == FL_WRITING) {
		event = NDSR_CS0_CMDD | NDSR_CS0_BBD;
	} else if(state == FL_ERASING) {
		event = NDSR_CS0_CMDD | NDSR_CS0_BBD;
	}

	ndsr = dfc_wait_event(event);

	if((ndsr & NDSR_CS0_BBD) || (ndsr & 0xff000000))
		return(0x1); /* Status Read error */
	return 0;
}

/* cmdfunc send commands to the DFC */
static void dfc_cmdfunc(struct mtd_info *mtd, unsigned command,
			int column, int page_addr)
{
	/* register struct nand_chip *this = mtd->priv; */
	unsigned long ndcb0=0, ndcb1=0, ndcb2=0, event=0;

	/* clear the ugly byte read buffer */
	bytes_read = -1;
	read_buf = 0;

	switch (command) {
	case NAND_CMD_READ0:
		DFC_DEBUG3("dfc_cmdfunc: NAND_CMD_READ0, page_addr: 0x%x, column: 0x%x.\n", page_addr, column);
		dfc_new_cmd();
		ndcb0 = (NAND_CMD_READ0  | (NAND_CMD_READSTART<<8) | (4<<16) | (1<<19));
		ndcb1 = (column & 0xfff) |  ((page_addr & 0xffff)<<16);
		ndcb2 = 0;
		event = NDSR_RDDREQ;
		goto write_cmd;
	case NAND_CMD_READ1:
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_READ1 unimplemented!\n");
		goto end;
	case NAND_CMD_READOOB:
		DFC_DEBUG1("dfc_cmdfunc: NAND_CMD_READOOB unimplemented!\n");
		goto end;
	case NAND_CMD_READID:
		dfc_new_cmd();
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_READID.\n");
		ndcb0 = (NAND_CMD_READID | (3 << 21) | (1 << 16)); /* addr cycles*/
		ndcb1 = 0;
		ndcb2 = 0;
		event = NDSR_RDDREQ;
		goto write_cmd;
	case NAND_CMD_PAGEPROG:
		/* sent as a multicommand in NAND_CMD_SEQIN */
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_PAGEPROG empty due to multicmd.\n");
		if(partial_page) {
			partial_page = 0;
			dfc_write_buf(mtd, partial_buffer, 2048 + 64);	/* write back the partial buffer */
		}
		goto end;
	case NAND_CMD_ERASE1:
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_ERASE1,  page_addr: 0x%x, column: 0x%x.\n", page_addr, column);
		dfc_new_cmd();
		ndcb0 = (0xd060 | (1<<25) | (2<<21) | (1<<19) | (2<<16));
		ndcb1 = (page_addr & 0x0000ffff);
		ndcb2 = 0;
		goto write_cmd;
	case NAND_CMD_SEQIN:
		/* send PAGE_PROG command(0x1080) */
		if(column) {
			partial_page = 1;
			partial_pointer = column;
			/* read whole page into local buffer first */
			DFC_DEBUG3("dfc_cmdfunc: NAND_CMD_READ0, page_addr: 0x%x, column: 0x%x.\n", page_addr, column);
			dfc_new_cmd();
			NDCB0 = (NAND_CMD_READ0  | (NAND_CMD_READSTART<<8) | (4<<16) | (1<<19));
			NDCB0 = (page_addr & 0xffff) << 16;
			NDCB0 = 0;
			event = NDSR_RDDREQ;
			dfc_wait_event(event);
			dfc_read_buf(mtd, partial_buffer, 2048 + 64); /* this works in that way only for large page */
		} else {
			partial_page = 0;
		}

		dfc_new_cmd();
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_SEQIN/PAGE_PROG,  page_addr: 0x%x, column: 0x%x.\n", page_addr, column);
		ndcb0 = (0x1080 | (1<<25) | (1<<21) | (1<<19) | (4<<16));
		/* column address must be 0 since we cannot write partial pages */
		ndcb1 = (page_addr & 0xffff)<<16;
		ndcb2 = 0;
		event = NDSR_WRDREQ;
		goto write_cmd;
	case NAND_CMD_ERASE2:
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_ERASE2 empty due to multicmd.\n");
		goto end;
	case NAND_CMD_STATUS:
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_STATUS.\n");
		dfc_new_cmd();
		ndcb0 = NAND_CMD_STATUS | (4<<21);
		ndcb1 = 0;
		ndcb2 = 0;
		event = NDSR_RDDREQ;
		goto write_cmd;
	case NAND_CMD_RESET:
		DFC_DEBUG2("dfc_cmdfunc: NAND_CMD_RESET.\n");
		return;		/* DFC or NAND hangs after Reset, but we do not need the command */
		ndcb0 = NAND_CMD_RESET | (5<<21);
		ndcb1 = 0;
		ndcb2 = 0;
		event = NDSR_CS0_CMDD;
		goto write_cmd;
	default:
		printk("dfc_cmdfunc: error, unsupported command.\n");
		goto end;
	}

 write_cmd:
 	DFC_DEBUG3("dfc_cmdfunc: writing NDCB0 = 0x%08X         NDCB1 = 0x%08X               NDCB2 = 0x%08X\n", ndcb0, ndcb1, ndcb2); 
	NDCB0 = ndcb0;
	NDCB0 = ndcb1;
	NDCB0 = ndcb2;

	/*  wait_event: */
	dfc_wait_event(event);
 end:
	return;
}

/*
 * Board-specific NAND initialization. The following members of the
 * argument are board-specific (per include/linux/mtd/nand_new.h):
 * - IO_ADDR_R?: address to read the 8 I/O lines of the flash device
 * - IO_ADDR_W?: address to write the 8 I/O lines of the flash device
 * - hwcontrol: hardwarespecific function for accessing control-lines
 * - dev_ready: hardwarespecific function for  accessing device ready/busy line
 * - enable_hwecc?: function to enable (reset)  hardware ecc generator. Must
 *   only be provided if a hardware ECC is available
 * - eccmode: mode of ecc, see defines
 * - chip_delay: chip dependent delay for transfering data from array to
 *   read regs (tR)
 * - options: various chip options. They can partly be set to inform
 *   nand_scan about special functionality. See the defines for further
 *   explanation
 * Members with a "?" were not set in the merged testing-NAND branch,
 * so they are not set here either.
 */
void board_nand_init(struct nand_chip *nand)
{
	unsigned long value;

	value = NDCR & ~(NDCR_ECC_EN); /* clear hardware ECC */
	NDCR = value;

	nand->hwcontrol = dfc_hwcontrol;
/*	nand->dev_ready = dfc_device_ready; */
	nand->eccmode = NAND_ECC_SOFT;
	nand->options = NAND_SAMSUNG_LP_OPTIONS;
	nand->waitfunc = dfc_wait;
	nand->read_byte = dfc_read_byte;
	nand->write_byte = dfc_write_byte;
	nand->read_word = dfc_read_word;
	nand->write_word = dfc_write_word;
	nand->read_buf = dfc_read_buf;
	nand->write_buf = dfc_write_buf;

	nand->cmdfunc = dfc_cmdfunc;
	nand->autooob = &triton320_oob;
	nand->badblock_pattern = &triton320_bbt_descr;
}

#else
 #error "U-Boot legacy NAND support not available for Monahans DFC."
#endif
#endif
