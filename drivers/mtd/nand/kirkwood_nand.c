/*
 * (C) Copyright 2009
 * Marvell Semiconductor <www.marvell.com>
 * Written-by: Prafulla Wadaskar <prafulla@marvell.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/kirkwood.h>
#include <nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/nand_bch.h>

#ifndef CONFIG_NAND_ECC_ALGO
#define CONFIG_NAND_ECC_ALGO	NAND_ECC_SOFT
#endif

/* NAND Flash Soc registers */
struct kwnandf_registers {
	u32 rd_params;	/* 0x10418 */
	u32 wr_param;	/* 0x1041c */
	u8  pad[0x10470 - 0x1041c - 4];
	u32 ctrl;	/* 0x10470 */
};

static struct kwnandf_registers *nf_reg =
	(struct kwnandf_registers *)KW_NANDF_BASE;

/*
 * hardware specific access to control-lines/bits
 */
#define NAND_ACTCEBOOT_BIT		0x02

static void kw_nand_hwcontrol(struct mtd_info *mtd, int cmd,
			      unsigned int ctrl)
{
	struct nand_chip *nc = mtd->priv;
	u32 offs;

	if (cmd == NAND_CMD_NONE)
		return;

	if (ctrl & NAND_CLE)
		offs = (1 << 0);	/* Commands with A[1:0] == 01 */
	else if (ctrl & NAND_ALE)
		offs = (1 << 1);	/* Addresses with A[1:0] == 10 */
	else
		return;

	writeb(cmd, nc->IO_ADDR_W + offs);
}

void kw_nand_select_chip(struct mtd_info *mtd, int chip)
{
	u32 data;

	data = readl(&nf_reg->ctrl);
	data |= NAND_ACTCEBOOT_BIT;
	writel(data, &nf_reg->ctrl);
}

#ifdef CONFIG_NAND_ECC_SOFT_RS
static struct nand_ecclayout kw_nand_oob_rs = {
	.eccbytes = 40,
	.eccpos = {
		24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
		34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
		44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
		54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
	},
	.oobfree = {
		{ .offset = 2, .length = 22, },
	},
};

static void kw_nand_switch_eccmode(struct mtd_info *mtd,
				   nand_ecc_modes_t eccmode)
{
	struct nand_chip *nand = mtd->priv;

	if (nand->ecc.mode == eccmode)
		return;

	switch (eccmode) {
	case NAND_ECC_SOFT_RS:
		nand->ecc.mode = NAND_ECC_SOFT_RS;
		nand->ecc.layout = &kw_nand_oob_rs;
		nand->ecc.size = 512;
		nand->ecc.bytes = 10;
		break;
	case NAND_ECC_SOFT_BCH:
		nand->ecc.layout = NULL;
		nand->ecc.bytes = 0;
		nand->ecc.size = 0;
		break;
	case NAND_ECC_SOFT:
		nand->ecc.layout = NULL;
		nand->ecc.size = 0;
		break;
	default:
		printf("Unsupported ecc mode %u\n", eccmode);
		return;
	}
	nand->ecc.mode = eccmode;
	nand->options |= NAND_OWN_BUFFERS;
	nand_scan_tail(mtd);
	nand->options &= ~NAND_OWN_BUFFERS;
}

static int do_kw_nand_switch_eccmode(cmd_tbl_t *cmdtp, int flag, int argc,
				     char *const argv[])
{
	nand_ecc_modes_t mode;
	struct mtd_info *mtd;

	if (argc > 2)
		return CMD_RET_USAGE;
	if (nand_curr_device < 0 ||
	    nand_curr_device >= CONFIG_SYS_MAX_NAND_DEVICE ||
	    !nand_info[nand_curr_device].name) {
		printf("Error: Can't switch ecc, no devices available\n");
		return CMD_RET_FAILURE;
	}
	mtd = &nand_info[nand_curr_device];

	if (argc < 2) {
		struct nand_chip *nand = mtd->priv;
		char *modestr;

		switch (nand->ecc.mode) {
		case NAND_ECC_SOFT:
			modestr = "HAMMING";
			break;
		case NAND_ECC_SOFT_RS:
			modestr = "RS";
			break;
		case NAND_ECC_SOFT_BCH:
			if (mtd_nand_has_bch()) {
				modestr = "BCH";
				break;
			}
			// fallthru
		default:
			printf("Unsupported ECC mode %d in use\n", nand->ecc.mode);
			return CMD_RET_FAILURE;
		}
		printf("NAND ECC mode: %s\n", modestr);
		return CMD_RET_SUCCESS;
	}
	if (strcmp(argv[1], "hamming") == 0 ||
	    strcmp(argv[1], "soft") == 0) {
		mode = NAND_ECC_SOFT;
	} else if (strcmp(argv[1], "rs") == 0) {
		mode = NAND_ECC_SOFT_RS;
	} else if (strcmp(argv[1], "bch") == 0) {
		mode = NAND_ECC_SOFT_BCH;
	} else {
		printf("Unsupported ECC mode '%s'; supported modes are 'hamming', 'RS'%s\n",
		       argv[1], mtd_nand_has_bch() ? ", 'BCH'" : "");
		return CMD_RET_USAGE;
	}

	kw_nand_switch_eccmode(mtd, mode);
	printf("ECC mode switched to %s\n", argv[1]);
	return CMD_RET_SUCCESS;
}
U_BOOT_CMD(nandecc, 2, 0, do_kw_nand_switch_eccmode,
	   "nandecc",
	   "switch ecc mode\n"
	   "\tvalid modes are:"
	   "HAMMING (1 bit ECC; not suitable for Micron NAND)"
	   "BCH 4 bit ECC; (default)"
	   "RS 4 bit ECC; (write only! for compatibility with ROM code)"
	   );
#endif

int board_nand_init(struct nand_chip *nand)
{
	nand->options = NAND_COPYBACK | NAND_CACHEPRG | NAND_NO_PADDING;
#ifndef CONFIG_NAND_ECC_SOFT_RS
	nand->ecc.mode = NAND_ECC_SOFT;
#else
	nand->ecc.mode = NAND_ECC_SOFT_BCH;
#endif
	nand->cmd_ctrl = kw_nand_hwcontrol;
	nand->chip_delay = 40;
	nand->select_chip = kw_nand_select_chip;
	return 0;
}
