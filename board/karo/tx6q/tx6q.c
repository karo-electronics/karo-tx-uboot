/*
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
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
//#define DEBUG
//#define TIMER_TEST

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
#include <mx2fb.h>
#include <linux/fb.h>
#include <i2c.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/iomux-mx6.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/sys_proto.h>

#include "../common/karo.h"

#define TX6Q_FEC_RST_GPIO		IMX_GPIO_NR(7, 6)
#define TX6Q_FEC_PWR_GPIO		IMX_GPIO_NR(3, 20)
#define TX6Q_FEC_INT_GPIO		IMX_GPIO_NR(2, 4)
#define TX6Q_LED_GPIO			IMX_GPIO_NR(2, 20)

#define TX6Q_LCD_PWR_GPIO		IMX_GPIO_NR(2, 31)
#define TX6Q_LCD_RST_GPIO		IMX_GPIO_NR(3, 29)
#define TX6Q_LCD_BACKLIGHT_GPIO		IMX_GPIO_NR(1, 1)

#define TX6Q_RESET_OUT_GPIO		IMX_GPIO_NR(7, 12)

#define TEMPERATURE_MIN			-40
#define TEMPERATURE_HOT			80
#define TEMPERATURE_MAX			125

DECLARE_GLOBAL_DATA_PTR;

#define MUX_CFG_SION			IOMUX_PAD(0, 0, IOMUX_CONFIG_SION, 0, 0, 0)

static const iomux_v3_cfg_t tx6q_pads[] = {
	/* NAND flash pads */
	MX6Q_PAD_NANDF_CLE__RAWNAND_CLE,
	MX6Q_PAD_NANDF_ALE__RAWNAND_ALE,
	MX6Q_PAD_NANDF_WP_B__RAWNAND_RESETN,
	MX6Q_PAD_NANDF_RB0__RAWNAND_READY0,
	MX6Q_PAD_NANDF_CS0__RAWNAND_CE0N,
	MX6Q_PAD_SD4_CMD__RAWNAND_RDN,
	MX6Q_PAD_SD4_CLK__RAWNAND_WRN,
	MX6Q_PAD_NANDF_D0__RAWNAND_D0,
	MX6Q_PAD_NANDF_D1__RAWNAND_D1,
	MX6Q_PAD_NANDF_D2__RAWNAND_D2,
	MX6Q_PAD_NANDF_D3__RAWNAND_D3,
	MX6Q_PAD_NANDF_D4__RAWNAND_D4,
	MX6Q_PAD_NANDF_D5__RAWNAND_D5,
	MX6Q_PAD_NANDF_D6__RAWNAND_D6,
	MX6Q_PAD_NANDF_D7__RAWNAND_D7,

	/* RESET_OUT */
	MX6Q_PAD_GPIO_17__GPIO_7_12,

	/* UART pads */
#if CONFIG_MXC_UART_BASE == UART1_BASE
	MX6Q_PAD_SD3_DAT7__UART1_TXD,
	MX6Q_PAD_SD3_DAT6__UART1_RXD,
	MX6Q_PAD_SD3_DAT1__UART1_RTS,
	MX6Q_PAD_SD3_DAT0__UART1_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART2_BASE
	MX6Q_PAD_SD4_DAT4__UART2_RXD,
	MX6Q_PAD_SD4_DAT7__UART2_TXD,
	MX6Q_PAD_SD4_DAT5__UART2_RTS,
	MX6Q_PAD_SD4_DAT6__UART2_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART3_BASE
	MX6Q_PAD_EIM_D24__UART3_TXD,
	MX6Q_PAD_EIM_D25__UART3_RXD,
	MX6Q_PAD_SD3_RST__UART3_RTS,
	MX6Q_PAD_SD3_DAT3__UART3_CTS,
