/*
 * Copyright (C) 2011-2013 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: board/freescale/mx28_evk.c (C) 2010 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <common.h>
#include <errno.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <i2c.h>
#include <lcd.h>
#include <netdev.h>
#include <mmc.h>
#include <fsl_esdhc.h>
#include <video_fb.h>
#include <ipu.h>
#include <mxcfb.h>
#include <linux/fb.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/iomux-mx53.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/sys_proto.h>

#include "../common/karo.h"

#define TX53_FEC_RST_GPIO	IMX_GPIO_NR(7, 6)
#define TX53_FEC_PWR_GPIO	IMX_GPIO_NR(3, 20)
#define TX53_FEC_INT_GPIO	IMX_GPIO_NR(2, 4)
#define TX53_LED_GPIO		IMX_GPIO_NR(2, 20)

#define TX53_LCD_PWR_GPIO	IMX_GPIO_NR(2, 31)
#define TX53_LCD_RST_GPIO	IMX_GPIO_NR(3, 29)
#define TX53_LCD_BACKLIGHT_GPIO	IMX_GPIO_NR(1, 1)

#define TX53_RESET_OUT_GPIO	IMX_GPIO_NR(7, 12)

DECLARE_GLOBAL_DATA_PTR;

#define MX53_GPIO_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_PKE | PAD_CTL_PUE | \
				PAD_CTL_DSE_HIGH | PAD_CTL_PUS_22K_UP)

#define TX53_SDHC_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_HYS | PAD_CTL_DSE_HIGH |	\
				PAD_CTL_SRE_FAST | PAD_CTL_PUS_47K_UP)

static iomux_v3_cfg_t tx53_pads[] = {
	/* NAND flash pads are set up in lowlevel_init.S */

	/* UART pads */
#if CONFIG_MXC_UART_BASE == UART1_BASE
	MX53_PAD_PATA_DIOW__UART1_TXD_MUX,
	MX53_PAD_PATA_DMACK__UART1_RXD_MUX,
	MX53_PAD_PATA_IORDY__UART1_RTS,
	MX53_PAD_PATA_RESET_B__UART1_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART2_BASE
	MX53_PAD_PATA_BUFFER_EN__UART2_RXD_MUX,
	MX53_PAD_PATA_DMARQ__UART2_TXD_MUX,
	MX53_PAD_PATA_DIOR__UART2_RTS,
	MX53_PAD_PATA_INTRQ__UART2_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART3_BASE
	MX53_PAD_PATA_CS_0__UART3_TXD_MUX,
	MX53_PAD_PATA_CS_1__UART3_RXD_MUX,
	MX53_PAD_PATA_DA_2__UART3_RTS,
	MX53_PAD_PATA_DA_1__UART3_CTS,
#endif
	/* internal I2C */
	MX53_PAD_EIM_D28__I2C1_SDA | MX53_GPIO_PAD_CTRL,
	MX53_PAD_EIM_D21__I2C1_SCL | MX53_GPIO_PAD_CTRL,

	/* FEC PHY GPIO functions */
	MX53_PAD_EIM_D20__GPIO3_20, /* PHY POWER */
	MX53_PAD_PATA_DA_0__GPIO7_6, /* PHY RESET */
	MX53_PAD_PATA_DATA4__GPIO2_4, /* PHY INT */

	/* FEC functions */
	MX53_PAD_FEC_MDC__FEC_MDC,
	MX53_PAD_FEC_MDIO__FEC_MDIO,
	MX53_PAD_FEC_REF_CLK__FEC_TX_CLK,
	MX53_PAD_FEC_RX_ER__FEC_RX_ER,
	MX53_PAD_FEC_CRS_DV__FEC_RX_DV,
	MX53_PAD_FEC_RXD1__FEC_RDATA_1,
	MX53_PAD_FEC_RXD0__FEC_RDATA_0,
	MX53_PAD_FEC_TX_EN__FEC_TX_EN,
	MX53_PAD_FEC_TXD1__FEC_TDATA_1,
	MX53_PAD_FEC_TXD0__FEC_TDATA_0,
};

static const struct gpio tx53_gpios[] = {
	{ TX53_RESET_OUT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "#RESET_OUT", },
	{ TX53_FEC_PWR_GPIO, GPIOF_OUTPUT_INIT_HIGH, "FEC PHY PWR", },
	{ TX53_FEC_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "FEC PHY RESET", },
	{ TX53_FEC_INT_GPIO, GPIOF_INPUT, "FEC PHY INT", },
};

/*
 * Functions
 */
/* placed in section '.data' to prevent overwriting relocation info
 * overlayed with bss
 */
static u32 wrsr __attribute__((section(".data")));

#define WRSR_POR	(1 << 4)
#define WRSR_TOUT	(1 << 1)
#define WRSR_SFTW	(1 << 0)

