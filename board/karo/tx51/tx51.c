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
#include <asm/arch/iomux-mx51.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/sys_proto.h>

#include "../common/karo.h"

#define TX51_FEC_RST_GPIO	IMX_GPIO_NR(2, 14)
#define TX51_FEC_PWR_GPIO	IMX_GPIO_NR(1, 3)
#define TX51_FEC_INT_GPIO	IMX_GPIO_NR(3, 18)
#define TX51_LED_GPIO		IMX_GPIO_NR(4, 10)

#define TX51_LCD_PWR_GPIO	IMX_GPIO_NR(4, 14)
#define TX51_LCD_RST_GPIO	IMX_GPIO_NR(4, 13)
#define TX51_LCD_BACKLIGHT_GPIO	IMX_GPIO_NR(1, 2)

#define TX51_RESET_OUT_GPIO	IMX_GPIO_NR(2, 15)

DECLARE_GLOBAL_DATA_PTR;

#define IOMUX_SION		IOMUX_PAD(0, 0, IOMUX_CONFIG_SION, 0, 0, 0)

#define FEC_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_DVS | PAD_CTL_DSE_HIGH | \
					PAD_CTL_SRE_FAST)
#define FEC_PAD_CTRL2		MUX_PAD_CTRL(PAD_CTL_DVS | PAD_CTL_SRE_FAST)
#define GPIO_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_DVS | PAD_CTL_DSE_HIGH | PAD_CTL_PUS_100K_UP)

static iomux_v3_cfg_t tx51_pads[] = {
	/* NAND flash pads are set up in lowlevel_init.S */

	/* RESET_OUT */
	MX51_PAD_EIM_A21__GPIO2_15 | GPIO_PAD_CTRL,

	/* UART pads */
#if CONFIG_MXC_UART_BASE == UART1_BASE
	MX51_PAD_UART1_RXD__UART1_RXD,
	MX51_PAD_UART1_TXD__UART1_TXD,
	MX51_PAD_UART1_RTS__UART1_RTS,
	MX51_PAD_UART1_CTS__UART1_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART2_BASE
	MX51_PAD_UART2_RXD__UART2_RXD,
	MX51_PAD_UART2_TXD__UART2_TXD,
	MX51_PAD_EIM_D26__UART2_RTS,
	MX51_PAD_EIM_D25__UART2_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART3_BASE
	MX51_PAD_UART3_RXD__UART3_RXD,
	MX51_PAD_UART3_TXD__UART3_TXD,
	MX51_PAD_EIM_D18__UART3_RTS,
	MX51_PAD_EIM_D17__UART3_CTS,
#endif
	/* internal I2C */
	MX51_PAD_I2C1_DAT__GPIO4_17 | IOMUX_SION,
	MX51_PAD_I2C1_CLK__GPIO4_16 | IOMUX_SION,

	/* FEC PHY GPIO functions */
	MX51_PAD_GPIO1_3__GPIO1_3 | GPIO_PAD_CTRL,    /* PHY POWER */
	MX51_PAD_EIM_A20__GPIO2_14 | GPIO_PAD_CTRL,   /* PHY RESET */
	MX51_PAD_NANDF_CS2__GPIO3_18 | GPIO_PAD_CTRL, /* PHY INT */

	/* FEC functions */
	MX51_PAD_NANDF_CS3__FEC_MDC | FEC_PAD_CTRL,
	MX51_PAD_EIM_EB2__FEC_MDIO | FEC_PAD_CTRL,
	MX51_PAD_NANDF_D11__FEC_RX_DV | FEC_PAD_CTRL2,
	MX51_PAD_EIM_CS4__FEC_RX_ER | FEC_PAD_CTRL2,
	MX51_PAD_NANDF_RDY_INT__FEC_TX_CLK | FEC_PAD_CTRL2,
	MX51_PAD_NANDF_CS7__FEC_TX_EN | FEC_PAD_CTRL,
	MX51_PAD_NANDF_D8__FEC_TDATA0 | FEC_PAD_CTRL,
	MX51_PAD_NANDF_CS4__FEC_TDATA1 | FEC_PAD_CTRL,
	MX51_PAD_NANDF_CS5__FEC_TDATA2 | FEC_PAD_CTRL,
	MX51_PAD_NANDF_CS6__FEC_TDATA3 | FEC_PAD_CTRL,

	/* strap pins for PHY configuration */
	MX51_PAD_NANDF_RB3__GPIO3_11 | GPIO_PAD_CTRL, /* RX_CLK/REGOFF */
	MX51_PAD_NANDF_D9__GPIO3_31 | GPIO_PAD_CTRL,  /* RXD0/Mode0 */
	MX51_PAD_EIM_EB3__GPIO2_23 | GPIO_PAD_CTRL,   /* RXD1/Mode1 */
	MX51_PAD_EIM_CS2__GPIO2_27 | GPIO_PAD_CTRL,   /* RXD2/Mode2 */
	MX51_PAD_EIM_CS3__GPIO2_28 | GPIO_PAD_CTRL,   /* RXD3/nINTSEL */
	MX51_PAD_NANDF_RB2__GPIO3_10 | GPIO_PAD_CTRL, /* COL/RMII/CRSDV */
	MX51_PAD_EIM_CS5__GPIO2_30 | GPIO_PAD_CTRL,   /* CRS/PHYAD4 */

	/* unusable pins on TX51 */
	MX51_PAD_GPIO1_0__GPIO1_0,
	MX51_PAD_GPIO1_1__GPIO1_1,
};