#endif
	/* internal I2C */
	MX6Q_PAD_EIM_D28__I2C1_SDA,
	MX6Q_PAD_EIM_D21__I2C1_SCL,

	/* FEC PHY GPIO functions */
	MX6Q_PAD_EIM_D20__GPIO_3_20 | MUX_CFG_SION, /* PHY POWER */
	MX6Q_PAD_SD3_DAT2__GPIO_7_6 | MUX_CFG_SION, /* PHY RESET */
	MX6Q_PAD_SD3_DAT4__GPIO_7_1, /* PHY INT */
};

static const iomux_v3_cfg_t tx6q_fec_pads[] = {
	/* FEC functions */
	MX6Q_PAD_ENET_MDC__ENET_MDC,
	MX6Q_PAD_ENET_MDIO__ENET_MDIO,
	MX6Q_PAD_GPIO_16__ENET_ANATOP_ETHERNET_REF_OUT,
	MX6Q_PAD_ENET_RX_ER__ENET_RX_ER,
	MX6Q_PAD_ENET_CRS_DV__ENET_RX_EN,
	MX6Q_PAD_ENET_RXD1__ENET_RDATA_1,
	MX6Q_PAD_ENET_RXD0__ENET_RDATA_0,
	MX6Q_PAD_ENET_TX_EN__ENET_TX_EN,
	MX6Q_PAD_ENET_TXD1__ENET_TDATA_1,
	MX6Q_PAD_ENET_TXD0__ENET_TDATA_0,
};

static const struct gpio tx6q_gpios[] = {
	{ TX6Q_RESET_OUT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "#RESET_OUT", },
	{ TX6Q_FEC_PWR_GPIO, GPIOF_OUTPUT_INIT_HIGH, "FEC PHY PWR", },
	{ TX6Q_FEC_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "FEC PHY RESET", },
	{ TX6Q_FEC_INT_GPIO, GPIOF_INPUT, "FEC PHY INT", },
};

/*
 * Functions
 */
/* placed in section '.data' to prevent overwriting relocation info
 * overlayed with bss
 */
static u32 wrsr __attribute__((section(".data")));

#define WRSR_POR			(1 << 4)
#define WRSR_TOUT			(1 << 1)
#define WRSR_SFTW			(1 << 0)

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

int read_cpu_temperature(void);
int check_cpu_temperature(int boot);

static void print_cpuinfo(void)
{
	u32 cpurev;

	cpurev = get_cpu_rev();

	printf("CPU:   Freescale i.MX6Q rev%d.%d at %d MHz\n",
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		mxc_get_clock(MXC_ARM_CLK) / 1000000);

	print_reset_cause();
	check_cpu_temperature(1);
}

#define LTC3676_DVB2A		0x0C
#define LTC3676_DVB2B		0x0D
#define LTC3676_DVB4A		0x10
#define LTC3676_DVB4B		0x11

#define VDD_SOC_mV		(1375 + 50)
#define VDD_CORE_mV		(1375 + 50)

#define mV_to_regval(mV)	(((mV) * 360 / 330 - 825 + 1) / 25)
#define regval_to_mV(v)		(((v) * 25 + 825) * 330 / 360)

static int setup_pmic_voltages(void)
{
	int ret;
	unsigned char value;

	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);

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

	/* VDDCORE/VDDSOC default 1.375V is not enough, considering
	   pfuze tolerance and IR drop and ripple, need increase
	   to 1.425V for SabreSD */

	value = 0x39; /* VB default value & PGOOD not forced when slewing */
	ret = i2c_write(CONFIG_SYS_I2C_SLAVE, LTC3676_DVB2B, 1, &value, 1);
	if (ret) {
		printf("%s: failed to write PMIC DVB2B register: %d\n",
			__func__, ret);
		return ret;
	}
	ret = i2c_write(CONFIG_SYS_I2C_SLAVE, LTC3676_DVB4B, 1, &value, 1);
	if (ret) {
		printf("%s: failed to write PMIC DVB4B register: %d\n",
			__func__, ret);
		return ret;
	}

	value = mV_to_regval(VDD_SOC_mV);
	ret = i2c_write(CONFIG_SYS_I2C_SLAVE, LTC3676_DVB2A, 1, &value, 1);
	if (ret) {
		printf("%s: failed to write PMIC DVB2A register: %d\n",
			__func__, ret);
		return ret;
	}
	printf("VDDSOC  set to %dmV\n", regval_to_mV(value));

	value = mV_to_regval(VDD_CORE_mV);
	ret = i2c_write(CONFIG_SYS_I2C_SLAVE, LTC3676_DVB4A, 1, &value, 1);
	if (ret) {
		printf("%s: failed to write PMIC DVB4A register: %d\n",
			__func__, ret);
		return ret;
	}
	printf("VDDCORE set to %dmV\n", regval_to_mV(value));
	return 0;
}