static void print_reset_cause(void)
{
	struct src *src_regs = (struct src *)SRC_BASE_ADDR;
	void __iomem *wdt_base = (void __iomem *)WDOG1_BASE_ADDR;
	u32 srsr;
	char *dlm = "";

	printf("Reset cause: ");

	srsr = readl(&src_regs->srsr);
	wrsr = readw(wdt_base + 4);

	if (wrsr & WRSR_POR) {
		printf("%sPOR", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00004) {
		printf("%sCSU", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00008) {
		printf("%sIPP USER", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00010) {
		if (wrsr & WRSR_SFTW) {
			printf("%sSOFT", dlm);
			dlm = " | ";
		}
		if (wrsr & WRSR_TOUT) {
			printf("%sWDOG", dlm);
			dlm = " | ";
		}
	}
	if (srsr & 0x00020) {
		printf("%sJTAG HIGH-Z", dlm);
		dlm = " | ";
	}
	if (srsr & 0x00040) {
		printf("%sJTAG SW", dlm);
		dlm = " | ";
	}
	if (srsr & 0x10000) {
		printf("%sWARM BOOT", dlm);
		dlm = " | ";
	}
	if (dlm[0] == '\0')
		printf("unknown");

	printf("\n");
}

static void tx53_print_cpuinfo(void)
{
	u32 cpurev;

	cpurev = get_cpu_rev();

	printf("CPU:   Freescale i.MX53 rev%d.%d at %d MHz\n",
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		mxc_get_clock(MXC_ARM_CLK) / 1000000);

	print_reset_cause();
}

enum LTC3589_REGS {
	LTC3589_SCR1 = 0x07,
	LTC3589_CLIRQ = 0x21,
	LTC3589_B1DTV1 = 0x23,
	LTC3589_B1DTV2 = 0x24,
	LTC3589_VRRCR = 0x25,
	LTC3589_B2DTV1 = 0x26,
	LTC3589_B2DTV2 = 0x27,
	LTC3589_B3DTV1 = 0x29,
	LTC3589_B3DTV2 = 0x2a,
	LTC3589_L2DTV1 = 0x32,
	LTC3589_L2DTV2 = 0x33,
};

#define LTC3589_PGOOD_MASK	(1 << 5)

#define LTC3589_CLK_RATE_LOW	(1 << 5)

#define VDD_LDO2_10mV		vout_to_vref(1325 * 10, 2)
#define VDD_CORE_10mV		vout_to_vref(1240 * 10, 3)
#define VDD_SOC_10mV		vout_to_vref(1325 * 10, 4)
#define VDD_BUCK3_10mV		vout_to_vref(2500 * 10, 5)

#ifndef CONFIG_SYS_TX53_HWREV_2
/* LDO2 vref divider */
#define R1_2	180
#define R2_2	191
/* BUCK1 vref divider */
#define R1_3	150
#define R2_3	180
/* BUCK2 vref divider */
#define R1_4	180
#define R2_4	191
/* BUCK3 vref divider */
#define R1_5	270
#define R2_5	100
#else
/* no dividers on vref */
#define R1_2	0
#define R2_2	1
#define R1_3	0
#define R2_3	1
#define R1_4	0
#define R2_4	1
#define R1_5	0
#define R2_5	1
#endif

/* calculate voltages in 10mV */
#define R1(idx)			R1_##idx
#define R2(idx)			R2_##idx

#define vout_to_vref(vout, idx)	((vout) * R2(idx) / (R1(idx) + R2(idx)))
#define vref_to_vout(vref, idx)	((vref) * (R1(idx) + R2(idx)) / R2(idx))

#define mV_to_regval(mV)	(((((mV) < 3625) ? 3625 : (mV)) - 3625) / 125)
#define regval_to_mV(v)		(((v) * 125 + 3625))

static struct pmic_regs {
	enum LTC3589_REGS addr;
	u8 val;
} ltc3589_regs[] = {
	{ LTC3589_SCR1, 0x15, }, /* burst mode for all regulators */

	{ LTC3589_L2DTV1, mV_to_regval(VDD_LDO2_10mV) | LTC3589_PGOOD_MASK, },
	{ LTC3589_L2DTV2, mV_to_regval(VDD_LDO2_10mV) | LTC3589_CLK_RATE_LOW, },

	{ LTC3589_B1DTV1, mV_to_regval(VDD_CORE_10mV) | LTC3589_PGOOD_MASK, },
	{ LTC3589_B1DTV2, mV_to_regval(VDD_CORE_10mV), },

	{ LTC3589_B2DTV1, mV_to_regval(VDD_SOC_10mV) | LTC3589_PGOOD_MASK, },
	{ LTC3589_B2DTV2, mV_to_regval(VDD_SOC_10mV), },

	{ LTC3589_B3DTV1, mV_to_regval(VDD_BUCK3_10mV) | LTC3589_PGOOD_MASK, },
	{ LTC3589_B3DTV2, mV_to_regval(VDD_BUCK3_10mV), },

	{ LTC3589_CLIRQ, 0, }, /* clear all interrupt flags */
};

static int setup_pmic_voltages(void)
{
	int ret;
	unsigned char value;
	int i;

	ret = i2c_probe(CONFIG_SYS_I2C_SLAVE);
	if (ret != 0) {
		printf("Failed to initialize I2C\n");
		return ret;
	}

	ret = i2c_read(CONFIG_SYS_I2C_SLAVE, 0x11, 1, &value, 1);
	if (ret) {
		printf("%s: i2c_read error: %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(ltc3589_regs); i++) {
		ret = i2c_read(CONFIG_SYS_I2C_SLAVE, ltc3589_regs[i].addr, 1,
				&value, 1);
		debug("Writing %02x to reg %02x (%02x)\n",
			ltc3589_regs[i].val, ltc3589_regs[i].addr, value);
		ret = i2c_write(CONFIG_SYS_I2C_SLAVE, ltc3589_regs[i].addr, 1,
				&ltc3589_regs[i].val, 1);
		if (ret) {
			printf("%s: failed to write PMIC register %02x: %d\n",
				__func__, ltc3589_regs[i].addr, ret);
			return ret;
		}
	}
	printf("VDDCORE set to %3d.%dmV\n",
		vref_to_vout(regval_to_mV(mV_to_regval(VDD_CORE_10mV)), 3) / 10,
		vref_to_vout(regval_to_mV(mV_to_regval(VDD_CORE_10mV)), 3) % 10);

	printf("VDDSOC  set to %3d.%dmV\n",
		vref_to_vout(regval_to_mV(mV_to_regval(VDD_SOC_10mV)), 4) / 10,
		vref_to_vout(regval_to_mV(mV_to_regval(VDD_SOC_10mV)), 4) % 10);
	return 0;
}

int board_early_init_f(void)
{
	struct mxc_ccm_reg *ccm_regs = (void *)CCM_BASE_ADDR;

	gpio_request_array(tx53_gpios, ARRAY_SIZE(tx53_gpios));
	imx_iomux_v3_setup_multiple_pads(tx53_pads, ARRAY_SIZE(tx53_pads));

	writel(0x77777777, AIPS1_BASE_ADDR + 0x00);
	writel(0x77777777, AIPS1_BASE_ADDR + 0x04);

	writel(0x00000000, AIPS1_BASE_ADDR + 0x40);
	writel(0x00000000, AIPS1_BASE_ADDR + 0x44);
	writel(0x00000000, AIPS1_BASE_ADDR + 0x48);
	writel(0x00000000, AIPS1_BASE_ADDR + 0x4c);
	writel(0x00000000, AIPS1_BASE_ADDR + 0x50);

	writel(0x77777777, AIPS2_BASE_ADDR + 0x00);
	writel(0x77777777, AIPS2_BASE_ADDR + 0x04);

	writel(0x00000000, AIPS2_BASE_ADDR + 0x40);
	writel(0x00000000, AIPS2_BASE_ADDR + 0x44);
	writel(0x00000000, AIPS2_BASE_ADDR + 0x48);
	writel(0x00000000, AIPS2_BASE_ADDR + 0x4c);
	writel(0x00000000, AIPS2_BASE_ADDR + 0x50);

	writel(0xffcf0fff, &ccm_regs->CCGR0);
	writel(0x000fffc3, &ccm_regs->CCGR1);
	writel(0x033c0000, &ccm_regs->CCGR2);
	writel(0x000000ff, &ccm_regs->CCGR3);
	writel(0x00000000, &ccm_regs->CCGR4);
	writel(0x00fff033, &ccm_regs->CCGR5);
	writel(0x0f00030f, &ccm_regs->CCGR6);
	writel(0xfff00000, &ccm_regs->CCGR7);
	writel(0x00000000, &ccm_regs->cmeor);

	return 0;
}

int board_init(void)
{
	int ret;

	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;

	if (ctrlc() || (wrsr & WRSR_TOUT)) {
		printf("CTRL-C detected; Skipping PMIC setup\n");
		return 1;
	}

	ret = setup_pmic_voltages();
	if (ret) {
		printf("Failed to setup PMIC voltages\n");
		hang();
	}
	return 0;
}

int dram_init(void)
{
	int ret;

	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				PHYS_SDRAM_1_SIZE);

	ret = mxc_set_clock(CONFIG_SYS_MX5_HCLK,
		CONFIG_SYS_SDRAM_CLK, MXC_DDR_CLK);
	if (ret)
		printf("%s: Failed to set DDR clock to %u MHz: %d\n", __func__,
			CONFIG_SYS_SDRAM_CLK, ret);
	else
		debug("%s: DDR clock set to %u.%03u MHz (desig.: %u.000 MHz)\n",
			__func__, mxc_get_clock(MXC_DDR_CLK) / 1000000,
			mxc_get_clock(MXC_DDR_CLK) / 1000 % 1000,
			CONFIG_SYS_SDRAM_CLK);
	return ret;
}

void dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = get_ram_size((void *)PHYS_SDRAM_1,
			PHYS_SDRAM_1_SIZE);
#if CONFIG_NR_DRAM_BANKS > 1
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = get_ram_size((void *)PHYS_SDRAM_2,
			PHYS_SDRAM_2_SIZE);
#endif
}

#ifdef	CONFIG_CMD_MMC
static const iomux_v3_cfg_t mmc0_pads[] = {
	MX53_PAD_SD1_CMD__ESDHC1_CMD | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD1_CLK__ESDHC1_CLK | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD1_DATA0__ESDHC1_DAT0 | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD1_DATA1__ESDHC1_DAT1 | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD1_DATA2__ESDHC1_DAT2 | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD1_DATA3__ESDHC1_DAT3 | TX53_SDHC_PAD_CTRL,
	/* SD1 CD */
	MX53_PAD_EIM_D24__GPIO3_24 | MX53_GPIO_PAD_CTRL,
};

static const iomux_v3_cfg_t mmc1_pads[] = {
	MX53_PAD_SD2_CMD__ESDHC2_CMD | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD2_CLK__ESDHC2_CLK | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD2_DATA0__ESDHC2_DAT0 | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD2_DATA1__ESDHC2_DAT1 | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD2_DATA2__ESDHC2_DAT2 | TX53_SDHC_PAD_CTRL,
	MX53_PAD_SD2_DATA3__ESDHC2_DAT3 | TX53_SDHC_PAD_CTRL,
	/* SD2 CD */
	MX53_PAD_EIM_D25__GPIO3_25 | MX53_GPIO_PAD_CTRL,
};

static struct tx53_esdhc_cfg {
	const iomux_v3_cfg_t *pads;
	int num_pads;
	struct fsl_esdhc_cfg cfg;
	int cd_gpio;
} tx53_esdhc_cfg[] = {
	{
		.pads = mmc0_pads,
		.num_pads = ARRAY_SIZE(mmc0_pads),
		.cfg = {
			.esdhc_base = (void __iomem *)MMC_SDHC1_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = IMX_GPIO_NR(3, 24),
	},
	{
		.pads = mmc1_pads,
		.num_pads = ARRAY_SIZE(mmc1_pads),
		.cfg = {
			.esdhc_base = (void __iomem *)MMC_SDHC2_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = IMX_GPIO_NR(3, 25),
	},
};

#define to_tx53_esdhc_cfg(p) container_of(p, struct tx53_esdhc_cfg, cfg)

int board_mmc_getcd(struct mmc *mmc)
{
	struct tx53_esdhc_cfg *cfg = to_tx53_esdhc_cfg(mmc->priv);

	if (cfg->cd_gpio < 0)
		return cfg->cd_gpio;

	debug("SD card %d is %spresent\n",
		cfg - tx53_esdhc_cfg,
		gpio_get_value(cfg->cd_gpio) ? "NOT " : "");
	return !gpio_get_value(cfg->cd_gpio);
}

int board_mmc_init(bd_t *bis)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tx53_esdhc_cfg); i++) {
		struct mmc *mmc;
		struct tx53_esdhc_cfg *cfg = &tx53_esdhc_cfg[i];
		int ret;

		if (i >= CONFIG_SYS_FSL_ESDHC_NUM)
			break;

		imx_iomux_v3_setup_multiple_pads(cfg->pads,
						cfg->num_pads);
		cfg->cfg.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);

		fsl_esdhc_initialize(bis, &cfg->cfg);

		ret = gpio_request_one(cfg->cd_gpio,
				GPIOF_INPUT, "MMC CD");
		if (ret) {
			printf("Error %d requesting GPIO%d_%d\n",
				ret, cfg->cd_gpio / 32, cfg->cd_gpio % 32);
			continue;
		}

		mmc = find_mmc_device(i);
		if (mmc == NULL)
			continue;
		if (board_mmc_getcd(mmc) > 0)
			mmc_init(mmc);
	}
	return 0;
}
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_FEC_MXC

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

