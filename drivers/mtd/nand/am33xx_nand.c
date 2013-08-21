/*
 * (C) Copyright 2012 Lothar Waßmann <LW@KARO-electronics.de>
 * based on ti81xx_nand.c
 * (C) Copyright 2004-2008 Texas Instruments, <www.ti.com>
 * Mansoor Ahamed <mansoor.ahamed@ti.com>
 *
 * Derived from work done by Rohit Choraria <rohitkc@ti.com> for omap
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
#include <asm/io.h>
#include <asm/errno.h>
#include <asm/arch/cpu.h>
#include <asm/arch/mem.h>
#include <asm/arch/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <nand.h>

struct nand_bch_priv {
	uint8_t type;
	uint8_t nibbles;
};

/* bch types */
#define ECC_BCH4	0
#define ECC_BCH8	1
#define ECC_BCH16	2

/* BCH nibbles for diff bch levels */
#define ECC_BCH4_NIBBLES	13
#define ECC_BCH8_NIBBLES	26
#define ECC_BCH16_NIBBLES	52

static uint8_t cs;
#ifndef CONFIG_SPL_BUILD
static struct nand_ecclayout hw_nand_oob = GPMC_NAND_HW_ECC_LAYOUT_KERNEL;
static struct nand_ecclayout hw_bch4_nand_oob = GPMC_NAND_HW_BCH4_ECC_LAYOUT;
static struct nand_ecclayout hw_bch16_nand_oob = GPMC_NAND_HW_BCH16_ECC_LAYOUT;
#endif
static struct nand_ecclayout hw_bch8_nand_oob = GPMC_NAND_HW_BCH8_ECC_LAYOUT;

static struct gpmc *gpmc_cfg = (struct gpmc *)GPMC_BASE;

static struct nand_bch_priv bch_priv = {
	.type = ECC_BCH8,
	.nibbles = ECC_BCH8_NIBBLES,
};

#ifndef CONFIG_SYS_NAND_NO_OOB
static uint8_t bbt_pattern[] = {'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = {'1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	58,
	.len = 4,
	.veroffs = 62,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE |
		NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs =	58,
	.len = 4,
	.veroffs = 62,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};
#endif

/*
 * am33xx_read_bch8_result - Read BCH result for BCH8 level
 *
 * @mtd:	MTD device structure
 * @big_endian:	When set read register 3 first
 * @ecc_code:	Read syndrome from BCH result registers
 */
static void am33xx_read_bch8_result(struct mtd_info *mtd, int big_endian,
				uint8_t *ecc_code)
{
	uint32_t *ptr;
	int i = 0, j;

	if (big_endian) {
		u32 res;
		ptr = &gpmc_cfg->bch_result_0_3[0].bch_result_x[3];
		res = readl(ptr);
		ecc_code[i++] = res & 0xFF;
		for (j = 0; j < 3; j++) {
			u32 res = readl(--ptr);

			ecc_code[i++] = (res >> 24) & 0xFF;
			ecc_code[i++] = (res >> 16) & 0xFF;
			ecc_code[i++] = (res >>  8) & 0xFF;
			ecc_code[i++] = res & 0xFF;
		}
	} else {
		ptr = &gpmc_cfg->bch_result_0_3[0].bch_result_x[0];
		for (j = 0; j < 3; j++) {
			u32 res = readl(ptr++);

			ecc_code[i++] = res & 0xFF;
			ecc_code[i++] = (res >>  8) & 0xFF;
			ecc_code[i++] = (res >> 16) & 0xFF;
			ecc_code[i++] = (res >> 24) & 0xFF;
		}
		ecc_code[i++] = readl(ptr) & 0xFF;
	}
	ecc_code[i] = 0xff;
}

/*
 * am33xx_ecc_disable - Disable H/W ECC calculation
 *
 * @mtd:	MTD device structure
 *
 */
static void am33xx_ecc_disable(struct mtd_info *mtd)
{
	writel((readl(&gpmc_cfg->ecc_config) & ~0x1),
		&gpmc_cfg->ecc_config);
}