int board_early_init_f(void)
{
	gpio_request_array(tx6q_gpios, ARRAY_SIZE(tx6q_gpios));
	imx_iomux_v3_setup_multiple_pads(tx6q_pads, ARRAY_SIZE(tx6q_pads));

	return 0;
}

int board_init(void)
{
	int ret;

	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;
#if 1
	gd->bd->bi_arch_number = 4429;
#endif
	ret = setup_pmic_voltages();
	if (ret) {
		printf("Failed to setup PMIC voltages\n");
		hang();
	}
	return 0;
}

int dram_init(void)
{
	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				PHYS_SDRAM_1_SIZE);
	return 0;
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
	MX6Q_PAD_SD1_CMD__USDHC1_CMD,
	MX6Q_PAD_SD1_CLK__USDHC1_CLK,
	MX6Q_PAD_SD1_DAT0__USDHC1_DAT0,
	MX6Q_PAD_SD1_DAT1__USDHC1_DAT1,
	MX6Q_PAD_SD1_DAT2__USDHC1_DAT2,
	MX6Q_PAD_SD1_DAT3__USDHC1_DAT3,
	/* SD1 CD */
	MX6Q_PAD_SD3_CMD__GPIO_7_2,
};

static const iomux_v3_cfg_t mmc1_pads[] = {
	MX6Q_PAD_SD2_CMD__USDHC2_CMD,
	MX6Q_PAD_SD2_CLK__USDHC2_CLK,
	MX6Q_PAD_SD2_DAT0__USDHC2_DAT0,
	MX6Q_PAD_SD2_DAT1__USDHC2_DAT1,
	MX6Q_PAD_SD2_DAT2__USDHC2_DAT2,
	MX6Q_PAD_SD2_DAT3__USDHC2_DAT3,
	/* SD2 CD */
	MX6Q_PAD_SD3_CLK__GPIO_7_3,
};

static struct tx6q_esdhc_cfg {
	const iomux_v3_cfg_t *pads;
	int num_pads;
	enum mxc_clock clkid;
	struct fsl_esdhc_cfg cfg;
} tx6q_esdhc_cfg[] = {
	{
		.pads = mmc0_pads,
		.num_pads = ARRAY_SIZE(mmc0_pads),
		.clkid = MXC_ESDHC_CLK,
		.cfg = {
			.esdhc_base = (void __iomem *)USDHC1_BASE_ADDR,
			.cd_gpio = IMX_GPIO_NR(7, 2),
			.wp_gpio = -EINVAL,
		},
	},
	{
		.pads = mmc1_pads,
		.num_pads = ARRAY_SIZE(mmc1_pads),
		.clkid = MXC_ESDHC2_CLK,
		.cfg = {
			.esdhc_base = (void __iomem *)USDHC2_BASE_ADDR,
			.cd_gpio = IMX_GPIO_NR(7, 3),
			.wp_gpio = -EINVAL,
		},
	},
};

static inline struct tx6q_esdhc_cfg *to_tx6q_esdhc_cfg(struct fsl_esdhc_cfg *cfg)
{
	void *p = cfg;

	return p - offsetof(struct tx6q_esdhc_cfg, cfg);
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = mmc->priv;

	if (cfg->cd_gpio < 0)
		return cfg->cd_gpio;

	debug("SD card %d is %spresent\n",
		to_tx6q_esdhc_cfg(cfg) - tx6q_esdhc_cfg, gpio_get_value(cfg->cd_gpio) ? "NOT " : "");
	return !gpio_get_value(cfg->cd_gpio);
}