void imx_get_mac_from_fuse(int dev_id, unsigned char *mac)
{
	int i;
	struct iim_regs *iim = (struct iim_regs *)IMX_IIM_BASE;
	struct fuse_bank *bank = &iim->bank[1];
	struct fuse_bank1_regs *fuse = (struct fuse_bank1_regs *)bank->fuse_regs;

	if (dev_id > 0)
		return;

	for (i = 0; i < ETH_ALEN; i++)
		mac[i] = readl(&fuse->mac_addr[i]);
}

#define FEC_PAD_CTL	(PAD_CTL_DVS | PAD_CTL_DSE_HIGH | \
			PAD_CTL_SRE_FAST)
#define FEC_PAD_CTL2	(PAD_CTL_DVS | PAD_CTL_SRE_FAST)
#define GPIO_PAD_CTL	(PAD_CTL_DVS | PAD_CTL_DSE_HIGH)

int board_eth_init(bd_t *bis)
{
	int ret;

	/* delay at least 21ms for the PHY internal POR signal to deassert */
	udelay(22000);

	/* Deassert RESET to the external phy */
	gpio_set_value(TX53_FEC_RST_GPIO, 1);

	ret = cpu_eth_init(bis);
	if (ret)
		printf("cpu_eth_init() failed: %d\n", ret);
	return ret;
}
#endif /* CONFIG_FEC_MXC */