static const struct gpio tx51_gpios[] = {
	/* RESET_OUT */
	{ TX51_RESET_OUT_GPIO, GPIOF_OUTPUT_INIT_LOW, "RESET_OUT", },

	/* FEC PHY control GPIOs */
	{ TX51_FEC_PWR_GPIO, GPIOF_OUTPUT_INIT_LOW, "FEC POWER", }, /* PHY POWER */
	{ TX51_FEC_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "FEC RESET", }, /* PHY RESET */
	{ TX51_FEC_INT_GPIO, GPIOF_INPUT, "FEC PHY INT", },	    /* PHY INT (TX_ER) */

	/* FEC PHY strap pins */
	{ IMX_GPIO_NR(3, 11), GPIOF_OUTPUT_INIT_LOW, "FEC PHY REGOFF", },  /* RX_CLK/REGOFF */
	{ IMX_GPIO_NR(3, 31), GPIOF_OUTPUT_INIT_LOW, "FEC PHY MODE0", },   /* RXD0/Mode0 */
	{ IMX_GPIO_NR(2, 23), GPIOF_OUTPUT_INIT_LOW, "FEC PHY MODE1", },   /* RXD1/Mode1 */
	{ IMX_GPIO_NR(2, 27), GPIOF_OUTPUT_INIT_LOW, "FEC PHY MODE2", },   /* RXD2/Mode2 */
	{ IMX_GPIO_NR(2, 28), GPIOF_OUTPUT_INIT_LOW, "FEC PHY nINTSEL", }, /* RXD3/nINTSEL */
	{ IMX_GPIO_NR(3, 10), GPIOF_OUTPUT_INIT_LOW, "FEC PHY RMII", },	   /* COL/RMII/CRSDV */
	{ IMX_GPIO_NR(2, 30), GPIOF_OUTPUT_INIT_LOW, "FEC PHY PHYAD4", },  /* CRS/PHYAD4 */

	/* module internal I2C bus */
	{ IMX_GPIO_NR(4, 17), GPIOF_INPUT, "I2C1 SDA", },
	{ IMX_GPIO_NR(4, 16), GPIOF_INPUT, "I2C1 SCL", },

	/* Unconnected pins */
	{ IMX_GPIO_NR(1, 0), GPIOF_OUTPUT_INIT_LOW, "N/C", },
	{ IMX_GPIO_NR(1, 1), GPIOF_OUTPUT_INIT_LOW, "N/C", },
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

static void tx51_print_cpuinfo(void)
{
	u32 cpurev;

	cpurev = get_cpu_rev();

	printf("CPU:   Freescale i.MX51 rev%d.%d at %d MHz\n",
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		mxc_get_clock(MXC_ARM_CLK) / 1000000);

	print_reset_cause();
}

int board_early_init_f(void)
{
	struct mxc_ccm_reg *ccm_regs = (void *)CCM_BASE_ADDR;

	gpio_request_array(tx51_gpios, ARRAY_SIZE(tx51_gpios));
	imx_iomux_v3_setup_multiple_pads(tx51_pads, ARRAY_SIZE(tx51_pads));

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

	writel(0xffcffffc, &ccm_regs->CCGR0);
	writel(0x003fffff, &ccm_regs->CCGR1);
	writel(0x030c003c, &ccm_regs->CCGR2);
	writel(0x000000ff, &ccm_regs->CCGR3);
	writel(0x00000000, &ccm_regs->CCGR4);
	writel(0x003fc003, &ccm_regs->CCGR5);
	writel(0x00000000, &ccm_regs->CCGR6);
	writel(0x00000000, &ccm_regs->cmeor);
#ifdef CONFIG_CMD_BOOTCE
	/* WinCE fails to enable these clocks */
	writel(readl(&ccm_regs->CCGR2) | 0x0c000000, &ccm_regs->CCGR2); /* usboh3_ipg_ahb */
	writel(readl(&ccm_regs->CCGR4) | 0x30000000, &ccm_regs->CCGR4); /* srtc */
	writel(readl(&ccm_regs->CCGR6) | 0x00000300, &ccm_regs->CCGR6); /* emi_garb */
#endif
	return 0;
}

int board_init(void)
{
	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;

	if (ctrlc() || (wrsr & WRSR_TOUT)) {
		printf("CTRL-C detected; Skipping boot critical setup\n");
		return 1;
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
	MX51_PAD_SD1_CMD__SD1_CMD,
	MX51_PAD_SD1_CLK__SD1_CLK,
	MX51_PAD_SD1_DATA0__SD1_DATA0,
	MX51_PAD_SD1_DATA1__SD1_DATA1,
	MX51_PAD_SD1_DATA2__SD1_DATA2,
	MX51_PAD_SD1_DATA3__SD1_DATA3,
	/* SD1 CD */
	MX51_PAD_DISPB2_SER_RS__GPIO3_8 | GPIO_PAD_CTRL,
};

static const iomux_v3_cfg_t mmc1_pads[] = {
	MX51_PAD_SD2_CMD__SD2_CMD,
	MX51_PAD_SD2_CLK__SD2_CLK,
	MX51_PAD_SD2_DATA0__SD2_DATA0,
	MX51_PAD_SD2_DATA1__SD2_DATA1,
	MX51_PAD_SD2_DATA2__SD2_DATA2,
	MX51_PAD_SD2_DATA3__SD2_DATA3,
	/* SD2 CD */
	MX51_PAD_DISPB2_SER_DIO__GPIO3_6 | GPIO_PAD_CTRL,
};

static struct tx51_esdhc_cfg {
	const iomux_v3_cfg_t *pads;
	int num_pads;
	struct fsl_esdhc_cfg cfg;
	int cd_gpio;
} tx51_esdhc_cfg[] = {
	{
		.pads = mmc0_pads,
		.num_pads = ARRAY_SIZE(mmc0_pads),
		.cfg = {
			.esdhc_base = (void __iomem *)MMC_SDHC1_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = IMX_GPIO_NR(3, 8),
	},
	{
		.pads = mmc1_pads,
		.num_pads = ARRAY_SIZE(mmc1_pads),
		.cfg = {
			.esdhc_base = (void __iomem *)MMC_SDHC2_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = IMX_GPIO_NR(3, 6),
	},
};

static inline struct tx51_esdhc_cfg *to_tx51_esdhc_cfg(struct fsl_esdhc_cfg *cfg)
{
	return container_of(cfg, struct tx51_esdhc_cfg, cfg);
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct tx51_esdhc_cfg *cfg = to_tx51_esdhc_cfg(mmc->priv);

	if (cfg->cd_gpio < 0)
		return cfg->cd_gpio;

	debug("SD card %d is %spresent\n",
		cfg - tx51_esdhc_cfg,
		gpio_get_value(cfg->cd_gpio) ? "NOT " : "");
	return !gpio_get_value(cfg->cd_gpio);
}

int board_mmc_init(bd_t *bis)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tx51_esdhc_cfg); i++) {
		struct mmc *mmc;
		struct tx51_esdhc_cfg *cfg = &tx51_esdhc_cfg[i];
		int ret;

		if (i >= CONFIG_SYS_FSL_ESDHC_NUM)
			break;

		imx_iomux_v3_setup_multiple_pads(cfg->pads,
						cfg->num_pads);
		cfg->cfg.sdhc_clk = mxc_get_clock(MXC_ESDHC_CLK);

		ret = gpio_request_one(cfg->cd_gpio,
				GPIOF_INPUT, "MMC CD");
		if (ret) {
			printf("Error %d requesting GPIO%d_%d\n",
				ret, cfg->cd_gpio / 32, cfg->cd_gpio % 32);
			continue;
		}

		debug("%s: Initializing MMC slot %d\n", __func__, i);
		fsl_esdhc_initialize(bis, &cfg->cfg);

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
		mac[ETH_ALEN - i - 1] = readl(&fuse->mac_addr[i]);
}

static iomux_v3_cfg_t tx51_fec_pads[] = {
	/* reconfigure strap pins for FEC function */
	MX51_PAD_NANDF_RB3__FEC_RX_CLK | FEC_PAD_CTRL2,
	MX51_PAD_NANDF_D9__FEC_RDATA0 | FEC_PAD_CTRL2,
	MX51_PAD_EIM_EB3__FEC_RDATA1 | FEC_PAD_CTRL2,
	MX51_PAD_EIM_CS2__FEC_RDATA2 | FEC_PAD_CTRL2,
	MX51_PAD_EIM_CS3__FEC_RDATA3 | FEC_PAD_CTRL2,
	MX51_PAD_NANDF_RB2__FEC_COL | FEC_PAD_CTRL2,
	MX51_PAD_EIM_CS5__FEC_CRS | FEC_PAD_CTRL,
};

/* take bit 4 of PHY address from configured PHY address or
 * set it to 0 if PHYADDR is -1 (probe for PHY)
 */
#define PHYAD4 ((CONFIG_FEC_MXC_PHYADDR >> 4) & !(CONFIG_FEC_MXC_PHYADDR >> 5))

static struct gpio tx51_fec_gpios[] = {
	{ TX51_FEC_PWR_GPIO, GPIOF_OUTPUT_INIT_HIGH, "FEC PHY POWER", },
	{ IMX_GPIO_NR(3, 31), GPIOF_OUTPUT_INIT_HIGH, "FEC PHY Mode0", },	/* RXD0/Mode0 */
	{ IMX_GPIO_NR(2, 23), GPIOF_OUTPUT_INIT_HIGH, "FEC PHY Mode1", },	/* RXD1/Mode1 */
	{ IMX_GPIO_NR(2, 27), GPIOF_OUTPUT_INIT_HIGH, "FEC PHY Mode2", },	/* RXD2/Mode2 */
	{ IMX_GPIO_NR(2, 28), GPIOF_OUTPUT_INIT_HIGH, "FEC PHY nINTSEL", },	/* RXD3/nINTSEL */
#if PHYAD4
	{ IMX_GPIO_NR(2, 30), GPIOF_OUTPUT_INIT_HIGH, "FEC PHY PHYAD4", }, /* CRS/PHYAD4 */
#else
	{ IMX_GPIO_NR(2, 30), GPIOF_OUTPUT_INIT_LOW, "FEC PHY PHYAD4", }, /* CRS/PHYAD4 */
#endif
};

int board_eth_init(bd_t *bis)
{
	int ret;

	/* Power up the external phy and assert strap options */
	gpio_request_array(tx51_fec_gpios, ARRAY_SIZE(tx51_fec_gpios));

	/* delay at least 21ms for the PHY internal POR signal to deassert */
	udelay(22000);

	/* Deassert RESET to the external phy */
	gpio_set_value(TX51_FEC_RST_GPIO, 1);

	/* Without this delay the PHY won't work, though nothing in
	 * the datasheets suggests that it should be necessary!
	 */
	udelay(400);
	imx_iomux_v3_setup_multiple_pads(tx51_fec_pads,
					ARRAY_SIZE(tx51_fec_pads));

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
		gpio_set_value(TX51_LED_GPIO, 1);
		led_state = LED_STATE_ON;
	} else {
		if (get_timer(last) > CONFIG_SYS_HZ) {
			last = get_timer(0);
			if (led_state == LED_STATE_ON) {
				gpio_set_value(TX51_LED_GPIO, 0);
			} else {
				gpio_set_value(TX51_LED_GPIO, 1);
			}
			led_state = 1 - led_state;
		}
	}
}

static const iomux_v3_cfg_t stk5_pads[] = {
	/* SW controlled LED on STK5 baseboard */
	MX51_PAD_CSI2_D13__GPIO4_10 | GPIO_PAD_CTRL,

	/* USB PHY reset */
	MX51_PAD_GPIO1_4__GPIO1_4 | GPIO_PAD_CTRL,
	/* USBOTG OC */
	MX51_PAD_GPIO1_6__GPIO1_6 | GPIO_PAD_CTRL,
	/* USB PHY clock enable */
	MX51_PAD_GPIO1_7__GPIO1_7 | GPIO_PAD_CTRL,
	/* USBH1 VBUS enable */
	MX51_PAD_GPIO1_8__GPIO1_8 | GPIO_PAD_CTRL,
	/* USBH1 OC */
	MX51_PAD_GPIO1_9__GPIO1_9 | GPIO_PAD_CTRL,
};

static const struct gpio stk5_gpios[] = {
	{ TX51_LED_GPIO, GPIOF_OUTPUT_INIT_LOW, "HEARTBEAT LED", },

	{ IMX_GPIO_NR(1, 4), GPIOF_OUTPUT_INIT_LOW, "ULPI PHY clk enable", },
	{ IMX_GPIO_NR(1, 6), GPIOF_INPUT, "USBOTG OC", },
	{ IMX_GPIO_NR(1, 7), GPIOF_OUTPUT_INIT_LOW, "ULPI PHY reset", },
	{ IMX_GPIO_NR(1, 8), GPIOF_OUTPUT_INIT_LOW, "USBH1 VBUS enable", },
	{ IMX_GPIO_NR(1, 9), GPIOF_INPUT, "USBH1 OC", },
};

#ifdef CONFIG_LCD
static u16 tx51_cmap[256];
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 1600,
	.vl_row = 1200,

	.vl_bpix = LCD_COLOR24,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
	.cmap = tx51_cmap,
};