int board_mmc_init(bd_t *bis)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tx6q_esdhc_cfg); i++) {
		struct mmc *mmc;
		struct fsl_esdhc_cfg *cfg = &tx6q_esdhc_cfg[i].cfg;

		if (i >= CONFIG_SYS_FSL_ESDHC_NUM)
			break;

		cfg->sdhc_clk = mxc_get_clock(tx6q_esdhc_cfg[i].clkid);
		imx_iomux_v3_setup_multiple_pads(tx6q_esdhc_cfg[i].pads,
						tx6q_esdhc_cfg[i].num_pads);

		debug("%s: Initializing MMC slot %d\n", __func__, i);
		fsl_esdhc_initialize(bis, cfg);

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

#define FEC_PAD_CTL	(PAD_CTL_DVS | PAD_CTL_DSE_HIGH | \
			PAD_CTL_SRE_FAST)
#define FEC_PAD_CTL2	(PAD_CTL_DVS | PAD_CTL_SRE_FAST)
#define GPIO_PAD_CTL	(PAD_CTL_DVS | PAD_CTL_DSE_HIGH)

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

int board_eth_init(bd_t *bis)
{
	int ret;

	/* delay at least 21ms for the PHY internal POR signal to deassert */
	udelay(22000);

	imx_iomux_v3_setup_multiple_pads(tx6q_fec_pads, ARRAY_SIZE(tx6q_fec_pads));

	/* Deassert RESET to the external phy */
	gpio_set_value(TX6Q_FEC_RST_GPIO, 1);

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

static inline int calc_blink_rate(int tmp)
{
	return CONFIG_SYS_HZ + CONFIG_SYS_HZ / 10 -
		(tmp - TEMPERATURE_MIN) * CONFIG_SYS_HZ /
		(TEMPERATURE_HOT - TEMPERATURE_MIN);
}

void show_activity(int arg)
{
	static int led_state = LED_STATE_INIT;
	static int blink_rate;
	static ulong last;

	if (led_state == LED_STATE_INIT) {
		last = get_timer(0);
		gpio_set_value(TX6Q_LED_GPIO, 1);
		led_state = LED_STATE_ON;
		blink_rate = calc_blink_rate(check_cpu_temperature(0));
	} else {
		if (get_timer(last) > blink_rate) {
			blink_rate = calc_blink_rate(check_cpu_temperature(0));
			last = get_timer_masked();
			if (led_state == LED_STATE_ON) {
				gpio_set_value(TX6Q_LED_GPIO, 0);
			} else {
				gpio_set_value(TX6Q_LED_GPIO, 1);
			}
			led_state = 1 - led_state;
		}
	}
}

static const iomux_v3_cfg_t stk5_pads[] = {
	/* SW controlled LED on STK5 baseboard */
	MX6Q_PAD_EIM_A18__GPIO_2_20,

	/* LCD data pins */
	MX6Q_PAD_DISP0_DAT0__IPU1_DISP0_DAT_0,
	MX6Q_PAD_DISP0_DAT1__IPU1_DISP0_DAT_1,
	MX6Q_PAD_DISP0_DAT2__IPU1_DISP0_DAT_2,
	MX6Q_PAD_DISP0_DAT3__IPU1_DISP0_DAT_3,
	MX6Q_PAD_DISP0_DAT4__IPU1_DISP0_DAT_4,
	MX6Q_PAD_DISP0_DAT5__IPU1_DISP0_DAT_5,
	MX6Q_PAD_DISP0_DAT6__IPU1_DISP0_DAT_6,
	MX6Q_PAD_DISP0_DAT7__IPU1_DISP0_DAT_7,
	MX6Q_PAD_DISP0_DAT8__IPU1_DISP0_DAT_8,
	MX6Q_PAD_DISP0_DAT9__IPU1_DISP0_DAT_9,
	MX6Q_PAD_DISP0_DAT10__IPU1_DISP0_DAT_10,
	MX6Q_PAD_DISP0_DAT11__IPU1_DISP0_DAT_11,
	MX6Q_PAD_DISP0_DAT12__IPU1_DISP0_DAT_12,
	MX6Q_PAD_DISP0_DAT13__IPU1_DISP0_DAT_13,
	MX6Q_PAD_DISP0_DAT14__IPU1_DISP0_DAT_14,
	MX6Q_PAD_DISP0_DAT15__IPU1_DISP0_DAT_15,
	MX6Q_PAD_DISP0_DAT16__IPU1_DISP0_DAT_16,
	MX6Q_PAD_DISP0_DAT17__IPU1_DISP0_DAT_17,
	MX6Q_PAD_DISP0_DAT18__IPU1_DISP0_DAT_18,
	MX6Q_PAD_DISP0_DAT19__IPU1_DISP0_DAT_19,
	MX6Q_PAD_DISP0_DAT20__IPU1_DISP0_DAT_20,
	MX6Q_PAD_DISP0_DAT21__IPU1_DISP0_DAT_21,
	MX6Q_PAD_DISP0_DAT22__IPU1_DISP0_DAT_22,
	MX6Q_PAD_DISP0_DAT23__IPU1_DISP0_DAT_23,
	MX6Q_PAD_DI0_PIN2__IPU1_DI0_PIN2, /* HSYNC */
	MX6Q_PAD_DI0_PIN3__IPU1_DI0_PIN3, /* VSYNC */
	MX6Q_PAD_DI0_PIN15__IPU1_DI0_PIN15, /* OE_ACD */
	MX6Q_PAD_DI0_DISP_CLK__IPU1_DI0_DISP_CLK, /* LSCLK */

	/* I2C bus on DIMM pins 40/41 */
	MX6Q_PAD_GPIO_6__I2C3_SDA,
	MX6Q_PAD_GPIO_3__I2C3_SCL,

	/* TSC200x PEN IRQ */
	MX6Q_PAD_EIM_D26__GPIO_3_26,

	/* EDT-FT5x06 Polytouch panel */
	MX6Q_PAD_NANDF_CS2__GPIO_6_15, /* IRQ */
	MX6Q_PAD_EIM_A16__GPIO_2_22, /* RESET */
	MX6Q_PAD_EIM_A17__GPIO_2_21, /* WAKE */

	/* USBH1 */
	MX6Q_PAD_EIM_D31__GPIO_3_31, /* VBUSEN */
	MX6Q_PAD_EIM_D30__GPIO_3_30, /* OC */
	/* USBOTG */
	MX6Q_PAD_EIM_D23__GPIO_3_23, /* USBOTG ID */
	MX6Q_PAD_GPIO_7__GPIO_1_7, /* VBUSEN */
	MX6Q_PAD_GPIO_8__GPIO_1_8, /* OC */

	/* DEBUG */
	MX6Q_PAD_GPIO_0__CCM_CLKO,
	MX6Q_PAD_NANDF_CS2__CCM_CLKO2,
};

static const struct gpio stk5_gpios[] = {
	{ TX6Q_LED_GPIO, GPIOF_OUTPUT_INIT_LOW, "HEARTBEAT LED", },

	{ IMX_GPIO_NR(3, 23), GPIOF_INPUT, "USBOTG ID", },
	{ IMX_GPIO_NR(1, 8), GPIOF_INPUT, "USBOTG OC", },
	{ IMX_GPIO_NR(1, 7), GPIOF_OUTPUT_INIT_LOW, "USBOTG VBUS enable", },
	{ IMX_GPIO_NR(3, 30), GPIOF_INPUT, "USBH1 OC", },
	{ IMX_GPIO_NR(3, 31), GPIOF_OUTPUT_INIT_LOW, "USBH1 VBUS enable", },
};

#ifdef CONFIG_LCD
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 1920,
	.vl_row = 1080,

	.vl_bpix = LCD_COLOR24,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
};