/*
 * am33xx_nand_hwcontrol - Set the address pointers correctly for the
 *			following address/data/command operation
 */
static void am33xx_nand_hwcontrol(struct mtd_info *mtd, int32_t cmd,
				uint32_t ctrl)
{
	register struct nand_chip *this = mtd->priv;

	debug("nand cmd %08x ctrl %08x\n", cmd, ctrl);
	/*
	 * Point the IO_ADDR to DATA and ADDRESS registers instead
	 * of chip address
	 */
	switch (ctrl) {
	case NAND_CTRL_CHANGE | NAND_CTRL_CLE:
		this->IO_ADDR_W = (void __iomem *)&gpmc_cfg->cs[cs].nand_cmd;
		break;
	case NAND_CTRL_CHANGE | NAND_CTRL_ALE:
		this->IO_ADDR_W = (void __iomem *)&gpmc_cfg->cs[cs].nand_adr;
		break;
	case NAND_CTRL_CHANGE | NAND_NCE:
		this->IO_ADDR_W = (void __iomem *)&gpmc_cfg->cs[cs].nand_dat;
	}

	if (cmd != NAND_CMD_NONE)
		writeb(cmd, this->IO_ADDR_W);
}

/*
 * am33xx_hwecc_init_bch - Initialize the BCH Hardware ECC for NAND flash in
 *				GPMC controller
 * @mtd:       MTD device structure
 * @mode:	Read/Write mode
 */
static void am33xx_hwecc_init_bch(struct nand_chip *chip, int32_t mode)
{
	uint32_t val, dev_width = (chip->options & NAND_BUSWIDTH_16) >> 1;
	uint32_t unused_length = 0;
	struct nand_bch_priv *bch = chip->priv;

	switch (bch->nibbles) {
		case ECC_BCH4_NIBBLES:
			unused_length = 3;
			break;
		case ECC_BCH8_NIBBLES:
			unused_length = 2;
			break;
		case ECC_BCH16_NIBBLES:
			unused_length = 0;
	}

	/* Clear the ecc result registers, select ecc reg as 1 */
	writel(ECCCLEAR | ECCRESULTREG1, &gpmc_cfg->ecc_control);

	switch (mode) {
		case NAND_ECC_WRITE:
			/* eccsize1 config */
			val = ((unused_length + bch->nibbles) << 22);
			break;

		case NAND_ECC_READ:
		default:
			/* by default eccsize0 selected for ecc1resultsize */
			/* eccsize0 config */
			val  = (bch->nibbles << 12);
			/* eccsize1 config */
			val |= (unused_length << 22);
	}
	/* ecc size configuration */
	writel(val, &gpmc_cfg->ecc_size_config);
	/* by default 512bytes sector page is selected */
	/* set bch mode */
	val  = (1 << 16);
	/* bch4 / bch8 / bch16 */
	val |= (bch->type << 12);
	/* set wrap mode to 1 */
	val |= (1 << 8);
	val |= (dev_width << 7);
	val |= (cs << 1);
	/* enable ecc */
	/* val |= (1); */ /* should not enable ECC just init i.e. config */
	writel(val, &gpmc_cfg->ecc_config);
}

#ifndef CONFIG_SPL_BUILD
/*
 * am33xx_hwecc_init - Initialize the Hardware ECC for NAND flash in
 *                   GPMC controller
 * @mtd:        MTD device structure
 *
 */
static void am33xx_hwecc_init(struct nand_chip *chip)
{
	/*
	 * Init ECC Control Register
	 * Clear all ECC | Enable Reg1
	 */
	writel(ECCCLEAR | ECCRESULTREG1, &gpmc_cfg->ecc_control);
	writel(ECCSIZE1 | ECCSIZE0 | ECCSIZE0SEL, &gpmc_cfg->ecc_size_config);
}

/*
 * gen_true_ecc - This function will generate true ECC value, which
 * can be used when correcting data read from NAND flash memory core
 *
 * @ecc_buf:	buffer to store ecc code
 *
 * @return:	re-formatted ECC value
 */
