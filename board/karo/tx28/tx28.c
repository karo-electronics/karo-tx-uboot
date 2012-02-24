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
#include <errno.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/iomux-mx28.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/regs-pinctrl.h>
#include <asm/arch/regs-clkctrl.h>
#include <asm/arch/regs-ocotp.h>
#include <asm/arch/sys_proto.h>

#include <mmc.h>
#include <imx_ssp_mmc.h>

/* This should be removed after it's added into mach-types.h */

static const int mach_type = MACH_TYPE_TX28;

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_IMX_SSP_MMC
/* MMC pins */
static iomux_cfg_t mmc0_pads[] = {
	MX28_PAD_SSP0_DATA0__SSP0_D0,
	MX28_PAD_SSP0_DATA1__SSP0_D1,
	MX28_PAD_SSP0_DATA2__SSP0_D2,
	MX28_PAD_SSP0_DATA3__SSP0_D3,
	MX28_PAD_SSP0_DATA4__SSP0_D4,
	MX28_PAD_SSP0_DATA5__SSP0_D5,
	MX28_PAD_SSP0_DATA6__SSP0_D6,
	MX28_PAD_SSP0_DATA7__SSP0_D7,
	MX28_PAD_SSP0_CMD__SSP0_CMD,
	MX28_PAD_SSP0_DETECT__SSP0_CARD_DETECT,
	MX28_PAD_SSP0_SCK__SSP0_SCK,
};
#endif

/* ENET pins */
static iomux_cfg_t enet_pads[] = {
	MX28_PAD_PWM4__GPIO_3_29,
	MX28_PAD_ENET0_RX_CLK__GPIO_4_13,
	MX28_PAD_ENET0_MDC__ENET0_MDC,
	MX28_PAD_ENET0_MDIO__ENET0_MDIO,
	MX28_PAD_ENET0_RX_EN__ENET0_RX_EN,
	MX28_PAD_ENET0_RXD0__ENET0_RXD0,
	MX28_PAD_ENET0_RXD1__ENET0_RXD1,
	MX28_PAD_ENET0_TX_EN__ENET0_TX_EN,
	MX28_PAD_ENET0_TXD0__ENET0_TXD0,
	MX28_PAD_ENET0_TXD1__ENET0_TXD1,
	MX28_PAD_ENET_CLK__CLKCTRL_ENET,
};

static iomux_cfg_t duart_pads[] = {
	MX28_PAD_PWM0__GPIO_3_16,
	MX28_PAD_PWM1__GPIO_3_17,
	MX28_PAD_I2C0_SCL__GPIO_3_24,
	MX28_PAD_I2C0_SDA__GPIO_3_25,

	MX28_PAD_AUART0_RTS__AUART0_RTS,
	MX28_PAD_AUART0_CTS__AUART0_CTS,
	MX28_PAD_AUART0_TX__AUART0_TX,
	MX28_PAD_AUART0_RX__AUART0_RX,
};

static iomux_cfg_t gpmi_pads[] = {
	MX28_PAD_GPMI_D00__GPMI_D0,
	MX28_PAD_GPMI_D01__GPMI_D1,
	MX28_PAD_GPMI_D02__GPMI_D2,
	MX28_PAD_GPMI_D03__GPMI_D3,
	MX28_PAD_GPMI_D04__GPMI_D4,
	MX28_PAD_GPMI_D05__GPMI_D5,
	MX28_PAD_GPMI_D06__GPMI_D6,
	MX28_PAD_GPMI_D07__GPMI_D7,
	MX28_PAD_GPMI_CE0N__GPMI_CE0N,
	MX28_PAD_GPMI_RDY0__GPMI_READY0,
	MX28_PAD_GPMI_RDN__GPMI_RDN,
	MX28_PAD_GPMI_WRN__GPMI_WRN,
	MX28_PAD_GPMI_ALE__GPMI_ALE,
	MX28_PAD_GPMI_CLE__GPMI_CLE,
	MX28_PAD_GPMI_RESETN__GPMI_RESETN,
};

/*
 * Functions
 */
static void duart_init(void)
{
	mx28_common_spl_init(&duart_pads, ARRAY_SIZE(duart_pads));
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

#ifdef CONFIG_DYNAMIC_MMC_DEVNO
int get_mmc_env_devno(void)
{
	unsigned long global_boot_mode;

	global_boot_mode = REG_RD_ADDR(GLOBAL_BOOT_MODE_ADDR);
	return ((global_boot_mode & 0xf) == BOOT_MODE_SD1) ? 1 : 0;
}
#endif

#if defined(CONFIG_MXC_FEC) && defined(CONFIG_GET_FEC_MAC_ADDR_FROM_IIM)
int fec_get_mac_addr(unsigned char *mac)
{
	u32 val;
	int timeout = 1000;
	struct mx28_ocotp_regs *ocotp_regs =
		(struct mx28_ocotp_regs *)MXS_OCOTP_BASE;

	/* set this bit to open the OTP banks for reading */
	writel(OCOTP_CTRL_RD_BANK_OPEN,
		ocotp_regs->hw_ocotp_ctrl_set);

	/* wait until OTP contents are readable */
	while (OCOTP_CTRL_BUSY & readl(ocotp_regs->hw_ocotp_ctrl)) {
		if (timeout-- < 0)
			return -ETIMEDOUT;
		udelay(100);
	}

	val = readl(ocotp_regs->hw_ocotp_cust0);
	mac[0] = (val >> 24) & 0xFF;
	mac[1] = (val >> 16) & 0xFF;
	mac[2] = (val >> 8) & 0xFF;
	mac[3] = (val >> 0) & 0xFF;
	val = readl(ocotp_regs->hw_ocotp_cust1);
	mac[4] = (val >> 24) & 0xFF;
	mac[5] = (val >> 16) & 0xFF;

	return 0;
}
#endif

void enet_board_init(void)
{
	/* Set up ENET pins */
	mx28_common_spl_init(&enet_pads, ARRAY_SIZE(enet_pads));

	/* Power on the external phy */
	gpio_direction_output(MX28_PAD_PWM4__GPIO_3_29, 1);

	/* Reset the external phy */
	gpio_direction_output(MX28_PAD_ENET0_RX_CLK__GPIO_4_13, 1);
	udelay(200);
	gpio_set_value(MX28_PAD_ENET0_RX_CLK__GPIO_4_13, 0);
}

#ifdef CONFIG_MXS_NAND
#include <linux/mtd/nand.h>
extern int mxs_gpmi_nand_init(struct mtd_info *mtd, struct nand_chip *chip);

int board_nand_init(struct mtd_info *mtd, struct nand_chip *chip)
{
	mx28_common_spl_init(&gpmi_pads, ARRAY_SIZE(gpmi_pads));
	return mxs_gpmi_nand_init(mtd, chip);
}
#endif

int checkboard(void)
{
	printf("Board: Ka-Ro TX28\n");

	return 0;
}
