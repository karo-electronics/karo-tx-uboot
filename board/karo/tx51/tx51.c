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
#include <libfdt.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <netdev.h>
#include <fdt_support.h>
#include <asm/string.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/iomux-mx51.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/sys_proto.h>

#if !defined(CONFIG_TX51_80x0) && !defined(CONFIG_TX51_80x1) && !defined(CONFIG_TX51_80x2)
#error TX51 model not selected
#endif

//#define TIMER_TEST

#define IMX_GPIO_NR(p, o)	((((p) - 1) << 5) | (o))

#define TX51_FEC_RESET_GPIO	IMX_GPIO_NR(2, 14)
#define TX51_FEC_POWER_GPIO	IMX_GPIO_NR(1, 3)
#define TX51_LED_GPIO		IMX_GPIO_NR(4, 10)

DECLARE_GLOBAL_DATA_PTR;

/*
 * Functions
 */
static void print_reset_cause(void)
{
	u32 srsr;
	struct src *src_regs = (struct src *)SRC_BASE_ADDR;
	void __iomem *wdt_base = (void __iomem *)WDOG1_BASE_ADDR;
	char *dlm = "";

	printf("Reset cause: ");

	srsr = readl(&src_regs->srsr);
	if (srsr & 0x00001) {
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
		u32 wrsr = readw(wdt_base + 4);
		if (wrsr & (1 << 0)) {
			printf("%sSOFT", dlm);
			dlm = " | ";
		}
		if (wrsr & (1 << 1)) {
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

static void print_cpuinfo(void)
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
#ifdef CONFIG_USB_EHCI_MX5
	setup_usb_h1();
#endif
	writel(0xffffffff, 0x73fd4068);
	writel(0xffffffff, 0x73fd406c);
	writel(0xffffffff, 0x73fd4070);
	writel(0xffffffff, 0x73fd4074);
	writel(0xffffffff, 0x73fd4078);
	writel(0xffffffff, 0x73fd407c);
	writel(0xffffffff, 0x73fd4080);
	writel(0xffffffff, 0x73fd4084);
	return 0;
}

void coloured_LED_init(void)
{
	/* Switch LED off */
	gpio_set_value(TX51_LED_GPIO, 0);
}

int board_init(void)
{
	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;
	return 0;
}

int dram_init(void)
{
	int ret;

	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				PHYS_SDRAM_1_SIZE);

	ret = mxc_set_clock(CONFIG_SYS_MX5_HCLK,
		CONFIG_SYS_SDRAM_CLOCK, MXC_DDR_CLK);
	if (ret)
		printf("%s: Failed to set DDR clock to %uMHz: %d\n", __func__,
			CONFIG_SYS_SDRAM_CLOCK, ret);
	else
		debug("%s: DDR clock set to %u.%03uMHz (desig.: %u.000MHz)\n",
			__func__, mxc_get_clock(MXC_DDR_CLK) / 1000000,
			mxc_get_clock(MXC_DDR_CLK) / 1000 % 1000,
			CONFIG_SYS_SDRAM_CLOCK);
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
int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = mmc->priv;

	if (cfg->cd_gpio < 0)
		return cfg->cd_gpio;

	return !gpio_get_value(cfg->cd_gpio);
}

static struct fsl_esdhc_cfg esdhc_cfg[] = {
	{
		.esdhc_base = (void __iomem *)MMC_SDHC1_BASE_ADDR,
		.no_snoop = 1,
		.cd_gpio = IMX_GPIO_NR(3, 8),
		.wp_gpio = -EINVAL,
	},
	{
		.esdhc_base = (void __iomem *)MMC_SDHC2_BASE_ADDR,
		.no_snoop = 1,
		.cd_gpio = IMX_GPIO_NR(3, 6),
		.wp_gpio = -EINVAL,
	},
};

static const iomux_v3_cfg_t mmc0_pads[] = {
	MX51_PAD_SD1_CMD__SD1_CMD,
	MX51_PAD_SD1_CLK__SD1_CLK,
	MX51_PAD_SD1_DATA0__SD1_DATA0,
	MX51_PAD_SD1_DATA1__SD1_DATA1,
	MX51_PAD_SD1_DATA2__SD1_DATA2,
	MX51_PAD_SD1_DATA3__SD1_DATA3,
	/* SD1 CD */
	MX51_PAD_DISPB2_SER_RS__GPIO3_8 |
	MUX_PAD_CTRL(PAD_CTL_PUE | PAD_CTL_PKE),
};

static const iomux_v3_cfg_t mmc1_pads[] = {
	MX51_PAD_SD2_CMD__SD2_CMD,
	MX51_PAD_SD2_CLK__SD2_CLK,
	MX51_PAD_SD2_DATA0__SD2_DATA0,
	MX51_PAD_SD2_DATA1__SD2_DATA1,
	MX51_PAD_SD2_DATA2__SD2_DATA2,
	MX51_PAD_SD2_DATA3__SD2_DATA3,
	/* SD2 CD */
	MX51_PAD_DISPB2_SER_DIO__GPIO3_6 |
	MUX_PAD_CTRL(PAD_CTL_PUE | PAD_CTL_PKE),
};

static struct {
	const iomux_v3_cfg_t *pads;
	int count;
} mmc_pad_config[] = {
	{ mmc0_pads, ARRAY_SIZE(mmc0_pads), },
	{ mmc1_pads, ARRAY_SIZE(mmc1_pads), },
};

int board_mmc_init(bd_t *bis)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(esdhc_cfg); i++) {
		struct mmc *mmc;

		if (i >= CONFIG_SYS_FSL_ESDHC_NUM)
			break;
		mxc_iomux_v3_setup_multiple_pads(mmc_pad_config[i].pads,
						mmc_pad_config[i].count);
		fsl_esdhc_initialize(bis, &esdhc_cfg[i]);
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

#define IOMUX_SION		IOMUX_PAD(0, 0, IOMUX_CONFIG_SION, 0, 0, 0)

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

#define FEC_PAD_CTL	(PAD_CTL_DVS | PAD_CTL_PKE | PAD_CTL_DSE_HIGH | \
			PAD_CTL_SRE_FAST)