static uint32_t gen_true_ecc(uint8_t *ecc_buf)
{
	return ecc_buf[0] | (ecc_buf[1] << 16) | ((ecc_buf[2] & 0xF0) << 20) |
		((ecc_buf[2] & 0x0F) << 8);
}
#endif

/*
 * am33xx_rotate_ecc_bch - Rotate the syndrome bytes
 *
 * @mtd:	MTD device structure
 * @calc_ecc:	ECC read from ECC registers
 * @syndrome:	Rotated syndrome will be retuned in this array
 *
 */
static inline void am33xx_rotate_ecc_bch(struct mtd_info *mtd, uint8_t *calc_ecc,
		uint8_t *syndrome)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_bch_priv *bch = chip->priv;
	int n_bytes;
	int i, j;

	switch (bch->type) {
		case ECC_BCH4:
			n_bytes = 8;
			break;

		case ECC_BCH16:
			n_bytes = 28;
			break;

		case ECC_BCH8:
		default:
			n_bytes = 13;
	}

	for (i = 0, j = (n_bytes - 1); i < n_bytes; i++, j--)
		syndrome[i] =  calc_ecc[j];
	syndrome[i] = 0xff;
}


/*
 * am33xx_fix_errors_bch - Correct bch error in the data
 *
 * @mtd:	MTD device structure
 * @data:	Data read from flash
 * @error_count:Number of errors in data
 * @error_loc:	Locations of errors in the data
 *
 */
static void am33xx_fix_errors_bch(struct mtd_info *mtd, uint8_t *data,
		uint32_t error_count, uint32_t *error_loc)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_bch_priv *bch = chip->priv;
	int count = 0;
	uint32_t error_byte_pos;
	uint32_t error_bit_mask;
	uint32_t last_bit = (bch->nibbles * 4) - 1;

	/* Flip all bits as specified by the error location array. */
	/* FOR( each found error location flip the bit ) */
	for (count = 0; count < error_count; count++) {
		if (error_loc[count] > last_bit) {
			/* Remove the ECC spare bits from correction. */
			error_loc[count] -= (last_bit + 1);
			/* Offset bit in data region */
			error_byte_pos = ((512 * 8) - (error_loc[count]) - 1) / 8;
			/* Error Bit mask */
			error_bit_mask = 0x1 << (error_loc[count] % 8);
			/* Toggle the error bit to make the correction. */
			data[error_byte_pos] ^= error_bit_mask;
		}
	}
}

/*
 * am33xx_correct_data_bch - Compares the ecc read from nand spare area
 * with ECC registers values and corrects one bit error if it has occured
 *
 * @mtd:	MTD device structure
 * @dat:	page data
 * @read_ecc:	ecc read from nand flash (ignored)
 * @calc_ecc:	ecc read from ECC registers
 *
 * @return 0 if data is OK or corrected, else returns -1
 */
static int am33xx_correct_data_bch(struct mtd_info *mtd, uint8_t *dat,
				uint8_t *read_ecc, uint8_t *calc_ecc)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_bch_priv *bch = chip->priv;
	uint8_t syndrome[28];
	uint32_t error_count = 0;
	uint32_t error_loc[8];
	uint32_t i, ecc_flag;

	ecc_flag = 0;
	for (i = 0; i < (chip->ecc.bytes - 1); i++)
		if (read_ecc[i] != 0xff)
			ecc_flag = 1;

	if (!ecc_flag)
		return 0;

	elm_reset();
	elm_config(bch->type);

	/* while reading ECC result we read it in big endian.
	 * Hence while loading to ELM we have rotate to get the right endian.
	 */
	am33xx_rotate_ecc_bch(mtd, calc_ecc, syndrome);

	/* use elm module to check for errors */
	if (elm_check_error(syndrome, bch->nibbles, &error_count, error_loc) != 0) {
		printf("uncorrectable ECC error\n");
		return -1;
	}

	/* correct bch error */
	if (error_count > 0) {
		am33xx_fix_errors_bch(mtd, dat, error_count, error_loc);
	}

	return 0;
}