static struct fb_videomode tx51_fb_modes[] = {
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
		gpio_set_value(TX51_LCD_PWR_GPIO, 1);
		udelay(100);
		gpio_set_value(TX51_LCD_RST_GPIO, 1);
		udelay(300000);
		gpio_set_value(TX51_LCD_BACKLIGHT_GPIO, 0);
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
		gpio_set_value(TX51_LCD_BACKLIGHT_GPIO, 1);
		gpio_set_value(TX51_LCD_RST_GPIO, 0);
		gpio_set_value(TX51_LCD_PWR_GPIO, 0);
	}
}

static const iomux_v3_cfg_t stk5_lcd_pads[] = {
	/* LCD RESET */
	MX51_PAD_CSI2_VSYNC__GPIO4_13,
	/* LCD POWER_ENABLE */
	MX51_PAD_CSI2_HSYNC__GPIO4_14,
	/* LCD Backlight (PWM) */
	MX51_PAD_GPIO1_2__GPIO1_2,

	/* Display */
	MX51_PAD_DISP1_DAT0__DISP1_DAT0,
	MX51_PAD_DISP1_DAT1__DISP1_DAT1,
	MX51_PAD_DISP1_DAT2__DISP1_DAT2,
	MX51_PAD_DISP1_DAT3__DISP1_DAT3,
	MX51_PAD_DISP1_DAT4__DISP1_DAT4,
	MX51_PAD_DISP1_DAT5__DISP1_DAT5,
	MX51_PAD_DISP1_DAT6__DISP1_DAT6,
	MX51_PAD_DISP1_DAT7__DISP1_DAT7,
	MX51_PAD_DISP1_DAT8__DISP1_DAT8,
	MX51_PAD_DISP1_DAT9__DISP1_DAT9,
	MX51_PAD_DISP1_DAT10__DISP1_DAT10,
	MX51_PAD_DISP1_DAT11__DISP1_DAT11,
	MX51_PAD_DISP1_DAT12__DISP1_DAT12,
	MX51_PAD_DISP1_DAT13__DISP1_DAT13,
	MX51_PAD_DISP1_DAT14__DISP1_DAT14,
	MX51_PAD_DISP1_DAT15__DISP1_DAT15,
	MX51_PAD_DISP1_DAT16__DISP1_DAT16,
	MX51_PAD_DISP1_DAT17__DISP1_DAT17,
	MX51_PAD_DISP1_DAT18__DISP1_DAT18,
	MX51_PAD_DISP1_DAT19__DISP1_DAT19,
	MX51_PAD_DISP1_DAT20__DISP1_DAT20,
	MX51_PAD_DISP1_DAT21__DISP1_DAT21,
	MX51_PAD_DISP1_DAT22__DISP1_DAT22,
	MX51_PAD_DISP1_DAT23__DISP1_DAT23,
	MX51_PAD_DI1_PIN2__DI1_PIN2, /* HSYNC */
	MX51_PAD_DI1_PIN3__DI1_PIN3, /* VSYNC */
};