enum {
	LED_STATE_INIT = -1,
	LED_STATE_OFF,
	LED_STATE_ON,
};

void show_activity(int arg)
{
	static int led_state = LED_STATE_INIT;
	static ulong last;

	if (led_state == LED_STATE_INIT) {
		last = get_timer(0);
		gpio_set_value(TX53_LED_GPIO, 1);
		led_state = LED_STATE_ON;
	} else {
		if (get_timer(last) > CONFIG_SYS_HZ) {
			last = get_timer(0);
			if (led_state == LED_STATE_ON) {
				gpio_set_value(TX53_LED_GPIO, 0);
			} else {
				gpio_set_value(TX53_LED_GPIO, 1);
			}
			led_state = 1 - led_state;
		}
	}
}

static const iomux_v3_cfg_t stk5_pads[] = {
	/* SW controlled LED on STK5 baseboard */
	MX53_PAD_EIM_A18__GPIO2_20,

	/* I2C bus on DIMM pins 40/41 */
	MX53_PAD_GPIO_6__I2C3_SDA | MX53_GPIO_PAD_CTRL,
	MX53_PAD_GPIO_3__I2C3_SCL | MX53_GPIO_PAD_CTRL,

	/* TSC200x PEN IRQ */
	MX53_PAD_EIM_D26__GPIO3_26 | MX53_GPIO_PAD_CTRL,

	/* EDT-FT5x06 Polytouch panel */
	MX53_PAD_NANDF_CS2__GPIO6_15 | MX53_GPIO_PAD_CTRL, /* IRQ */
	MX53_PAD_EIM_A16__GPIO2_22 | MX53_GPIO_PAD_CTRL, /* RESET */
	MX53_PAD_EIM_A17__GPIO2_21 | MX53_GPIO_PAD_CTRL, /* WAKE */

	/* USBH1 */
	MX53_PAD_EIM_D31__GPIO3_31 | MX53_GPIO_PAD_CTRL, /* VBUSEN */
	MX53_PAD_EIM_D30__GPIO3_30 | MX53_GPIO_PAD_CTRL, /* OC */
	/* USBOTG */
	MX53_PAD_GPIO_7__GPIO1_7, /* VBUSEN */
	MX53_PAD_GPIO_8__GPIO1_8, /* OC */

	/* DS1339 Interrupt */
	MX53_PAD_DI0_PIN4__GPIO4_20 | MX53_GPIO_PAD_CTRL,
};