#ifndef CONFIG_SPL_BUILD
/*
 * am33xx_correct_data - Compares the ecc read from nand spare area with ECC
 * registers values and corrects one bit error if it has occured
 * Further details can be had from Am33xx TRM and the following selected links:
 * http://en.wikipedia.org/wiki/Hamming_code
 * http://www.cs.utexas.edu/users/plaxton/c/337/05f/slides/ErrorCorrection-4.pdf
 *
 * @mtd:		 MTD device structure
 * @dat:		 page data
 * @read_ecc:		 ecc read from nand flash
 * @calc_ecc:		 ecc read from ECC registers
 *
 * @return 0 if data is OK or corrected, else returns -1
 */
static int am33xx_correct_data(struct mtd_info *mtd, uint8_t *dat,
				uint8_t *read_ecc, uint8_t *calc_ecc)
{
	uint32_t orig_ecc, new_ecc, res, hm;
	uint16_t parity_bits, byte;
	int bit;

	/* Regenerate the orginal ECC */
	orig_ecc = gen_true_ecc(read_ecc);
	new_ecc = gen_true_ecc(calc_ecc);
	/* Get the XOR of real ecc */
	res = orig_ecc ^ new_ecc;
	if (res) {
		/* Get the hamming width */
		hm = hweight32(res);
		/* Single bit errors can be corrected! */
		if (hm == 12) {
			/* Correctable data! */
			parity_bits = res >> 16;
			bit = (parity_bits & 0x7);
			byte = (parity_bits >> 3) & 0x1FF;
			/* Flip the bit to correct */
			dat[byte] ^= (0x1 << bit);
		} else if (hm == 1) {
			printf("am33xx_nand: Error: Corrupted ECC\n");
			/* ECC itself is corrupted */
			return 2;
		} else {
			/*
			 * hm distance != parity pairs OR one, could mean 2 bit
			 * error OR potentially be on a blank page..
			 * orig_ecc: contains spare area data from nand flash.
			 * new_ecc: generated ecc while reading data area.
			 * Note: if the ecc = 0, all data bits from which it was
			 * generated are 0xFF.
			 * The 3 byte(24 bits) ecc is generated per 512byte
			 * chunk of a page. If orig_ecc(from spare area)
			 * is 0xFF && new_ecc(computed now from data area)=0x0,
			 * this means that data area is 0xFF and spare area is
			 * 0xFF. A sure sign of an erased page!
			 */
			if ((orig_ecc == 0x0FFF0FFF) && (new_ecc == 0x00000000))
				return 0;
			printf("am33xx_nand: Error: Multibit error detected; hm=%d\n",
				hm);
			/* detected 2 bit error */
			return -1;
		}
	}
	return 0;
}
#endif

/*
 *  am33xx_calculate_ecc_bch - Read BCH ECC result
 *
 *  @mtd:	MTD structure
 *  @dat:	unused
 *  @ecc_code:	ecc_code buffer
 */
static int am33xx_calculate_ecc_bch(struct mtd_info *mtd, const uint8_t *dat,
				uint8_t *ecc_code)
{
	struct nand_chip *chip = mtd->priv;
	struct nand_bch_priv *bch = chip->priv;
	int big_endian = 1;
	int ret = 0;

	if (bch->type == ECC_BCH8)
		am33xx_read_bch8_result(mtd, big_endian, ecc_code);
	else /* BCH4 and BCH16 currently not supported */
		ret = -1;

	/*
	 * Stop reading anymore ECC vals and clear old results
	 * enable will be called if more reads are required
	 */
	am33xx_ecc_disable(mtd);

	return ret;
}

#ifndef CONFIG_SPL_BUILD
/*
 *  am33xx_calculate_ecc - Generate non-inverted ECC bytes.
 *
 *  Using noninverted ECC can be considered ugly since writing a blank
 *  page ie. padding will clear the ECC bytes. This is no problem as
 *  long nobody is trying to write data on the seemingly unused page.
 *  Reading an erased page will produce an ECC mismatch between
 *  generated and read ECC bytes that has to be dealt with separately.
 *  E.g. if page is 0xFF (fresh erased), and if HW ECC engine within GPMC
 *  is used, the result of read will be 0x0 while the ECC offsets of the
 *  spare area will be 0xFF which will result in an ECC mismatch.
 *  @mtd:	MTD structure
 *  @dat:	unused
 *  @ecc_code:	ecc_code buffer
 */