static struct fb_videomode tx6q_fb_mode = {
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
	.vmode		= FB_VMODE_NONINTERLACED,
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

	karo_load_splashimage(1);
	if (lcd_enabled) {
		debug("Switching LCD on\n");
		gpio_set_value(TX6Q_LCD_PWR_GPIO, 1);
		udelay(100);
		gpio_set_value(TX6Q_LCD_RST_GPIO, 1);
		udelay(300000);
		gpio_set_value(TX6Q_LCD_BACKLIGHT_GPIO, 0);
	}
}

static const iomux_v3_cfg_t stk5_lcd_pads[] = {
	/* LCD RESET */
	MX6Q_PAD_EIM_D29__GPIO_3_29,
	/* LCD POWER_ENABLE */
	MX6Q_PAD_EIM_EB3__GPIO_2_31,
	/* LCD Backlight (PWM) */
	MX6Q_PAD_GPIO_1__GPIO_1_1,

	/* Display */
	MX6Q_PAD_DI0_DISP_CLK__IPU1_DI0_DISP_CLK,
	MX6Q_PAD_DI0_PIN15__IPU1_DI0_PIN15,
	MX6Q_PAD_DI0_PIN2__IPU1_DI0_PIN2,
	MX6Q_PAD_DI0_PIN3__IPU1_DI0_PIN3,
	MX6Q_PAD_DISP0_DAT0__IPU1_DISP0_DAT_0,
	MX6Q_PAD_DISP0_DAT1__IPU1_DISP0_DAT_1,
	MX6Q_PAD_DISP0_DAT2__IPU1_DISP0_DAT_2,
	MX6Q_PAD_DISP0_DAT3__IPU1_DISP0_DAT_3,
	MX6Q_PAD_DISP0_DAT4__IPU1_DISP0_DAT_4,
	MX6Q_PAD_DISP0_DAT5__IPU1_DISP0_DAT_5,
	MX6Q_PAD_DISP0_DAT6__IPU1_DISP0_DAT_6,
	MX6Q_PAD_DISP0_DAT7__IPU1_DISP0_DAT_7,
	MX6Q_PAD_DISP0_DAT8__IPU1_DISP0_DAT_8,
	MX6Q_PAD_DISP0_DAT9__IPU1_DISP0_DAT_9,
	MX6Q_PAD_DISP0_DAT10__IPU1_DISP0_DAT_10,
	MX6Q_PAD_DISP0_DAT11__IPU1_DISP0_DAT_11,
	MX6Q_PAD_DISP0_DAT12__IPU1_DISP0_DAT_12,
	MX6Q_PAD_DISP0_DAT13__IPU1_DISP0_DAT_13,
	MX6Q_PAD_DISP0_DAT14__IPU1_DISP0_DAT_14,
	MX6Q_PAD_DISP0_DAT15__IPU1_DISP0_DAT_15,
	MX6Q_PAD_DISP0_DAT16__IPU1_DISP0_DAT_16,
	MX6Q_PAD_DISP0_DAT17__IPU1_DISP0_DAT_17,
	MX6Q_PAD_DISP0_DAT18__IPU1_DISP0_DAT_18,
	MX6Q_PAD_DISP0_DAT19__IPU1_DISP0_DAT_19,
	MX6Q_PAD_DISP0_DAT20__IPU1_DISP0_DAT_20,
	MX6Q_PAD_DISP0_DAT21__IPU1_DISP0_DAT_21,
	MX6Q_PAD_DISP0_DAT22__IPU1_DISP0_DAT_22,
	MX6Q_PAD_DISP0_DAT23__IPU1_DISP0_DAT_23,

	/* LVDS option */
	MX6Q_PAD_LVDS1_TX3_P__LDB_LVDS1_TX3,
	MX6Q_PAD_LVDS1_TX2_P__LDB_LVDS1_TX2,
	MX6Q_PAD_LVDS1_CLK_P__LDB_LVDS1_CLK,
	MX6Q_PAD_LVDS1_TX1_P__LDB_LVDS1_TX1,
	MX6Q_PAD_LVDS1_TX0_P__LDB_LVDS1_TX0,
	MX6Q_PAD_LVDS0_TX3_P__LDB_LVDS0_TX3,
	MX6Q_PAD_LVDS0_CLK_P__LDB_LVDS0_CLK,
	MX6Q_PAD_LVDS0_TX2_P__LDB_LVDS0_TX2,
	MX6Q_PAD_LVDS0_TX1_P__LDB_LVDS0_TX1,
	MX6Q_PAD_LVDS0_TX0_P__LDB_LVDS0_TX0,
};