#define FEC_PAD_CTL2	(PAD_CTL_DVS | PAD_CTL_PKE | PAD_CTL_PUE |	\
			PAD_CTL_PUS_100K_UP | PAD_CTL_SRE_FAST)
#define GPIO_PAD_CTL	(PAD_CTL_DVS | PAD_CTL_DSE_HIGH)

static iomux_v3_cfg_t tx51_fec_gpio_pads[] = {
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS3__GPIO3_19, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_EIM_EB2__GPIO2_22, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_RB3__GPIO3_11, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_D11__GPIO3_29, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_D9__GPIO3_31, GPIO_PAD_CTL) | IOMUX_SION, /* RXD0/Mode0 */
	NEW_PAD_CTRL(MX51_PAD_EIM_EB3__GPIO2_23, GPIO_PAD_CTL) | IOMUX_SION, /* RXD1/Mode1 */
	NEW_PAD_CTRL(MX51_PAD_EIM_CS2__GPIO2_27, GPIO_PAD_CTL) | IOMUX_SION, /* RXD2/Mode2 */
	NEW_PAD_CTRL(MX51_PAD_EIM_CS3__GPIO2_28, GPIO_PAD_CTL) | IOMUX_SION, /* RXD3/nINTSEL */
	NEW_PAD_CTRL(MX51_PAD_EIM_CS4__GPIO2_29, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_RDY_INT__GPIO3_24, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS7__GPIO3_23, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_D8__GPIO4_0, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS4__GPIO3_20, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS5__GPIO3_21, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS6__GPIO3_22, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_RB2__GPIO3_10, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_EIM_CS5__GPIO2_30, GPIO_PAD_CTL) | IOMUX_SION,
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS2__GPIO3_18, GPIO_PAD_CTL) | IOMUX_SION, /* PHY INT */
	NEW_PAD_CTRL(MX51_PAD_EIM_A20__GPIO2_14, GPIO_PAD_CTL) | IOMUX_SION, /* PHY RESET */
	NEW_PAD_CTRL(MX51_PAD_GPIO1_3__GPIO1_3, GPIO_PAD_CTL) | IOMUX_SION,  /* PHY POWER */
};

static iomux_v3_cfg_t tx51_fec_pads[] = {
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS3__FEC_MDC, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_EIM_EB2__FEC_MDIO, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_NANDF_RB3__FEC_RX_CLK, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_NANDF_D11__FEC_RX_DV, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_NANDF_D9__FEC_RDATA0, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_EIM_EB3__FEC_RDATA1, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_EIM_CS2__FEC_RDATA2, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_EIM_CS3__FEC_RDATA3, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_EIM_CS4__FEC_RX_ER, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_NANDF_RDY_INT__FEC_TX_CLK, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS7__FEC_TX_EN, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_NANDF_D8__FEC_TDATA0, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS4__FEC_TDATA1, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS5__FEC_TDATA2, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS6__FEC_TDATA3, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_NANDF_RB2__FEC_COL, FEC_PAD_CTL2),
	NEW_PAD_CTRL(MX51_PAD_EIM_CS5__FEC_CRS, FEC_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_NANDF_CS2__GPIO3_18, GPIO_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_EIM_A20__GPIO2_14, GPIO_PAD_CTL),
	NEW_PAD_CTRL(MX51_PAD_GPIO1_3__GPIO1_3, GPIO_PAD_CTL),
};