static const struct gpio stk5_lcd_gpios[] = {
	{ TX51_LCD_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD RESET", },
	{ TX51_LCD_PWR_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD POWER", },
	{ TX51_LCD_BACKLIGHT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

void lcd_ctrl_init(void *lcdbase)
{
	int color_depth = 24;
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	const char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p = &tx51_fb_modes[0];
	struct fb_videomode fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;
	int pix_fmt;
	int lcd_bus_width;
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

	if (video_mode == NULL) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		return;
	}
	vm = video_mode;
	if (karo_fdt_get_fb_mode(working_fdt, video_mode, &fb_mode) == 0) {
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
					case 8:
					case 16:
					case 24:
					case 32:
						color_depth = val;
						break;

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
			if (*vm != '\0')
				vm++;
		}
	}
	if (p->xres == 0 || p->yres == 0) {
		printf("Invalid video mode: %s\n", getenv("video_mode"));
		lcd_enabled = 0;
		printf("Supported video modes are:");
		for (p = &tx51_fb_modes[0]; p->name != NULL; p++) {
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
		(p->yres + p->upper_margin + p->lower_margin + p->vsync_len) /
				1000);
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

	lcd_bus_width = karo_fdt_get_lcd_bus_width(working_fdt, 24);
	switch (lcd_bus_width) {
	case 24:
		pix_fmt = IPU_PIX_FMT_RGB24;
		break;

	case 18:
		pix_fmt = IPU_PIX_FMT_RGB666;
		break;

	case 16:
		pix_fmt = IPU_PIX_FMT_RGB565;
		break;

	default:
		lcd_enabled = 0;
		printf("Invalid LCD bus width: %d\n", lcd_bus_width);
		return;
	}
	if (karo_load_splashimage(0) == 0) {
		int ret;
		struct mxc_ccm_reg *ccm_regs = (struct mxc_ccm_reg *)MXC_CCM_BASE;
		u32 ccgr4 = readl(&ccm_regs->CCGR4);

		/* MIPI HSC clock is required for initialization */
		writel(ccgr4 | (3 << 12), &ccm_regs->CCGR4);

		gd->arch.ipu_hw_rev = IPUV3_HW_REV_IPUV3DEX;

		debug("Initializing LCD controller\n");
		ret = ipuv3_fb_init(p, 0, pix_fmt, di_clk_parent, di_clk_rate, -1);
		writel(ccgr4 & ~(3 << 12), &ccm_regs->CCGR4);
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

static void tx51_set_cpu_clock(void)
{
	unsigned long cpu_clk = getenv_ulong("cpu_clk", 10, 0);
	int ret;

	if (had_ctrlc() || (wrsr & WRSR_TOUT))
		return;

	if (cpu_clk == 0 || cpu_clk == mxc_get_clock(MXC_ARM_CLK) / 1000000)
		return;

	if (mxc_set_clock(CONFIG_SYS_MX5_HCLK, cpu_clk, MXC_ARM_CLK) == 0) {
		cpu_clk = mxc_get_clock(MXC_ARM_CLK);
		printf("CPU clock set to %lu.%03lu MHz\n",
			cpu_clk / 1000000, cpu_clk / 1000 % 1000);
	} else {
		printf("Error: Failed to set CPU clock to %lu MHz\n", cpu_clk);
	}
}

static void tx51_init_mac(void)
{
	u8 mac[ETH_ALEN];

	imx_get_mac_from_fuse(0, mac);
	if (!is_valid_ether_addr(mac)) {
		printf("No valid MAC address programmed\n");
		return;
	}

	printf("MAC addr from fuse: %pM\n", mac);
	eth_setenv_enetaddr("ethaddr", mac);
}

int board_late_init(void)
{
	int ret = 0;
	const char *baseboard;

	tx51_set_cpu_clock();
	karo_fdt_move_fdt();

	baseboard = getenv("baseboard");
	if (!baseboard)
		goto exit;

	printf("Baseboard: %s\n", baseboard);

	if (strncmp(baseboard, "stk5", 4) == 0) {
		if ((strlen(baseboard) == 4) ||
			strcmp(baseboard, "stk5-v3") == 0) {
			stk5v3_board_init();
		} else if (strcmp(baseboard, "stk5-v5") == 0) {
			printf("ERROR: Baseboard '%s' incompatible with TX51 module!\n",
				baseboard);
			stk5v3_board_init();
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
	tx51_init_mac();

	gpio_set_value(TX51_RESET_OUT_GPIO, 1);
	clear_ctrlc();
	return ret;
}

int checkboard(void)
{
	tx51_print_cpuinfo();
#if CONFIG_NR_DRAM_BANKS > 1
	printf("Board: Ka-Ro TX51-8xx1 | TX51-8xx2\n");
#else
	printf("Board: Ka-Ro TX51-8xx0\n");
#endif
	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
#include <jffs2/jffs2.h>
#include <mtd_node.h>
static struct node_info nodes[] = {
	{ "fsl,imx51-nand", MTD_DEV_TYPE_NAND, },
};
#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif

static const char *tx51_touchpanels[] = {
	"ti,tsc2007",
	"edt,edt-ft5x06",
};

void ft_board_setup(void *blob, bd_t *bd)
{
	const char *video_mode = karo_get_vmode(getenv("video_mode"));

	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
	fdt_fixup_ethernet(blob);

	karo_fdt_fixup_touchpanel(blob, tx51_touchpanels,
				ARRAY_SIZE(tx51_touchpanels));
	karo_fdt_fixup_usb_otg(blob, "usbotg", "fsl,usbphy");
	karo_fdt_update_fb_mode(blob, video_mode);
}
#endif /* CONFIG_OF_BOARD_SETUP */