static const struct gpio stk5_lcd_gpios[] = {
	{ TX6Q_LCD_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD RESET", },
	{ TX6Q_LCD_PWR_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD POWER", },
	{ TX6Q_LCD_BACKLIGHT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

void lcd_ctrl_init(void *lcdbase)
{
	int color_depth = 24;
	char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p = &tx6q_fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;
	int pix_fmt = 0;
	ipu_di_clk_parent_t di_clk_parent = DI_PCLK_PLL3;
	unsigned long di_clk_rate = 65000000;

	if (!lcd_enabled) {
		debug("LCD disabled\n");
		return;
	}

	if (tstc() || (wrsr & WRSR_TOUT)) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		return;
	}

	vm = getenv("video_mode");
	if (vm == NULL) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		return;
	}
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
	switch (color_depth) {
	case 8:
		panel_info.vl_bpix = 3;
		break;

	case 16:
		panel_info.vl_bpix = 4;
		break;

	case 18:
	case 24:
		panel_info.vl_bpix = 5;
	}

	p->pixclock = KHZ2PICOS(refresh *
		(p->xres + p->left_margin + p->right_margin + p->hsync_len) *
		(p->yres + p->upper_margin + p->lower_margin + p->vsync_len)
		/ 1000);
	debug("Pixel clock set to %lu.%03lu MHz\n",
		PICOS2KHZ(p->pixclock) / 1000,
		PICOS2KHZ(p->pixclock) % 1000);

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
		writel(readl(&ccm_regs->CCGR3) | (3 << 12) | (3 << 2),
			&ccm_regs->CCGR3);
	}

	if (karo_load_splashimage(0) == 0) {
		debug("Initializing LCD controller\n");
		ipuv3_fb_init(p, 0, pix_fmt, di_clk_parent, di_clk_rate, -1);
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
	imx_iomux_v3_setup_pad(MX6Q_PAD_DISP0_DAT0__GPIO_4_21);
}

static void tx6q_set_cpu_clock(void)
{
	unsigned long cpu_clk = getenv_ulong("cpu_clk", 10, 0);

	if (tstc() || (wrsr & WRSR_TOUT))
		return;

	if (cpu_clk == 0 || cpu_clk == mxc_get_clock(MXC_ARM_CLK) / 1000000)
		return;

	if (mxc_set_clock(CONFIG_SYS_MX6_HCLK, cpu_clk, MXC_ARM_CLK) == 0) {
		cpu_clk = mxc_get_clock(MXC_ARM_CLK);
		printf("CPU clock set to %lu.%03lu MHz\n",
			cpu_clk / 1000000, cpu_clk / 1000 % 1000);
	} else {
		printf("Failed to set CPU clock to %lu MHz\n", cpu_clk);
	}
}

static void tx6_init_mac(void)
{
	u8 mac[ETH_ALEN];
	char mac_str[ETH_ALEN * 3] = "";

	imx_get_mac_from_fuse(-1, mac);
	if (!is_valid_ether_addr(mac)) {
		printf("No valid MAC address programmed\n");
		return;
	}

	snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	setenv("ethaddr", mac_str);
	printf("MAC addr from fuse: %02x:%02x:%02x:%02x:%02x:%02x\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int board_late_init(void)
{
	int ret = 0;
	const char *baseboard;

	tx6q_set_cpu_clock();
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
	tx6_init_mac();

	gpio_set_value(TX6Q_RESET_OUT_GPIO, 1);
	return ret;
}

#define iomux_field(v,f)	(((iomux_v3_cfg_t)(v) << f##_SHIFT) & f##_MASK)

#define chk_iomux_field(f1,f2)	({					\
	iomux_v3_cfg_t __c = iomux_field(~0, f1);			\
	if (__c & f2##_MASK) {						\
		printf("%18s[%016llx] overlaps with:\n%18s[%016llx]\n",	\
			#f1, f1##_MASK,					\
			#f2, f2##_MASK);				\
	}								\
	(__c & f2##_MASK) != 0;						\
})

#define chk_iomux_bit(f1,f2)	({					\
	iomux_v3_cfg_t __c = iomux_field(~0, f1);			\
	if (__c & f2) {							\
		printf("%18s[%016llx] overlaps with:\n%18s[%016llx]\n",	\
			#f1, f1##_MASK,					\
			#f2, (iomux_v3_cfg_t)f2);			\
	}								\
	(__c & f2) != 0;						\
})

int checkboard(void)
{
	print_cpuinfo();

	printf("Board: Ka-Ro TX6Q\n");

#ifdef TIMER_TEST
	{
		struct mxc_gpt {
			unsigned int control;
			unsigned int prescaler;
			unsigned int status;
			unsigned int nouse[6];
			unsigned int counter;
		};
		const int us_delay = 10;
		unsigned long start = get_timer(0);
		unsigned long last = gd->arch.tbl;
		unsigned long loop = 0;
		unsigned long cnt = 0;
		static struct mxc_gpt *timer_base = (struct mxc_gpt *)GPT1_BASE_ADDR;

		printf("GPT prescaler=%u\n", readl(&timer_base->prescaler) + 1);
		printf("clock tick rate: %lu.%03lukHz\n",
			gd->arch.timer_rate_hz / 1000, gd->arch.timer_rate_hz % 1000);
		printf("ticks/us=%lu\n", gd->arch.timer_rate_hz / CONFIG_SYS_HZ / 1000);

		while (!tstc()) {
			unsigned long elapsed = get_timer(start);
			unsigned long diff = gd->arch.tbl - last;

			loop++;
			last = gd->arch.tbl;

			printf("loop %4lu: t=%08lx diff=%08lx steps=%6lu elapsed time: %4lu",
				loop, gd->arch.tbl, diff, cnt, elapsed / CONFIG_SYS_HZ);
			cnt = 0;
			while (get_timer(elapsed + start) < CONFIG_SYS_HZ) {
				cnt++;
				udelay(us_delay);
			}
			printf(" counter=%08x udelay(%u)=%lu.%03luus\n",
				readl(&timer_base->counter), us_delay,
				1000000000 / cnt / 1000, 1000000000 / cnt % 1000);
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
	{ "fsl,imx6q-gpmi-nand", MTD_DEV_TYPE_NAND, },
};

#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif

#ifdef CONFIG_SERIAL_TAG
void get_board_serial(struct tag_serialnr *serialnr)
{
	struct iim_regs *iim = (struct iim_regs *)IMX_IIM_BASE;
	struct fuse_bank0_regs *fuse = (void *)iim->bank[0].fuse_regs;

	serialnr->low = readl(&fuse->cfg0);
	serialnr->high = readl(&fuse->cfg1);
}
#endif

static void tx6q_fixup_flexcan(void *blob)
{
	const char *baseboard = getenv("baseboard");

	if (baseboard && strcmp(baseboard, "stk5-v5") == 0)
		return;

	karo_fdt_del_prop(blob, "fsl,p1010-flexcan", 0x02090000, "transceiver-switch");
	karo_fdt_del_prop(blob, "fsl,p1010-flexcan", 0x02094000, "transceiver-switch");
}

void ft_board_setup(void *blob, bd_t *bd)
{
	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
	fdt_fixup_ethernet(blob);

	karo_fdt_fixup_touchpanel(blob);
	karo_fdt_fixup_usb_otg(blob, "", 0);
	tx6q_fixup_flexcan(blob);
}
#endif