static struct {
	unsigned gpio:8,
		dir:1,
		val:1;
} tx51_fec_gpios[] = {
	{ IMX_GPIO_NR(1, 3), 1, 0, },	/* PHY power */
	{ IMX_GPIO_NR(2, 14), 1, 0, },	/* PHY reset */
	{ IMX_GPIO_NR(3, 19), 1, 0, },	/* MDC */
	{ IMX_GPIO_NR(2, 22), 1, 0, },	/* MDIO */
	{ IMX_GPIO_NR(3, 11), 0, 1, },	/* RX_CLK */
	{ IMX_GPIO_NR(3, 29), 0, 0, },	/* RX_DV */
	{ IMX_GPIO_NR(3, 31), 1, 1, },	/* RXD0/Mode0 */
	{ IMX_GPIO_NR(2, 23), 1, 1, },	/* RXD1/Mode1 */
	{ IMX_GPIO_NR(2, 27), 1, 1, },	/* RXD2/Mode2 */
	{ IMX_GPIO_NR(2, 28), 1, 1, },	/* RXD3/nINTSEL */
	{ IMX_GPIO_NR(2, 29), 0, 0, },	/* RX_ER */
	{ IMX_GPIO_NR(3, 24), 0, 0, },	/* TX_CLK */
	{ IMX_GPIO_NR(3, 23), 1, 0, },	/* TX_EN */
	{ IMX_GPIO_NR(4, 0), 1, 0, },	/* TXD0 */
	{ IMX_GPIO_NR(3, 20), 1, 0, },	/* TXD1 */
	{ IMX_GPIO_NR(3, 21), 1, 0, },	/* TXD2 */
	{ IMX_GPIO_NR(3, 22), 1, 0, },	/* TXD3 */
	{ IMX_GPIO_NR(3, 10), 1, 0, },	/* COL */
	{ IMX_GPIO_NR(2, 30), 1, (CONFIG_FEC_MXC_PHYADDR >> 4) & 1, }, /* PHYAD4 */
	{ IMX_GPIO_NR(3, 18), 0, 1, },	/* PHY INT (TX_ER) */
};

int board_eth_init(bd_t *bis)
{
	int ret;
	unsigned char mac[ETH_ALEN];
	char mac_str[ETH_ALEN * 3] = "";
	int i;

	for (i = 0; i < ARRAY_SIZE(tx51_fec_gpios); i++) {
		int gpio = tx51_fec_gpios[i].gpio;

		debug("Setting GPIO%d_%d as output LOW\n",
			gpio / 32 + 1, gpio % 32);
		gpio_direction_output(gpio, 0);
	}
	mxc_iomux_v3_setup_multiple_pads(tx51_fec_gpio_pads,
					ARRAY_SIZE(tx51_fec_gpio_pads));

	/* Power on the external phy */
	gpio_direction_output(TX51_FEC_POWER_GPIO, 1);

	udelay(1000);

	for (i = 0; i < ARRAY_SIZE(tx51_fec_gpios); i++) {
		int gpio = tx51_fec_gpios[i].gpio;
		int dir = tx51_fec_gpios[i].dir;
		int val = tx51_fec_gpios[i].val;

		if (dir && val) {
			gpio_direction_output(gpio, val);
		} else if (!dir) {
			gpio_direction_input(gpio);
		}
	}

	udelay(25000);

	/* Deassert RESET to the external phy */
	gpio_set_value(TX51_FEC_RESET_GPIO, 1);

	udelay(400);

	mxc_iomux_v3_setup_multiple_pads(tx51_fec_pads,
					ARRAY_SIZE(tx51_fec_pads));

	ret = cpu_eth_init(bis);
	if (ret) {
		printf("cpu_eth_init() failed: %d\n", ret);
	}

	imx_get_mac_from_fuse(0, mac);
	snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	setenv("ethaddr", mac_str);

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
	/* heartbeat LED */
	MX51_PAD_CSI2_D13__GPIO4_10,
	/* LCD RESET */
	MX51_PAD_CSI2_VSYNC__GPIO4_13,
	/* LCD POWER_ENABLE */
	MX51_PAD_CSI2_HSYNC__GPIO4_14,
	/* LCD Backlight (PWM) */
	MX51_PAD_GPIO1_2__GPIO1_2,
};

enum gpio_flags {
	GPIOF_INPUT,
	GPIOF_OUTPUT_INIT_LOW,
	GPIOF_OUTPUT_INIT_HIGH,
};

struct gpio {
	unsigned int gpio;
	enum gpio_flags flags;
	const char *label;
};