static int am33xx_calculate_ecc(struct mtd_info *mtd, const uint8_t *dat,
				uint8_t *ecc_code)
{
	u_int32_t val;

	/* Start Reading from HW ECC1_Result = 0x200 */
	val = readl(&gpmc_cfg->ecc1_result);

	ecc_code[0] = val & 0xFF;
	ecc_code[1] = (val >> 16) & 0xFF;
	ecc_code[2] = ((val >> 8) & 0x0F) | ((val >> 20) & 0xF0);

	/*
	 * Stop reading anymore ECC vals and clear old results
	 * enable will be called if more reads are required
	 */
	writel(0x000, &gpmc_cfg->ecc_config);

	return 0;
}
#endif

#ifdef CONFIG_SPL_BUILD
static void am33xx_spl_nand_command(struct mtd_info *mtd, unsigned int cmd,
				int col, int page)
{
	struct nand_chip *chip = mtd->priv;

	while (!chip->dev_ready(mtd))
		;

	/* Emulate NAND_CMD_READOOB */
	if (cmd == NAND_CMD_READOOB) {
		col += CONFIG_SYS_NAND_PAGE_SIZE;
		cmd = NAND_CMD_READ0;
	}

	/* Shift the offset from byte addressing to word addressing. */
	if (chip->options & NAND_BUSWIDTH_16)
		col >>= 1;

	/* Begin command latch cycle */
	chip->cmd_ctrl(mtd, cmd, NAND_CTRL_CLE | NAND_CTRL_CHANGE);
	/* Set ALE and clear CLE to start address cycle */
	/* Column address */
	chip->cmd_ctrl(mtd, col & 0xff,
		       NAND_CTRL_ALE | NAND_CTRL_CHANGE); /* A[7:0] */
	chip->cmd_ctrl(mtd, (col >> 8) & 0xff, NAND_CTRL_ALE); /* A[11:9] */
	/* Row address */
	chip->cmd_ctrl(mtd, page & 0xff, NAND_CTRL_ALE); /* A[19:12] */
	chip->cmd_ctrl(mtd, (page >> 8) & 0xff,
		       NAND_CTRL_ALE); /* A[27:20] */
#ifdef CONFIG_SYS_NAND_5_ADDR_CYCLE
	/* One more address cycle for devices > 128MiB */
	chip->cmd_ctrl(mtd, (page >> 16) & 0x0f,
		       NAND_CTRL_ALE); /* A[31:28] */
#endif
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	/* Latch in address */
	chip->cmd_ctrl(mtd, cmd == NAND_CMD_RNDOUT ?
		NAND_CMD_RNDOUTSTART : NAND_CMD_READSTART,
		NAND_CTRL_CLE | NAND_CTRL_CHANGE);
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	/*
	 * Wait a while for the data to be ready
	 */
	while (!chip->dev_ready(mtd))
		;
}
#endif

/**
 * am33xx_read_page_bch - hardware ecc based page read function
 * @mtd:	mtd info structure
 * @chip:	nand chip info structure
 * @buf:	buffer to store read data
 * @page:	page number to read
 *
 */
static inline int am33xx_read_page_bch(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int oob_required, int page)
{
	int ret = 0;
	int i, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	uint8_t *oob = chip->oob_poi;
	uint32_t data_pos = 0;
	uint32_t oob_pos = (eccsize * eccsteps) + eccpos[0];

	chip->cmdfunc(mtd, NAND_CMD_READ0, data_pos, page);

	for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize,
				oob += eccbytes) {
		chip->ecc.hwctl(mtd, NAND_ECC_READ);
		/* read data */
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, data_pos, page);
		chip->read_buf(mtd, p, eccsize);
		/* read respective ecc from oob area */
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, oob_pos, page);
		chip->read_buf(mtd, oob, eccbytes);
		/* read syndrome */
		chip->ecc.calculate(mtd, p, &ecc_calc[i]);

		data_pos += eccsize;
		oob_pos += eccbytes;
	}

	for (i = 0; i < chip->ecc.total; i++) {
		ecc_code[i] = chip->oob_poi[i];
	}

	eccsteps = chip->ecc.steps;
	p = buf;

	for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
		int stat;

		stat = chip->ecc.correct(mtd, p, &ecc_code[i], &ecc_calc[i]);
		if (stat < 0) {
			printf("am33xx_nand: uncorrectable ECC error in page %5d\n",
				page);
			mtd->ecc_stats.failed++;
			return -EBADMSG;
		} else if (stat) {
			mtd->ecc_stats.corrected += stat;
			printf("%s: corrected ECC errors: %d\n", __func__, stat);
			ret += stat;
		}
	}
	return ret;
}

