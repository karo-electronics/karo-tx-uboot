/*
 * Copyright (C) 2011 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: board/freesclae/mx28_evk.c (C) 2010 Freescale Semiconductor, Inc.
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
#include <asm/arch/regs-pinctrl.h>
#include <asm/arch/pinctrl.h>
#include <asm/arch/regs-clkctrl.h>
#include <asm/arch/regs-ocotp.h>
#include <asm/errno.h>

#include <mmc.h>
#include <imx_ssp_mmc.h>

/* This should be removed after it's added into mach-types.h */

static const int mach_type = MACH_TYPE_TX28;

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_IMX_SSP_MMC

/* MMC pins */
static struct pin_desc mmc0_pins_desc[] = {
	{ PINID_SSP0_DATA0, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA1, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA2, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA3, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA4, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA5, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA6, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DATA7, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_CMD, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_DETECT, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_SSP0_SCK, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
};

static struct pin_group mmc0_pins = {
	.pins		= mmc0_pins_desc,
	.nr_pins	= ARRAY_SIZE(mmc0_pins_desc)
};

struct imx_ssp_mmc_cfg ssp_mmc_cfg[2] = {
	{REGS_SSP0_BASE, HW_CLKCTRL_SSP0, BM_CLKCTRL_CLKSEQ_BYPASS_SSP0},
	{REGS_SSP1_BASE, HW_CLKCTRL_SSP1, BM_CLKCTRL_CLKSEQ_BYPASS_SSP1},
};
#endif

/* ENET pins */
static struct pin_desc enet_pins_desc[] = {
	{ PINID_ENET0_MDC, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_MDIO, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_RX_EN, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_RXD0, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_RXD1, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_TX_EN, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_TXD0, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET0_TXD1, PIN_FUN1, PAD_8MA, PAD_3V3, 1 },
	{ PINID_ENET_CLK, PIN_FUN1, PAD_8MA, PAD_3V3, 1 }
};

static struct pin_group enet_pins = {
	.pins		= enet_pins_desc,
	.nr_pins	= ARRAY_SIZE(enet_pins_desc),
};

static struct pin_desc duart_pins_desc[] = {
	{ PINID_PWM0, PIN_GPIO, PAD_8MA, PAD_3V3, 1 },
	{ PINID_PWM1, PIN_GPIO, PAD_8MA, PAD_3V3, 1 },
	{ PINID_I2C0_SCL, PIN_GPIO, PAD_8MA, PAD_3V3, 1 },
	{ PINID_I2C0_SDA, PIN_GPIO, PAD_8MA, PAD_3V3, 1 },