static const struct gpio stk5_gpios[] = {
	{ IMX_GPIO_NR(4, 10), GPIOF_OUTPUT_INIT_LOW, "HEARTBEAT LED", },
	{ IMX_GPIO_NR(4, 13), GPIOF_OUTPUT_INIT_LOW, "LCD RESET", },
	{ IMX_GPIO_NR(4, 14), GPIOF_OUTPUT_INIT_LOW, "LCD POWER", },
	{ IMX_GPIO_NR(1, 2), GPIOF_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

static int gpio_request_array(const struct gpio *gp, int count)
{
	int ret;
	int i;

	for (i = 0; i < count; i++) {
		ret = gpio_request(gp[i].gpio, gp[i].label);
		if (ret)
			goto error;

		if (gp[i].flags == GPIOF_INPUT)
			gpio_direction_input(gp[i].gpio);
		else if (gp[i].flags == GPIOF_OUTPUT_INIT_LOW)
			gpio_direction_output(gp[i].gpio, 0);
		else if (gp[i].flags == GPIOF_OUTPUT_INIT_HIGH)
			gpio_direction_output(gp[i].gpio, 1);
	}
	return 0;

error:
	while (--i >= 0)
		gpio_free(gp[i].gpio);

	return ret;
}

static void stk5_board_init(void)
{
	mxc_iomux_v3_setup_multiple_pads(stk5_pads, ARRAY_SIZE(stk5_pads));
	gpio_request_array(stk5_gpios, ARRAY_SIZE(stk5_gpios));
}

static void stk5v3_board_init(void)
{
	stk5_board_init();
}

static void stk5v5_board_init(void)
{
	stk5_board_init();
}

static void tx51_move_fdt(void)
{
	unsigned long fdt_addr = getenv_ulong("fdtcontroladdr", 16, 0);
	void *fdt = NULL;

#ifdef CONFIG_OF_EMBED
	fdt = _binary_dt_dtb_start;
#elif defined CONFIG_OF_SEPARATE
	fdt = (void *)(_end_ofs + _TEXT_BASE);
#endif
	if (fdt && fdt_addr != 0) {
		if (fdt_check_header(fdt) == 0) {
			size_t fdt_len = fdt_totalsize(fdt);

			memmove((void *)fdt_addr, fdt, fdt_len);
		} else {
			printf("ERROR: No valid FDT found at %p\n", fdt);
		}
	}
}

int board_late_init(void)
{
	const char *baseboard;

	baseboard = getenv("baseboard");
	if (!baseboard)
		return 0;

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
		return -EINVAL;
	}
	return 0;
}

int checkboard(void)
{
	print_cpuinfo();

	printf("Board: Ka-Ro TX51\n");

	tx51_move_fdt();

#ifdef TIMER_TEST
	{
		unsigned long start = get_timer(0);
		unsigned long last = gd->tbl;
		unsigned long loop = 0;
		unsigned long cnt = 0;
		while (!tstc()) {
			unsigned long elapsed = get_timer(start);
			unsigned long diff = gd->tbl - last;

			loop++;
			last = gd->tbl;

			printf("loop %4lu: t=%08lx diff=%08lx steps=%5lu elapsed time: %lu",
				loop, gd->tbl, diff, cnt, elapsed / CONFIG_SYS_HZ);
			cnt = 0;
			while (get_timer(start) < loop * CONFIG_SYS_HZ) {
				cnt++;
				udelay(100);
			}
			printf("\n");
		}
	}
#endif
	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
#include <jffs2/jffs2.h>
#include <mtd_node.h>
struct node_info nodes[] = {
	{ "fsl,imx51-nand", MTD_DEV_TYPE_NAND, },
};

#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif

static const char *tx51_touchpanels[] = {
	"ti,tsc2007",
	"edt,edt-ft5x06",
};

static void tx51_fixup_touchpanel(void *blob)
{
	int i;
	const char *model = getenv("touchpanel");

	for (i = 0; i < ARRAY_SIZE(tx51_touchpanels); i++) {
		int offs;
		const char *tp = tx51_touchpanels[i];

		if (model != NULL && strcmp(model, tp) == 0)
			continue;

		tp = strchr(tp, ',');
		if (tp != NULL && *tp != '\0' && strcmp(model, tp + 1) == 0)
			continue;

		offs = fdt_node_offset_by_compatible(blob, -1,
						tx51_touchpanels[i]);
		if (offs < 0) {
			printf("node '%s' not found: %d\n",
				tx51_touchpanels[i], offs);
			continue;
		}
		printf("Removing node '%s' at offset %d\n",
			tx51_touchpanels[i], offs);
		fdt_del_node(blob, offs);
	}
}

void ft_board_setup(void *blob, bd_t *bd)
{
	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
	fdt_fixup_ethernet(blob);

	tx51_fixup_touchpanel(blob);
}
#endif