/*
 * am33xx_enable_ecc_bch- This function enables the bch h/w ecc functionality
 * @mtd:        MTD device structure
 * @mode:       Read/Write mode
 *
 */
static void am33xx_enable_ecc_bch(struct mtd_info *mtd, int32_t mode)
{
	struct nand_chip *chip = mtd->priv;

	am33xx_hwecc_init_bch(chip, mode);
	/* enable ecc */
	writel(readl(&gpmc_cfg->ecc_config) | 0x1, &gpmc_cfg->ecc_config);
}

#ifndef CONFIG_SPL_BUILD
/*
 * am33xx_enable_ecc - This function enables the hardware ecc functionality
 * @mtd:        MTD device structure
 * @mode:       Read/Write mode
 */
static void am33xx_enable_ecc(struct mtd_info *mtd, int32_t mode)
{
	struct nand_chip *chip = mtd->priv;
	uint32_t val, dev_width = (chip->options & NAND_BUSWIDTH_16) >> 1;

	switch (mode) {
	case NAND_ECC_READ:
	case NAND_ECC_WRITE:
		/* Clear the ecc result registers, select ecc reg as 1 */
		writel(ECCCLEAR | ECCRESULTREG1, &gpmc_cfg->ecc_control);

		/*
		 * Size 0 = 0xFF, Size1 is 0xFF - both are 512 bytes
		 * tell all regs to generate size0 sized regs
		 * we just have a single ECC engine for all CS
		 */
		writel(ECCSIZE1 | ECCSIZE0 | ECCSIZE0SEL,
			&gpmc_cfg->ecc_size_config);
		val = (dev_width << 7) | (cs << 1) | (1 << 0);
		writel(val, &gpmc_cfg->ecc_config);
		break;
	default:
		printf("Error: Unrecognized Mode[%d]!\n", mode);
	}
}

/*
 * __am33xx_nand_switch_ecc - switch the ECC operation ib/w h/w ecc
 * (i.e. hamming / bch) and s/w ecc.
 * The default is to come up on s/w ecc
 *
 * @nand:	NAND chip datastructure
 * @hardware:  NAND_ECC_HW -switch to h/w ecc
 *				NAND_ECC_SOFT -switch to s/w ecc
 *
 * @mode:	0 - hamming code
 *		1 - bch4
 *		2 - bch8
 *		3 - bch16
 */