static const struct gpio stk5_gpios[] = {
	{ TX53_LED_GPIO, GPIOF_OUTPUT_INIT_LOW, "HEARTBEAT LED", },

	{ IMX_GPIO_NR(1, 8), GPIOF_INPUT, "USBOTG OC", },
	{ IMX_GPIO_NR(1, 7), GPIOF_OUTPUT_INIT_LOW, "USBOTG VBUS enable", },
	{ IMX_GPIO_NR(3, 30), GPIOF_INPUT, "USBH1 OC", },
	{ IMX_GPIO_NR(3, 31), GPIOF_OUTPUT_INIT_LOW, "USBH1 VBUS enable", },
};

#ifdef CONFIG_LCD
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 1600,
	.vl_row = 1200,

	.vl_bpix = LCD_COLOR24,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
};

static struct fb_videomode tx53_fb_modes[] = {
	{
		/* Standard VGA timing */
		.name		= "VGA",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(25175),
		.left_margin	= 48,
		.hsync_len	= 96,
		.right_margin	= 16,
		.upper_margin	= 31,
		.vsync_len	= 2,
		.lower_margin	= 12,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ETV570 640 x 480 display. Syncs low active,
		 * DE high active, 115.2 mm x 86.4 mm display area
		 * VGA compatible timing
		 */
		.name		= "ETV570",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(25175),
		.left_margin	= 114,
		.hsync_len	= 30,
		.right_margin	= 16,
		.upper_margin	= 32,
		.vsync_len	= 3,
		.lower_margin	= 10,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0350G0DH6 320 x 240 display.
		 * 70.08 mm x 52.56 mm display area.
		 */
		.name		= "ET0350",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= KHZ2PICOS(6500),
		.left_margin	= 68 - 34,
		.hsync_len	= 34,
		.right_margin	= 20,
		.upper_margin	= 18 - 3,
		.vsync_len	= 3,
		.lower_margin	= 4,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0430G0DH6 480 x 272 display.
		 * 95.04 mm x 53.856 mm display area.
		 */
		.name		= "ET0430",
		.refresh	= 60,
		.xres		= 480,
		.yres		= 272,
		.pixclock	= KHZ2PICOS(9000),
		.left_margin	= 2,
		.hsync_len	= 41,
		.right_margin	= 2,
		.upper_margin	= 2,
		.vsync_len	= 10,
		.lower_margin	= 2,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0500G0DH6 800 x 480 display.
		 * 109.6 mm x 66.4 mm display area.
		 */
		.name		= "ET0500",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ETQ570G0DH6 320 x 240 display.
		 * 115.2 mm x 86.4 mm display area.
		 */
		.name		= "ETQ570",
		.refresh	= 60,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= KHZ2PICOS(6400),
		.left_margin	= 38,
		.hsync_len	= 30,
		.right_margin	= 30,
		.upper_margin	= 16, /* 15 according to datasheet */
		.vsync_len	= 3, /* TVP -> 1>x>5 */
		.lower_margin	= 4, /* 4.5 according to datasheet */
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* Emerging ET0700G0DH6 800 x 480 display.
		 * 152.4 mm x 91.44 mm display area.
		 */
		.name		= "ET0700",
		.refresh	= 60,
		.xres		= 800,
		.yres		= 480,
		.pixclock	= KHZ2PICOS(33260),
		.left_margin	= 216 - 128,
		.hsync_len	= 128,
		.right_margin	= 1056 - 800 - 216,
		.upper_margin	= 35 - 2,
		.vsync_len	= 2,
		.lower_margin	= 525 - 480 - 35,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
	{
		/* unnamed entry for assigning parameters parsed from 'video_mode' string */
		.refresh	= 60,
		.left_margin	= 48,
		.hsync_len	= 96,
		.right_margin	= 16,
		.upper_margin	= 31,
		.vsync_len	= 2,
		.lower_margin	= 12,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
};

static int lcd_enabled = 1;

void lcd_enable(void)
{
	/* HACK ALERT:
	 * global variable from common/lcd.c
	 * Set to 0 here to prevent messages from going to LCD
	 * rather than serial console
	 */
	lcd_is_enabled = 0;

	if (lcd_enabled) {
		karo_load_splashimage(1);

		debug("Switching LCD on\n");
		gpio_set_value(TX53_LCD_PWR_GPIO, 1);
		udelay(100);
		gpio_set_value(TX53_LCD_RST_GPIO, 1);
		udelay(300000);
		gpio_set_value(TX53_LCD_BACKLIGHT_GPIO, 0);
	}
}

void lcd_disable(void)
{
	if (lcd_enabled) {
		printf("Disabling LCD\n");
		ipuv3_fb_shutdown();
	}
}

void lcd_panel_disable(void)
{
	if (lcd_enabled) {
		debug("Switching LCD off\n");
		gpio_set_value(TX53_LCD_BACKLIGHT_GPIO, 1);
		gpio_set_value(TX53_LCD_RST_GPIO, 0);
		gpio_set_value(TX53_LCD_PWR_GPIO, 0);
	}
}

static const iomux_v3_cfg_t stk5_lcd_pads[] = {
	/* LCD RESET */
	MX53_PAD_EIM_D29__GPIO3_29 | MX53_GPIO_PAD_CTRL,
	/* LCD POWER_ENABLE */
	MX53_PAD_EIM_EB3__GPIO2_31 | MX53_GPIO_PAD_CTRL,
	/* LCD Backlight (PWM) */
	MX53_PAD_GPIO_1__GPIO1_1 | MX53_GPIO_PAD_CTRL,

	/* Display */
	MX53_PAD_DI0_DISP_CLK__IPU_DI0_DISP_CLK,
	MX53_PAD_DI0_PIN15__IPU_DI0_PIN15,
	MX53_PAD_DI0_PIN2__IPU_DI0_PIN2,
	MX53_PAD_DI0_PIN3__IPU_DI0_PIN3,
	MX53_PAD_DISP0_DAT0__IPU_DISP0_DAT_0,
	MX53_PAD_DISP0_DAT1__IPU_DISP0_DAT_1,
	MX53_PAD_DISP0_DAT2__IPU_DISP0_DAT_2,
	MX53_PAD_DISP0_DAT3__IPU_DISP0_DAT_3,
	MX53_PAD_DISP0_DAT4__IPU_DISP0_DAT_4,
	MX53_PAD_DISP0_DAT5__IPU_DISP0_DAT_5,
	MX53_PAD_DISP0_DAT6__IPU_DISP0_DAT_6,
	MX53_PAD_DISP0_DAT7__IPU_DISP0_DAT_7,
	MX53_PAD_DISP0_DAT8__IPU_DISP0_DAT_8,
	MX53_PAD_DISP0_DAT9__IPU_DISP0_DAT_9,
	MX53_PAD_DISP0_DAT10__IPU_DISP0_DAT_10,
	MX53_PAD_DISP0_DAT11__IPU_DISP0_DAT_11,
	MX53_PAD_DISP0_DAT12__IPU_DISP0_DAT_12,
	MX53_PAD_DISP0_DAT13__IPU_DISP0_DAT_13,
	MX53_PAD_DISP0_DAT14__IPU_DISP0_DAT_14,
	MX53_PAD_DISP0_DAT15__IPU_DISP0_DAT_15,
	MX53_PAD_DISP0_DAT16__IPU_DISP0_DAT_16,
	MX53_PAD_DISP0_DAT17__IPU_DISP0_DAT_17,
	MX53_PAD_DISP0_DAT18__IPU_DISP0_DAT_18,
	MX53_PAD_DISP0_DAT19__IPU_DISP0_DAT_19,
	MX53_PAD_DISP0_DAT20__IPU_DISP0_DAT_20,
	MX53_PAD_DISP0_DAT21__IPU_DISP0_DAT_21,
	MX53_PAD_DISP0_DAT22__IPU_DISP0_DAT_22,
	MX53_PAD_DISP0_DAT23__IPU_DISP0_DAT_23,

	/* LVDS option */
	MX53_PAD_LVDS1_TX3_P__LDB_LVDS1_TX3,
	MX53_PAD_LVDS1_TX2_P__LDB_LVDS1_TX2,
	MX53_PAD_LVDS1_CLK_P__LDB_LVDS1_CLK,
	MX53_PAD_LVDS1_TX1_P__LDB_LVDS1_TX1,
	MX53_PAD_LVDS1_TX0_P__LDB_LVDS1_TX0,
	MX53_PAD_LVDS0_TX3_P__LDB_LVDS0_TX3,
	MX53_PAD_LVDS0_CLK_P__LDB_LVDS0_CLK,
	MX53_PAD_LVDS0_TX2_P__LDB_LVDS0_TX2,
	MX53_PAD_LVDS0_TX1_P__LDB_LVDS0_TX1,
	MX53_PAD_LVDS0_TX0_P__LDB_LVDS0_TX0,
};

static const struct gpio stk5_lcd_gpios[] = {
	{ TX53_LCD_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD RESET", },
	{ TX53_LCD_PWR_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD POWER", },
	{ TX53_LCD_BACKLIGHT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

void lcd_ctrl_init(void *lcdbase)
{
	int color_depth = 24;
	const char *video_mode = getenv("video_mode");
	const char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p = &tx53_fb_modes[0];
	struct fb_videomode fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;
	int pix_fmt = 0;
	ipu_di_clk_parent_t di_clk_parent = DI_PCLK_PLL3;
	unsigned long di_clk_rate = 65000000;

	if (!lcd_enabled) {
		debug("LCD disabled\n");
		return;
	}

	if (had_ctrlc() || (wrsr & WRSR_TOUT)) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		setenv("splashimage", NULL);
		return;
	}

	karo_fdt_move_fdt();

	vm = karo_fdt_set_display(video_mode, "/soc", "/soc/aips/ldb");
	if (vm == NULL) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		return;
	}
	video_mode = vm;
	if (karo_fdt_get_fb_mode(working_fdt, vm, &fb_mode) == 0) {
		p = &fb_mode;
		debug("Using video mode from FDT\n");
		vm += strlen(vm);
		if (fb_mode.xres > panel_info.vl_col ||
			fb_mode.yres > panel_info.vl_row) {
			printf("video resolution from DT: %dx%d exceeds hardware limits: %dx%d\n",
				fb_mode.xres, fb_mode.yres,
				panel_info.vl_col, panel_info.vl_row);
			lcd_enabled = 0;
			return;
		}
	}
	if (p->name != NULL)
		debug("Trying compiled-in video modes\n");
	while (p->name != NULL) {
		if (strcmp(p->name, vm) == 0) {
			debug("Using video mode: '%s'\n", p->name);
			vm += strlen(vm);
			break;
		}
		p++;
	}
	if (*vm != '\0')
		debug("Trying to decode video_mode: '%s'\n", vm);
	while (*vm != '\0') {
		if (*vm >= '0' && *vm <= '9') {
			char *end;

			val = simple_strtoul(vm, &end, 0);
			if (end > vm) {
				if (!xres_set) {
					if (val > panel_info.vl_col)
						val = panel_info.vl_col;
					p->xres = val;
					panel_info.vl_col = val;
					xres_set = 1;
				} else if (!yres_set) {
					if (val > panel_info.vl_row)
						val = panel_info.vl_row;
					p->yres = val;
					panel_info.vl_row = val;
					yres_set = 1;
				} else if (!bpp_set) {
					switch (val) {
					case 32:
					case 24:
						if (pix_fmt == IPU_PIX_FMT_LVDS666)
							pix_fmt = IPU_PIX_FMT_LVDS888;
						/* fallthru */
					case 16:
					case 8:
						color_depth = val;
						break;

					case 18:
						if (pix_fmt == IPU_PIX_FMT_LVDS666) {
							color_depth = val;
							break;
						}
						/* fallthru */
					default:
						printf("Invalid color depth: '%.*s' in video_mode; using default: '%u'\n",
							end - vm, vm, color_depth);
					}
					bpp_set = 1;
				} else if (!refresh_set) {
					refresh = val;
					refresh_set = 1;
				}
			}
			vm = end;
		}
		switch (*vm) {
		case '@':
			bpp_set = 1;
			/* fallthru */
		case '-':
			yres_set = 1;
			/* fallthru */
		case 'x':
			xres_set = 1;
			/* fallthru */
		case 'M':
		case 'R':
			vm++;
			break;

		default:
			if (!pix_fmt) {
				char *tmp;

				if (strncmp(vm, "LVDS", 4) == 0) {
					pix_fmt = IPU_PIX_FMT_LVDS666;
					di_clk_parent = DI_PCLK_LDB;
				} else {
					pix_fmt = IPU_PIX_FMT_RGB24;
				}
				tmp = strchr(vm, ':');
				if (tmp)
					vm = tmp;
			}
			if (*vm != '\0')
				vm++;
		}
	}
	if (p->xres == 0 || p->yres == 0) {
		printf("Invalid video mode: %s\n", getenv("video_mode"));
		lcd_enabled = 0;
		printf("Supported video modes are:");
		for (p = &tx53_fb_modes[0]; p->name != NULL; p++) {
			printf(" %s", p->name);
		}
		printf("\n");
		return;
	}
	if (p->xres > panel_info.vl_col || p->yres > panel_info.vl_row) {
		printf("video resolution: %dx%d exceeds hardware limits: %dx%d\n",
			p->xres, p->yres, panel_info.vl_col, panel_info.vl_row);
		lcd_enabled = 0;
		return;
	}
	panel_info.vl_col = p->xres;
	panel_info.vl_row = p->yres;

	switch (color_depth) {
	case 8:
		panel_info.vl_bpix = LCD_COLOR8;
		break;
	case 16:
		panel_info.vl_bpix = LCD_COLOR16;
		break;
	default:
		panel_info.vl_bpix = LCD_COLOR24;
	}

	p->pixclock = KHZ2PICOS(refresh *
		(p->xres + p->left_margin + p->right_margin + p->hsync_len) *
		(p->yres + p->upper_margin + p->lower_margin + p->vsync_len)
		/ 1000);
	debug("Pixel clock set to %lu.%03lu MHz\n",
		PICOS2KHZ(p->pixclock) / 1000,
		PICOS2KHZ(p->pixclock) % 1000);

	if (p != &fb_mode) {
		int ret;

		debug("Creating new display-timing node from '%s'\n",
			video_mode);
		ret = karo_fdt_create_fb_mode(working_fdt, video_mode, p);
		if (ret)
			printf("Failed to create new display-timing node from '%s': %d\n",
				video_mode, ret);
	}

	gpio_request_array(stk5_lcd_gpios, ARRAY_SIZE(stk5_lcd_gpios));
	imx_iomux_v3_setup_multiple_pads(stk5_lcd_pads,
					ARRAY_SIZE(stk5_lcd_pads));

	debug("Initializing FB driver\n");
	if (!pix_fmt)
		pix_fmt = IPU_PIX_FMT_RGB24;
	else if (pix_fmt == IPU_PIX_FMT_LVDS666) {
		writel(0x01, IOMUXC_BASE_ADDR + 8);
	} else if (pix_fmt == IPU_PIX_FMT_LVDS888) {
		writel(0x21, IOMUXC_BASE_ADDR + 8);
	}
	if (pix_fmt != IPU_PIX_FMT_RGB24) {
		struct mxc_ccm_reg *ccm_regs = (struct mxc_ccm_reg *)CCM_BASE_ADDR;
		/* enable LDB & DI0 clock */
		writel(readl(&ccm_regs->CCGR6) | (3 << 28) | (3 << 10),
			&ccm_regs->CCGR6);
	}

	if (karo_load_splashimage(0) == 0) {
		int ret;

		gd->arch.ipu_hw_rev = IPUV3_HW_REV_IPUV3M;

		debug("Initializing LCD controller\n");
		ret = ipuv3_fb_init(p, 0, pix_fmt, di_clk_parent, di_clk_rate, -1);
		if (ret) {
			printf("Failed to initialize FB driver: %d\n", ret);
			lcd_enabled = 0;
		}
	} else {
		debug("Skipping initialization of LCD controller\n");
	}
}
#else
#define lcd_enabled 0
#endif /* CONFIG_LCD */

static void stk5_board_init(void)
{
	gpio_request_array(stk5_gpios, ARRAY_SIZE(stk5_gpios));
	imx_iomux_v3_setup_multiple_pads(stk5_pads, ARRAY_SIZE(stk5_pads));
}

static void stk5v3_board_init(void)
{
	stk5_board_init();
}

static void stk5v5_board_init(void)
{
	stk5_board_init();

	gpio_request_one(IMX_GPIO_NR(4, 21), GPIOF_OUTPUT_INIT_HIGH,
			"Flexcan Transceiver");
	imx_iomux_v3_setup_pad(MX53_PAD_DISP0_DAT0__GPIO4_21);
}

static void tx53_set_cpu_clock(void)
{
	unsigned long cpu_clk = getenv_ulong("cpu_clk", 10, 0);
	int ret;

	if (had_ctrlc() || (wrsr & WRSR_TOUT))
		return;

	if (cpu_clk == 0 || cpu_clk == mxc_get_clock(MXC_ARM_CLK) / 1000000)
		return;

	ret = mxc_set_clock(CONFIG_SYS_MX5_HCLK, cpu_clk, MXC_ARM_CLK);
	if (ret != 0) {
		printf("Error: Failed to set CPU clock to %lu MHz\n", cpu_clk);
		return;
	}
	printf("CPU clock set to %u.%03u MHz\n",
		mxc_get_clock(MXC_ARM_CLK) / 1000000,
		mxc_get_clock(MXC_ARM_CLK) / 1000 % 1000);
}

static void tx53_init_mac(void)
{
	u8 mac[ETH_ALEN];

	imx_get_mac_from_fuse(0, mac);
	if (!is_valid_ether_addr(mac)) {
		printf("No valid MAC address programmed\n");
		return;
	}

	eth_setenv_enetaddr("ethaddr", mac);
	printf("MAC addr from fuse: %pM\n", mac);
}

int board_late_init(void)
{
	int ret = 0;
	const char *baseboard;

	tx53_set_cpu_clock();
	karo_fdt_move_fdt();

	baseboard = getenv("baseboard");
	if (!baseboard)
		goto exit;

	if (strncmp(baseboard, "stk5", 4) == 0) {
		printf("Baseboard: %s\n", baseboard);
		if ((strlen(baseboard) == 4) ||
			strcmp(baseboard, "stk5-v3") == 0) {
			stk5v3_board_init();
		} else if (strcmp(baseboard, "stk5-v5") == 0) {
			stk5v5_board_init();
		} else {
			printf("WARNING: Unsupported STK5 board rev.: %s\n",
				baseboard + 4);
		}
	} else {
		printf("WARNING: Unsupported baseboard: '%s'\n",
			baseboard);
		ret = -EINVAL;
	}

exit:
	tx53_init_mac();
	gpio_set_value(TX53_RESET_OUT_GPIO, 1);
	clear_ctrlc();
	return ret;
}

int checkboard(void)
{
	tx53_print_cpuinfo();

	printf("Board: Ka-Ro TX53-xx3%s\n",
		TX53_MOD_SUFFIX);

	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
#include <jffs2/jffs2.h>
#include <mtd_node.h>
struct node_info nodes[] = {
	{ "fsl,imx53-nand", MTD_DEV_TYPE_NAND, },
};

#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif

#ifdef CONFIG_SYS_TX53_HWREV_2
void tx53_fixup_rtc(void *blob)
{
	karo_fdt_del_prop(blob, "dallas,ds1339", 0x68, "interrupt-parent");
	karo_fdt_del_prop(blob, "dallas,ds1339", 0x68, "interrupts");
}
#else
static inline void tx53_fixup_rtc(void *blob)
{
}
#endif /* CONFIG_SYS_TX53_HWREV_2 */

void ft_board_setup(void *blob, bd_t *bd)
{
	const char *baseboard = getenv("baseboard");
	int stk5_v5 = baseboard != NULL && (strcmp(baseboard, "stk5-v5") == 0);
	const char *video_mode = getenv("video_mode");

	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
	fdt_fixup_ethernet(blob);

	karo_fdt_fixup_touchpanel(blob);
	karo_fdt_fixup_usb_otg(blob, "fsl,imx-otg", "fsl,usbphy");
	karo_fdt_fixup_flexcan(blob, stk5_v5);
	tx53_fixup_rtc(blob);
	video_mode = karo_fdt_set_display(video_mode, "/soc", "/soc/aips/ldb");
	karo_fdt_update_fb_mode(blob, video_mode);
}
#endif /* CONFIG_OF_BOARD_SETUP */