	{ PINID_AUART0_RTS, PIN_FUN3, PAD_8MA, PAD_3V3, 1 },
	{ PINID_AUART0_CTS, PIN_FUN3, PAD_8MA, PAD_3V3, 1 },
	{ PINID_AUART0_TX, PIN_FUN3, PAD_8MA, PAD_3V3, 1 },
	{ PINID_AUART0_RX, PIN_FUN3, PAD_8MA, PAD_3V3, 1 },
};

static struct pin_group duart_pins = {
	.pins = duart_pins_desc,
	.nr_pins = ARRAY_SIZE(duart_pins_desc),
};

static struct pin_desc gpmi_pins_desc[] = {
	{ PINID_GPMI_D00, PIN_FUN1, },
	{ PINID_GPMI_D01, PIN_FUN1, },
	{ PINID_GPMI_D02, PIN_FUN1, },
	{ PINID_GPMI_D03, PIN_FUN1, },
	{ PINID_GPMI_D04, PIN_FUN1, },
	{ PINID_GPMI_D05, PIN_FUN1, },
	{ PINID_GPMI_D06, PIN_FUN1, },
	{ PINID_GPMI_D07, PIN_FUN1, },
	{ PINID_GPMI_CE0N, PIN_FUN1, },
	{ PINID_GPMI_RDY0, PIN_FUN1, },
	{ PINID_GPMI_RDN, PIN_FUN1, },
	{ PINID_GPMI_WRN, PIN_FUN1, },
	{ PINID_GPMI_ALE, PIN_FUN1, },
	{ PINID_GPMI_CLE, PIN_FUN1, },
	{ PINID_GPMI_RESETN, PIN_FUN1, },
};

static struct pin_group gpmi_pins = {
	.pins		= gpmi_pins_desc,
	.nr_pins	= ARRAY_SIZE(gpmi_pins_desc),
};

/*
 * Functions
 */
static void duart_init(void)
{
	pin_set_group(&duart_pins);
}

int board_init(void)
{
	gd->bd->bi_arch_number = mach_type;

	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;

	duart_init();
	return 0;
}

int dram_init(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;

	return 0;
}

#ifdef CONFIG_IMX_SSP_MMC

#ifdef CONFIG_DYNAMIC_MMC_DEVNO
int get_mmc_env_devno(void)
{
	unsigned long global_boot_mode;

	global_boot_mode = REG_RD_ADDR(GLOBAL_BOOT_MODE_ADDR);
	return ((global_boot_mode & 0xf) == BOOT_MODE_SD1) ? 1 : 0;
}
#endif


u32 ssp_mmc_is_wp(struct mmc *mmc)
{
	return 0;
}

int ssp_mmc_gpio_init(bd_t *bis)
{
	s32 status = 0;
	u32 index = 0;

	for (index = 0; index < CONFIG_SYS_SSP_MMC_NUM; index++) {
		switch (index) {
		case 0:
			/* Set up MMC pins */
			pin_set_group(&mmc0_pins);
			break;

		default:
			printf("Warning: more ssp mmc controllers configured(%d) than supported by the board(2)\n",
				CONFIG_SYS_SSP_MMC_NUM);
			return status;
		}
		status |= imx_ssp_mmc_initialize(bis, &ssp_mmc_cfg[index]);
	}

	return status;
}

int board_mmc_init(bd_t *bis)
{
	if (!ssp_mmc_gpio_init(bis))
		return 0;
	else
		return -1;
}

#endif

#if defined(CONFIG_MXC_FEC) && defined(CONFIG_GET_FEC_MAC_ADDR_FROM_IIM)
int fec_get_mac_addr(unsigned char *mac)
{
	u32 val;
	int timeout = 1000;

	/* set this bit to open the OTP banks for reading */
	REG_WR(REGS_OCOTP_BASE, HW_OCOTP_CTRL_SET,
		BM_OCOTP_CTRL_RD_BANK_OPEN);

	/* wait until OTP contents are readable */
	while (BM_OCOTP_CTRL_BUSY & REG_RD(REGS_OCOTP_BASE, HW_OCOTP_CTRL)) {
		if (timeout-- < 0)
			return -ETIMEDOUT;
		udelay(100);
	}

	val = REG_RD(REGS_OCOTP_BASE, HW_OCOTP_CUSTn(0));
	mac[0] = (val >> 24) & 0xFF;
	mac[1] = (val >> 16) & 0xFF;
	mac[2] = (val >> 8) & 0xFF;
	mac[3] = (val >> 0) & 0xFF;
	val = REG_RD(REGS_OCOTP_BASE, HW_OCOTP_CUSTn(1));
	mac[4] = (val >> 24) & 0xFF;
	mac[5] = (val >> 16) & 0xFF;

	return 0;
}
#endif

void enet_board_init(void)
{
	/* Set up ENET pins */
	pin_set_group(&enet_pins);

	/* Power on the external phy */
	pin_gpio_set(PINID_PWM4, 1);
	pin_gpio_direction(PINID_PWM4, 1);
	pin_set_type(PINID_PWM4, PIN_GPIO);

	/* Reset the external phy */
	pin_gpio_set(PINID_ENET0_RX_CLK, 0);
	pin_gpio_direction(PINID_ENET0_RX_CLK, 1);
	pin_set_type(PINID_ENET0_RX_CLK, PIN_GPIO);
	udelay(200);
	pin_gpio_set(PINID_ENET0_RX_CLK, 1);
}

#ifdef CONFIG_MXS_NAND
#include <linux/mtd/nand.h>
extern int mxs_gpmi_nand_init(struct mtd_info *mtd, struct nand_chip *chip);

int board_nand_init(struct mtd_info *mtd, struct nand_chip *chip)
{
	pin_set_group(&gpmi_pins);
	return mxs_gpmi_nand_init(mtd, chip);
}
#endif

int checkboard(void)
{
	printf("Board: Ka-Ro TX28\n");

	return 0;
}