static void __am33xx_nand_switch_ecc(struct nand_chip *nand,
		nand_ecc_modes_t hardware, int32_t mode)
{
	struct nand_bch_priv *bch;

	bch = nand->priv;

	/* Reset ecc interface */
	nand->ecc.read_page = NULL;
	nand->ecc.write_page = NULL;
	nand->ecc.read_oob = NULL;
	nand->ecc.write_oob = NULL;
	nand->ecc.hwctl = NULL;
	nand->ecc.correct = NULL;
	nand->ecc.calculate = NULL;

	nand->ecc.mode = hardware;
	/* Setup the ecc configurations again */
	if (hardware == NAND_ECC_HW) {
		if (mode) {
			/* -1 for converting mode to bch type */
			bch->type = mode - 1;
			debug("HW ECC BCH");
			switch (bch->type) {
				case ECC_BCH4:
					nand->ecc.bytes = 8;
					nand->ecc.layout = &hw_bch4_nand_oob;
					bch->nibbles = ECC_BCH4_NIBBLES;
					debug("4 not supported\n");
					return;

				case ECC_BCH16:
					nand->ecc.bytes = 26;
					nand->ecc.layout = &hw_bch16_nand_oob;
					bch->nibbles = ECC_BCH16_NIBBLES;
					debug("16 not supported\n");
					return;

				case ECC_BCH8:
				default:
					nand->ecc.bytes = CONFIG_SYS_NAND_ECCBYTES;
					nand->ecc.layout = &hw_bch8_nand_oob;
					bch->nibbles = ECC_BCH8_NIBBLES;
					debug("8 Selected\n");
			}
			nand->ecc.mode = NAND_ECC_HW;
			nand->ecc.size = 512;
			nand->ecc.read_page = am33xx_read_page_bch;
			nand->ecc.hwctl = am33xx_enable_ecc_bch;
			nand->ecc.correct = am33xx_correct_data_bch;
			nand->ecc.calculate = am33xx_calculate_ecc_bch;
			am33xx_hwecc_init_bch(nand, NAND_ECC_READ);
		} else {
			nand->ecc.layout = &hw_nand_oob;
			nand->ecc.size = 512;
			nand->ecc.bytes = 3;
			nand->ecc.hwctl = am33xx_enable_ecc;
			nand->ecc.correct = am33xx_correct_data;
			nand->ecc.calculate = am33xx_calculate_ecc;
			am33xx_hwecc_init(nand);
			debug("HW ECC Hamming Code selected\n");
		}
	} else if (hardware == NAND_ECC_SOFT) {
		/* Use mtd default settings */
		nand->ecc.layout = NULL;
		debug("SW ECC selected\n");
	} else {
		debug("ECC Disabled\n");
	}
}

/*
 * am33xx_nand_switch_ecc - switch the ECC operation ib/w h/w ecc
 * (i.e. hamming / bch) and s/w ecc.
 * The default is to come up on s/w ecc
 *
 * @hardware -  NAND_ECC_HW -switch to h/w ecc
 *				NAND_ECC_SOFT -switch to s/w ecc
 *
 * @mode -	0 - hamming code
 *		1 - bch4
 *		2 - bch8
 *		3 - bch16
 */
void am33xx_nand_switch_ecc(nand_ecc_modes_t hardware, int32_t mode)
{
	struct nand_chip *nand;
	struct mtd_info *mtd;

	if (nand_curr_device < 0 ||
	    nand_curr_device >= CONFIG_SYS_MAX_NAND_DEVICE) {
		printf("Error: Can't switch ecc, no devices available\n");
		return;
	}

	mtd = &nand_info[nand_curr_device];
	nand = mtd->priv;

	__am33xx_nand_switch_ecc(nand, hardware, mode);

	nand->options |= NAND_OWN_BUFFERS;
	/* Update NAND handling after ECC mode switch */
	nand_scan_tail(mtd);
	nand->options &= ~NAND_OWN_BUFFERS;
}

#else /* CONFIG_SPL_BUILD */
/* Check wait pin as dev ready indicator */
static int am33xx_spl_dev_ready(struct mtd_info *mtd)
{
	int ret;

//	printf("dev status: ");
	ret = readl(&gpmc_cfg->status) & (1 << 8);
//	printf("%d %08x\n", ret, gpmc_cfg->status);
	return ret;
}
#endif

#ifdef CONFIG_SPL_BUILD
/*
 * usually implemented in nand_base.c which is not compiled in SPL_BUILD
 */
void nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readb(chip->IO_ADDR_R);
}

void nand_read_buf16(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;
	u16 *p = (u16 *) buf;
	len >>= 1;

	for (i = 0; i < len; i++)
		p[i] = readw(chip->IO_ADDR_R);
}
#endif

/*
 * Board-specific NAND initialization. The following members of the
 * argument are board-specific:
 * - IO_ADDR_R: address to read the 8 I/O lines of the flash device
 * - IO_ADDR_W: address to write the 8 I/O lines of the flash device
 * - cmd_ctrl: hardwarespecific function for accesing control-lines
 * - waitfunc: hardwarespecific function for accesing device ready/busy line
 * - ecc.hwctl: function to enable (reset) hardware ecc generator
 * - ecc.mode: mode of ecc, see defines
 * - chip_delay: chip dependent delay for transfering data from array to
 *   read regs (tR)
 * - options: various chip options. They can partly be set to inform
 *   nand_scan about special functionality. See the defines for further
 *   explanation
 */
int board_nand_init(struct nand_chip *nand)
{
	/* int32_t gpmc_config = 0; */
	cs = 0;

	/*
	 * xloader/Uboot's gpmc configuration would have configured GPMC for
	 * nand type of memory. The following logic scans and latches on to the
	 * first CS with NAND type memory.
	 * TBD: need to make this logic generic to handle multiple CS NAND
	 * devices.
	 */
	while (cs < GPMC_MAX_CS) {
		/* Check if NAND type is set */
		if ((readl(&gpmc_cfg->cs[cs].config1) & 0xC00) == 0x800) {
			/* Found it!! */
			debug("Searching for NAND device @ GPMC CS:%d\n", cs);
			break;
		}
		cs++;
	}
	if (cs >= GPMC_MAX_CS) {
		printf("NAND: Unable to find NAND settings in "
			"GPMC Configuration - quitting\n");
		return -ENODEV;
	}

	nand->IO_ADDR_R = (void __iomem *)&gpmc_cfg->cs[cs].nand_dat;
	nand->IO_ADDR_W = (void __iomem *)&gpmc_cfg->cs[cs].nand_cmd;

	nand->cmd_ctrl = am33xx_nand_hwcontrol;
	nand->options = NAND_NO_PADDING | NAND_CACHEPRG;
#ifdef CONFIG_SYS_NAND_USE_FLASH_BBT
#ifdef CONFIG_SYS_NAND_NO_OOB
	nand->bbt_options |= NAND_BBT_USE_FLASH | NAND_USE_FLASH_BBT_NO_OOB;
#else
	nand->bbt_options |= NAND_BBT_USE_FLASH;
#endif /* CONFIG_SYS_NAND_NO_OOB */
	nand->bbt_td = &bbt_main_descr;
	nand->bbt_md = &bbt_mirror_descr;
#endif /* CONFIG_SYS_NAND_USE_FLASH_BBT */

	/* If we are 16 bit dev, our gpmc config tells us that */
	if ((readl(&gpmc_cfg->cs[cs].config1) & 0x3000) == 0x1000) {
		nand->options |= NAND_BUSWIDTH_16;
	}

	nand->chip_delay = 100;

	/* required in case of BCH */
	elm_init();

	/* BCH info that will be correct for SPL or overridden otherwise. */
	nand->priv = &bch_priv;

	bch_priv.nibbles = ECC_BCH8_NIBBLES;
	bch_priv.type = ECC_BCH8;
	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.layout = &hw_bch8_nand_oob;
	nand->ecc.size = CONFIG_SYS_NAND_ECCSIZE;
	nand->ecc.bytes = CONFIG_SYS_NAND_ECCBYTES;
	nand->ecc.hwctl = am33xx_enable_ecc_bch;
	nand->ecc.read_page = am33xx_read_page_bch;
	nand->ecc.correct = am33xx_correct_data_bch;
	nand->ecc.calculate = am33xx_calculate_ecc_bch;

#ifndef CONFIG_SPL_BUILD
	nand_curr_device = 0;
#else
	nand->cmdfunc = am33xx_spl_nand_command;

	nand->ecc.steps = CONFIG_SYS_NAND_PAGE_SIZE / CONFIG_SYS_NAND_ECCSIZE;
	nand->ecc.total = CONFIG_SYS_NAND_ECCBYTES * nand->ecc.steps;

	if (nand->options & NAND_BUSWIDTH_16)
		nand->read_buf = nand_read_buf16;
	else
		nand->read_buf = nand_read_buf;

	nand->dev_ready = am33xx_spl_dev_ready;
#endif /* CONFIG_SPL_BUILD */
	am33xx_hwecc_init_bch(nand, NAND_ECC_READ);

	return 0;
}
