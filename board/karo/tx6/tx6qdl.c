/*
 * Copyright (C) 2012,2013 Lothar Waßmann <LW@KARO-electronics.de>
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
#include <i2c.h>
#include <linux/fb.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/sys_proto.h>

#include "../common/karo.h"

#define TX6_FEC_RST_GPIO		IMX_GPIO_NR(7, 6)
#define TX6_FEC_PWR_GPIO		IMX_GPIO_NR(3, 20)
#define TX6_FEC_INT_GPIO		IMX_GPIO_NR(7, 1)
#define TX6_LED_GPIO			IMX_GPIO_NR(2, 20)

#define TX6_LCD_PWR_GPIO		IMX_GPIO_NR(2, 31)
#define TX6_LCD_RST_GPIO		IMX_GPIO_NR(3, 29)
#define TX6_LCD_BACKLIGHT_GPIO		IMX_GPIO_NR(1, 1)

#define TX6_RESET_OUT_GPIO		IMX_GPIO_NR(7, 12)

#define TEMPERATURE_MIN			-40
#define TEMPERATURE_HOT			80
#define TEMPERATURE_MAX			125

DECLARE_GLOBAL_DATA_PTR;

#define MUX_CFG_SION			IOMUX_PAD(0, 0, IOMUX_CONFIG_SION, 0, 0, 0)

static const iomux_v3_cfg_t tx6qdl_pads[] = {
	/* NAND flash pads */
	MX6_PAD_NANDF_CLE__RAWNAND_CLE,
	MX6_PAD_NANDF_ALE__RAWNAND_ALE,
	MX6_PAD_NANDF_WP_B__RAWNAND_RESETN,
	MX6_PAD_NANDF_RB0__RAWNAND_READY0,
	MX6_PAD_NANDF_CS0__RAWNAND_CE0N,
	MX6_PAD_SD4_CMD__RAWNAND_RDN,
	MX6_PAD_SD4_CLK__RAWNAND_WRN,
	MX6_PAD_NANDF_D0__RAWNAND_D0,
	MX6_PAD_NANDF_D1__RAWNAND_D1,
	MX6_PAD_NANDF_D2__RAWNAND_D2,
	MX6_PAD_NANDF_D3__RAWNAND_D3,
	MX6_PAD_NANDF_D4__RAWNAND_D4,
	MX6_PAD_NANDF_D5__RAWNAND_D5,
	MX6_PAD_NANDF_D6__RAWNAND_D6,
	MX6_PAD_NANDF_D7__RAWNAND_D7,

	/* RESET_OUT */
	MX6_PAD_GPIO_17__GPIO_7_12,

	/* UART pads */
#if CONFIG_MXC_UART_BASE == UART1_BASE
	MX6_PAD_SD3_DAT7__UART1_TXD,
	MX6_PAD_SD3_DAT6__UART1_RXD,
	MX6_PAD_SD3_DAT1__UART1_RTS,
	MX6_PAD_SD3_DAT0__UART1_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART2_BASE
	MX6_PAD_SD4_DAT4__UART2_RXD,
	MX6_PAD_SD4_DAT7__UART2_TXD,
	MX6_PAD_SD4_DAT5__UART2_RTS,
	MX6_PAD_SD4_DAT6__UART2_CTS,
#endif
#if CONFIG_MXC_UART_BASE == UART3_BASE
	MX6_PAD_EIM_D24__UART3_TXD,
	MX6_PAD_EIM_D25__UART3_RXD,
	MX6_PAD_SD3_RST__UART3_RTS,
	MX6_PAD_SD3_DAT3__UART3_CTS,
#endif
	/* internal I2C */
	MX6_PAD_EIM_D28__I2C1_SDA,
	MX6_PAD_EIM_D21__I2C1_SCL,

	/* FEC PHY GPIO functions */
	MX6_PAD_EIM_D20__GPIO_3_20 | MUX_CFG_SION, /* PHY POWER */
	MX6_PAD_SD3_DAT2__GPIO_7_6 | MUX_CFG_SION, /* PHY RESET */
	MX6_PAD_SD3_DAT4__GPIO_7_1, /* PHY INT */
};

static const iomux_v3_cfg_t tx6qdl_fec_pads[] = {
	/* FEC functions */
	MX6_PAD_ENET_MDC__ENET_MDC,
	MX6_PAD_ENET_MDIO__ENET_MDIO,
	MX6_PAD_GPIO_16__ENET_ANATOP_ETHERNET_REF_OUT,
	MX6_PAD_ENET_RX_ER__ENET_RX_ER,
	MX6_PAD_ENET_CRS_DV__ENET_RX_EN,
	MX6_PAD_ENET_RXD1__ENET_RDATA_1,
	MX6_PAD_ENET_RXD0__ENET_RDATA_0,
	MX6_PAD_ENET_TX_EN__ENET_TX_EN,
	MX6_PAD_ENET_TXD1__ENET_TDATA_1,
	MX6_PAD_ENET_TXD0__ENET_TDATA_0,
};

static const struct gpio tx6qdl_gpios[] = {
	{ TX6_RESET_OUT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "#RESET_OUT", },
	{ TX6_FEC_PWR_GPIO, GPIOF_OUTPUT_INIT_HIGH, "FEC PHY PWR", },
	{ TX6_FEC_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "FEC PHY RESET", },
	{ TX6_FEC_INT_GPIO, GPIOF_INPUT, "FEC PHY INT", },
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

static void tx6qdl_print_cpuinfo(void)
{
	u32 cpurev = get_cpu_rev();
	char *cpu_str = "?";

	switch ((cpurev >> 12) & 0xff) {
	case MXC_CPU_MX6SL:
		cpu_str = "SL";
		break;
	case MXC_CPU_MX6DL:
		cpu_str = "DL";
		break;
	case MXC_CPU_MX6SOLO:
		cpu_str = "SOLO";
		break;
	case MXC_CPU_MX6Q:
		cpu_str = "Q";
		break;
	}

	printf("CPU:   Freescale i.MX6%s rev%d.%d at %d MHz\n",
		cpu_str,
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		mxc_get_clock(MXC_ARM_CLK) / 1000000);

	print_reset_cause();
	check_cpu_temperature(1);
}

#define LTC3676_BUCK1		0x01
#define LTC3676_BUCK2		0x02
#define LTC3676_BUCK3		0x03
#define LTC3676_BUCK4		0x04
#define LTC3676_DVB1A		0x0A
#define LTC3676_DVB1B		0x0B
#define LTC3676_DVB2A		0x0C
#define LTC3676_DVB2B		0x0D
#define LTC3676_DVB3A		0x0E
#define LTC3676_DVB3B		0x0F
#define LTC3676_DVB4A		0x10
#define LTC3676_DVB4B		0x11
#define LTC3676_MSKPG		0x13
#define LTC3676_CLIRQ		0x1f

#define LTC3676_BUCK_DVDT_FAST	(1 << 0)
#define LTC3676_BUCK_KEEP_ALIVE	(1 << 1)
#define LTC3676_BUCK_CLK_RATE_LOW (1 << 2)
#define LTC3676_BUCK_PHASE_SEL	(1 << 3)
#define LTC3676_BUCK_ENABLE_300	(1 << 4)
#define LTC3676_BUCK_PULSE_SKIP	(0 << 5)
#define LTC3676_BUCK_BURST_MODE	(1 << 5)
#define LTC3676_BUCK_CONTINUOUS	(2 << 5)
#define LTC3676_BUCK_ENABLE	(1 << 7)

#define LTC3676_PGOOD_MASK	(1 << 5)

#define LTC3676_MSKPG_BUCK1	(1 << 0)
#define LTC3676_MSKPG_BUCK2	(1 << 1)
#define LTC3676_MSKPG_BUCK3	(1 << 2)
#define LTC3676_MSKPG_BUCK4	(1 << 3)
#define LTC3676_MSKPG_LDO2	(1 << 5)
#define LTC3676_MSKPG_LDO3	(1 << 6)
#define LTC3676_MSKPG_LDO4	(1 << 7)

#define VDD_IO_VAL		mV_to_regval(vout_to_vref(3300 * 10, 5))
#define VDD_IO_VAL_LP		mV_to_regval(vout_to_vref(3100 * 10, 5))
#define VDD_IO_VAL_2		mV_to_regval(vout_to_vref(3300 * 10, 5_2))
#define VDD_IO_VAL_2_LP		mV_to_regval(vout_to_vref(3100 * 10, 5_2))
#define VDD_SOC_VAL		mV_to_regval(vout_to_vref(1425 * 10, 6))
#define VDD_SOC_VAL_LP		mV_to_regval(vout_to_vref(900 * 10, 6))
#define VDD_DDR_VAL		mV_to_regval(vout_to_vref(1500 * 10, 7))
#define VDD_CORE_VAL		mV_to_regval(vout_to_vref(1425 * 10, 8))
#define VDD_CORE_VAL_LP		mV_to_regval(vout_to_vref(900 * 10, 8))

/* LDO1 */
#define R1_1			470
#define R2_1			150
/* LDO4 */
#define R1_4			470
#define R2_4			150
/* Buck1 */
#define R1_5			390
#define R2_5			110
#define R1_5_2			470
#define R2_5_2			150
/* Buck2 */
#define R1_6			150
#define R2_6			180
/* Buck3 */
#define R1_7			150
#define R2_7			140
/* Buck4 */
#define R1_8			150
#define R2_8			180

/* calculate voltages in 10mV */
#define R1(idx)			R1_##idx
#define R2(idx)			R2_##idx

#define vout_to_vref(vout, idx)	((vout) * R2(idx) / (R1(idx) + R2(idx)))
#define vref_to_vout(vref, idx)	DIV_ROUND_UP((vref) * (R1(idx) + R2(idx)), R2(idx))

#define mV_to_regval(mV)	DIV_ROUND(((((mV) < 4125) ? 4125 : (mV)) - 4125), 125)
#define regval_to_mV(v)		(((v) * 125 + 4125))

static struct ltc3673_regs {
	u8 addr;
	u8 val;
	u8 mask;
} ltc3676_regs[] = {
	{ LTC3676_MSKPG, ~LTC3676_MSKPG_BUCK1, },
	{ LTC3676_DVB2B, VDD_SOC_VAL | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB3B, VDD_DDR_VAL, ~0x3f, },
	{ LTC3676_DVB4B, VDD_CORE_VAL | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB2A, VDD_SOC_VAL, ~0x3f, },
	{ LTC3676_DVB3A, VDD_DDR_VAL, ~0x3f, },
	{ LTC3676_DVB4A, VDD_CORE_VAL, ~0x3f, },
	{ LTC3676_BUCK1, LTC3676_BUCK_BURST_MODE | LTC3676_BUCK_CLK_RATE_LOW, },
	{ LTC3676_BUCK2, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_BUCK3, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_BUCK4, LTC3676_BUCK_BURST_MODE, },
	{ LTC3676_CLIRQ, 0, }, /* clear interrupt status */
};

static struct ltc3673_regs ltc3676_regs_1[] = {
	{ LTC3676_DVB1B, VDD_IO_VAL_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB1A, VDD_IO_VAL, ~0x3f, },
};

static struct ltc3673_regs ltc3676_regs_2[] = {
	{ LTC3676_DVB1B, VDD_IO_VAL_2_LP | LTC3676_PGOOD_MASK, ~0x3f, },
	{ LTC3676_DVB1A, VDD_IO_VAL_2, ~0x3f, },
};

static int tx6_rev_2(void)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank5_regs *fuse = (void *)ocotp->bank[5].fuse_regs;
	u32 pad_settings = readl(&fuse->pad_settings);

	debug("Fuse pad_settings @ %p = %02x\n",
		&fuse->pad_settings, pad_settings);
	return pad_settings & 1;
}

static int tx6_ltc3676_setup_regs(struct ltc3673_regs *r, size_t count)
{
	int ret;
	int i;

	for (i = 0; i < count; i++, r++) {
#ifdef DEBUG
		unsigned char value;

		ret = i2c_read(CONFIG_SYS_I2C_SLAVE, r->addr, 1, &value, 1);
		if ((value & ~r->mask) != r->val) {
			printf("Changing PMIC reg %02x from %02x to %02x\n",
				r->addr, value, r->val);
		}
		if (ret) {
			printf("%s: failed to read PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
#endif
		ret = i2c_write(CONFIG_SYS_I2C_SLAVE,
				r->addr, 1, &r->val, 1);
		if (ret) {
			printf("%s: failed to write PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
	}
	return 0;
}

static int setup_pmic_voltages(void)
{
	int ret;
	unsigned char value;

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

	ret = tx6_ltc3676_setup_regs(ltc3676_regs, ARRAY_SIZE(ltc3676_regs));
	if (ret)
		return ret;

	printf("VDDCORE set to %umV\n",
		DIV_ROUND(vref_to_vout(regval_to_mV(VDD_CORE_VAL), 8), 10));
	printf("VDDSOC  set to %umV\n",
		DIV_ROUND(vref_to_vout(regval_to_mV(VDD_SOC_VAL), 6), 10));

	if (tx6_rev_2()) {
		ret = tx6_ltc3676_setup_regs(ltc3676_regs_2,
				ARRAY_SIZE(ltc3676_regs_2));
		printf("VDDIO   set to %umV\n",
			DIV_ROUND(vref_to_vout(
					regval_to_mV(VDD_IO_VAL_2), 5_2), 10));
	} else {
		ret = tx6_ltc3676_setup_regs(ltc3676_regs_1,
				ARRAY_SIZE(ltc3676_regs_1));
	}
	return ret;
}

int board_early_init_f(void)
{
	gpio_request_array(tx6qdl_gpios, ARRAY_SIZE(tx6qdl_gpios));
	imx_iomux_v3_setup_multiple_pads(tx6qdl_pads, ARRAY_SIZE(tx6qdl_pads));

	return 0;
}

int board_init(void)
{
	int ret;

	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;
	gd->bd->bi_arch_number = -1;

	if (ctrlc()) {
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
	MX6_PAD_SD1_CMD__USDHC1_CMD,
	MX6_PAD_SD1_CLK__USDHC1_CLK,
	MX6_PAD_SD1_DAT0__USDHC1_DAT0,
	MX6_PAD_SD1_DAT1__USDHC1_DAT1,
	MX6_PAD_SD1_DAT2__USDHC1_DAT2,
	MX6_PAD_SD1_DAT3__USDHC1_DAT3,
	/* SD1 CD */
	MX6_PAD_SD3_CMD__GPIO_7_2,
};

static const iomux_v3_cfg_t mmc1_pads[] = {
	MX6_PAD_SD2_CMD__USDHC2_CMD,
	MX6_PAD_SD2_CLK__USDHC2_CLK,
	MX6_PAD_SD2_DAT0__USDHC2_DAT0,
	MX6_PAD_SD2_DAT1__USDHC2_DAT1,
	MX6_PAD_SD2_DAT2__USDHC2_DAT2,
	MX6_PAD_SD2_DAT3__USDHC2_DAT3,
	/* SD2 CD */
	MX6_PAD_SD3_CLK__GPIO_7_3,
};

static struct tx6_esdhc_cfg {
	const iomux_v3_cfg_t *pads;
	int num_pads;
	enum mxc_clock clkid;
	struct fsl_esdhc_cfg cfg;
	int cd_gpio;
} tx6qdl_esdhc_cfg[] = {
	{
		.pads = mmc0_pads,
		.num_pads = ARRAY_SIZE(mmc0_pads),
		.clkid = MXC_ESDHC_CLK,
		.cfg = {
			.esdhc_base = (void __iomem *)USDHC1_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = IMX_GPIO_NR(7, 2),
	},
	{
		.pads = mmc1_pads,
		.num_pads = ARRAY_SIZE(mmc1_pads),
		.clkid = MXC_ESDHC2_CLK,
		.cfg = {
			.esdhc_base = (void __iomem *)USDHC2_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = IMX_GPIO_NR(7, 3),
	},
};

static inline struct tx6_esdhc_cfg *to_tx6_esdhc_cfg(struct fsl_esdhc_cfg *cfg)
{
	return container_of(cfg, struct tx6_esdhc_cfg, cfg);
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct tx6_esdhc_cfg *cfg = to_tx6_esdhc_cfg(mmc->priv);

	if (cfg->cd_gpio < 0)
		return cfg->cd_gpio;

	debug("SD card %d is %spresent\n",
		cfg - tx6qdl_esdhc_cfg,
		gpio_get_value(cfg->cd_gpio) ? "NOT " : "");
	return !gpio_get_value(cfg->cd_gpio);
}

int board_mmc_init(bd_t *bis)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tx6qdl_esdhc_cfg); i++) {
		struct mmc *mmc;
		struct tx6_esdhc_cfg *cfg = &tx6qdl_esdhc_cfg[i];
		int ret;

		if (i >= CONFIG_SYS_FSL_ESDHC_NUM)
			break;

		cfg->cfg.sdhc_clk = mxc_get_clock(cfg->clkid);
		imx_iomux_v3_setup_multiple_pads(cfg->pads, cfg->num_pads);

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

	imx_iomux_v3_setup_multiple_pads(tx6qdl_fec_pads, ARRAY_SIZE(tx6qdl_fec_pads));

	/* Deassert RESET to the external phy */
	gpio_set_value(TX6_FEC_RST_GPIO, 1);

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
		gpio_set_value(TX6_LED_GPIO, 1);
		led_state = LED_STATE_ON;
		blink_rate = calc_blink_rate(check_cpu_temperature(0));
	} else {
		if (get_timer(last) > blink_rate) {
			blink_rate = calc_blink_rate(check_cpu_temperature(0));
			last = get_timer_masked();
			if (led_state == LED_STATE_ON) {
				gpio_set_value(TX6_LED_GPIO, 0);
			} else {
				gpio_set_value(TX6_LED_GPIO, 1);
			}
			led_state = 1 - led_state;
		}
	}
}

static const iomux_v3_cfg_t stk5_pads[] = {
	/* SW controlled LED on STK5 baseboard */
	MX6_PAD_EIM_A18__GPIO_2_20,

	/* LCD data pins */
	MX6_PAD_DISP0_DAT0__IPU1_DISP0_DAT_0,
	MX6_PAD_DISP0_DAT1__IPU1_DISP0_DAT_1,
	MX6_PAD_DISP0_DAT2__IPU1_DISP0_DAT_2,
	MX6_PAD_DISP0_DAT3__IPU1_DISP0_DAT_3,
	MX6_PAD_DISP0_DAT4__IPU1_DISP0_DAT_4,
	MX6_PAD_DISP0_DAT5__IPU1_DISP0_DAT_5,
	MX6_PAD_DISP0_DAT6__IPU1_DISP0_DAT_6,
	MX6_PAD_DISP0_DAT7__IPU1_DISP0_DAT_7,
	MX6_PAD_DISP0_DAT8__IPU1_DISP0_DAT_8,
	MX6_PAD_DISP0_DAT9__IPU1_DISP0_DAT_9,
	MX6_PAD_DISP0_DAT10__IPU1_DISP0_DAT_10,
	MX6_PAD_DISP0_DAT11__IPU1_DISP0_DAT_11,
	MX6_PAD_DISP0_DAT12__IPU1_DISP0_DAT_12,
	MX6_PAD_DISP0_DAT13__IPU1_DISP0_DAT_13,
	MX6_PAD_DISP0_DAT14__IPU1_DISP0_DAT_14,
	MX6_PAD_DISP0_DAT15__IPU1_DISP0_DAT_15,
	MX6_PAD_DISP0_DAT16__IPU1_DISP0_DAT_16,
	MX6_PAD_DISP0_DAT17__IPU1_DISP0_DAT_17,
	MX6_PAD_DISP0_DAT18__IPU1_DISP0_DAT_18,
	MX6_PAD_DISP0_DAT19__IPU1_DISP0_DAT_19,
	MX6_PAD_DISP0_DAT20__IPU1_DISP0_DAT_20,
	MX6_PAD_DISP0_DAT21__IPU1_DISP0_DAT_21,
	MX6_PAD_DISP0_DAT22__IPU1_DISP0_DAT_22,
	MX6_PAD_DISP0_DAT23__IPU1_DISP0_DAT_23,
	MX6_PAD_DI0_PIN2__IPU1_DI0_PIN2, /* HSYNC */
	MX6_PAD_DI0_PIN3__IPU1_DI0_PIN3, /* VSYNC */
	MX6_PAD_DI0_PIN15__IPU1_DI0_PIN15, /* OE_ACD */
	MX6_PAD_DI0_DISP_CLK__IPU1_DI0_DISP_CLK, /* LSCLK */

	/* I2C bus on DIMM pins 40/41 */
	MX6_PAD_GPIO_6__I2C3_SDA,
	MX6_PAD_GPIO_3__I2C3_SCL,

	/* TSC200x PEN IRQ */
	MX6_PAD_EIM_D26__GPIO_3_26,

	/* EDT-FT5x06 Polytouch panel */
	MX6_PAD_NANDF_CS2__GPIO_6_15, /* IRQ */
	MX6_PAD_EIM_A16__GPIO_2_22, /* RESET */
	MX6_PAD_EIM_A17__GPIO_2_21, /* WAKE */

	/* USBH1 */
	MX6_PAD_EIM_D31__GPIO_3_31, /* VBUSEN */
	MX6_PAD_EIM_D30__GPIO_3_30, /* OC */
	/* USBOTG */
	MX6_PAD_EIM_D23__GPIO_3_23, /* USBOTG ID */
	MX6_PAD_GPIO_7__GPIO_1_7, /* VBUSEN */
	MX6_PAD_GPIO_8__GPIO_1_8, /* OC */
};

static const struct gpio stk5_gpios[] = {
	{ TX6_LED_GPIO, GPIOF_OUTPUT_INIT_LOW, "HEARTBEAT LED", },

	{ IMX_GPIO_NR(3, 23), GPIOF_INPUT, "USBOTG ID", },
	{ IMX_GPIO_NR(1, 8), GPIOF_INPUT, "USBOTG OC", },
	{ IMX_GPIO_NR(1, 7), GPIOF_OUTPUT_INIT_LOW, "USBOTG VBUS enable", },
	{ IMX_GPIO_NR(3, 30), GPIOF_INPUT, "USBH1 OC", },
	{ IMX_GPIO_NR(3, 31), GPIOF_OUTPUT_INIT_LOW, "USBH1 VBUS enable", },
};

#ifdef CONFIG_LCD
static u16 tx6_cmap[256];
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 1920,
	.vl_row = 1080,

	.vl_bpix = LCD_COLOR24,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
	.cmap = tx6_cmap,
};

static struct fb_videomode tx6_fb_modes[] = {
#ifndef CONFIG_SYS_LVDS_IF
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
		/* Emerging ET070001DM6 800 x 480 display.
		 * 152.4 mm x 91.44 mm display area.
		 */
		.name		= "ET070001DM6",
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
		.sync		= 0,
	},
#else
	{
		/* HannStar HSD100PXN1
		 * 202.7m mm x 152.06 mm display area.
		 */
		.name		= "HSD100PXN1",
		.refresh	= 60,
		.xres		= 1024,
		.yres		= 768,
		.pixclock	= KHZ2PICOS(65000),
		.left_margin	= 0,
		.hsync_len	= 0,
		.right_margin	= 320,
		.upper_margin	= 0,
		.vsync_len	= 0,
		.lower_margin	= 38,
		.sync		= FB_SYNC_CLK_LAT_FALL,
	},
#endif
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

	karo_load_splashimage(1);

	if (lcd_enabled) {
		debug("Switching LCD on\n");
		gpio_set_value(TX6_LCD_PWR_GPIO, 1);
		udelay(100);
		gpio_set_value(TX6_LCD_RST_GPIO, 1);
		udelay(300000);
		gpio_set_value(TX6_LCD_BACKLIGHT_GPIO, is_lvds());
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
		gpio_set_value(TX6_LCD_BACKLIGHT_GPIO, !is_lvds());
		gpio_set_value(TX6_LCD_RST_GPIO, 0);
		gpio_set_value(TX6_LCD_PWR_GPIO, 0);
	}
}

static const iomux_v3_cfg_t stk5_lcd_pads[] = {
	/* LCD RESET */
	MX6_PAD_EIM_D29__GPIO_3_29,
	/* LCD POWER_ENABLE */
	MX6_PAD_EIM_EB3__GPIO_2_31,
	/* LCD Backlight (PWM) */
	MX6_PAD_GPIO_1__GPIO_1_1,

	/* Display */
	MX6_PAD_DI0_DISP_CLK__IPU1_DI0_DISP_CLK,
	MX6_PAD_DI0_PIN15__IPU1_DI0_PIN15,
	MX6_PAD_DI0_PIN2__IPU1_DI0_PIN2,
	MX6_PAD_DI0_PIN3__IPU1_DI0_PIN3,
	MX6_PAD_DISP0_DAT0__IPU1_DISP0_DAT_0,
	MX6_PAD_DISP0_DAT1__IPU1_DISP0_DAT_1,
	MX6_PAD_DISP0_DAT2__IPU1_DISP0_DAT_2,
	MX6_PAD_DISP0_DAT3__IPU1_DISP0_DAT_3,
	MX6_PAD_DISP0_DAT4__IPU1_DISP0_DAT_4,
	MX6_PAD_DISP0_DAT5__IPU1_DISP0_DAT_5,
	MX6_PAD_DISP0_DAT6__IPU1_DISP0_DAT_6,
	MX6_PAD_DISP0_DAT7__IPU1_DISP0_DAT_7,
	MX6_PAD_DISP0_DAT8__IPU1_DISP0_DAT_8,
	MX6_PAD_DISP0_DAT9__IPU1_DISP0_DAT_9,
	MX6_PAD_DISP0_DAT10__IPU1_DISP0_DAT_10,
	MX6_PAD_DISP0_DAT11__IPU1_DISP0_DAT_11,
	MX6_PAD_DISP0_DAT12__IPU1_DISP0_DAT_12,
	MX6_PAD_DISP0_DAT13__IPU1_DISP0_DAT_13,
	MX6_PAD_DISP0_DAT14__IPU1_DISP0_DAT_14,
	MX6_PAD_DISP0_DAT15__IPU1_DISP0_DAT_15,
	MX6_PAD_DISP0_DAT16__IPU1_DISP0_DAT_16,
	MX6_PAD_DISP0_DAT17__IPU1_DISP0_DAT_17,
	MX6_PAD_DISP0_DAT18__IPU1_DISP0_DAT_18,
	MX6_PAD_DISP0_DAT19__IPU1_DISP0_DAT_19,
	MX6_PAD_DISP0_DAT20__IPU1_DISP0_DAT_20,
	MX6_PAD_DISP0_DAT21__IPU1_DISP0_DAT_21,
	MX6_PAD_DISP0_DAT22__IPU1_DISP0_DAT_22,
	MX6_PAD_DISP0_DAT23__IPU1_DISP0_DAT_23,
};

static const struct gpio stk5_lcd_gpios[] = {
	{ TX6_LCD_RST_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD RESET", },
	{ TX6_LCD_PWR_GPIO, GPIOF_OUTPUT_INIT_LOW, "LCD POWER", },
	{ TX6_LCD_BACKLIGHT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

void lcd_ctrl_init(void *lcdbase)
{
	int color_depth = 24;
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	const char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p = &tx6_fb_modes[0];
	struct fb_videomode fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;
	int pix_fmt;
	int lcd_bus_width;
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
					case 32:
					case 24:
						if (is_lvds())
							pix_fmt = IPU_PIX_FMT_LVDS888;
						/* fallthru */
					case 16:
					case 8:
						color_depth = val;
						break;

					case 18:
						if (is_lvds()) {
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
			if (*vm != '\0')
				vm++;
		}
	}
	if (p->xres == 0 || p->yres == 0) {
		printf("Invalid video mode: %s\n", getenv("video_mode"));
		lcd_enabled = 0;
		printf("Supported video modes are:");
		for (p = &tx6_fb_modes[0]; p->name != NULL; p++) {
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
		PICOS2KHZ(p->pixclock) / 1000, PICOS2KHZ(p->pixclock) % 1000);

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
		pix_fmt = is_lvds() ? IPU_PIX_FMT_LVDS888 : IPU_PIX_FMT_RGB24;
		break;

	case 18:
		pix_fmt = is_lvds() ? IPU_PIX_FMT_LVDS666 : IPU_PIX_FMT_RGB666;
		break;

	case 16:
		if (!is_lvds()) {
			pix_fmt = IPU_PIX_FMT_RGB565;
			break;
		}
		/* fallthru */
	default:
		lcd_enabled = 0;
		printf("Invalid %s bus width: %d\n", is_lvds() ? "LVDS" : "LCD",
			lcd_bus_width);
		return;
	}
	if (is_lvds()) {
		int lvds_mapping = karo_fdt_get_lvds_mapping(working_fdt, 0);
		int lvds_chan_mask = karo_fdt_get_lvds_channels(working_fdt);
		uint32_t gpr2;

		if (lvds_chan_mask == 0) {
			printf("No LVDS channel active\n");
			lcd_enabled = 0;
			return;
		}

		gpr2 = (lvds_mapping << 6) | (lvds_mapping << 8);
		if (lcd_bus_width == 24)
			gpr2 |= (1 << 5) | (1 << 7);
		gpr2 |= (lvds_chan_mask & 1) ? 1 << 0 : 0;
		gpr2 |= (lvds_chan_mask & 2) ? 3 << 2 : 0;
		debug("writing %08x to GPR2[%08x]\n", gpr2, IOMUXC_BASE_ADDR + 8);
		writel(gpr2, IOMUXC_BASE_ADDR + 8);
	}
	if (karo_load_splashimage(0) == 0) {
		int ret;

		debug("Initializing LCD controller\n");
		ret = ipuv3_fb_init(p, 0, pix_fmt, DI_PCLK_PLL3, di_clk_rate, -1);
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
	imx_iomux_v3_setup_pad(MX6_PAD_DISP0_DAT0__GPIO_4_21);
}

static void tx6qdl_set_cpu_clock(void)
{
	unsigned long cpu_clk = getenv_ulong("cpu_clk", 10, 0);

	if (had_ctrlc() || (wrsr & WRSR_TOUT))
		return;

	if (cpu_clk == 0 || cpu_clk == mxc_get_clock(MXC_ARM_CLK) / 1000000)
		return;

	if (mxc_set_clock(CONFIG_SYS_MX6_HCLK, cpu_clk, MXC_ARM_CLK) == 0) {
		cpu_clk = mxc_get_clock(MXC_ARM_CLK);
		printf("CPU clock set to %lu.%03lu MHz\n",
			cpu_clk / 1000000, cpu_clk / 1000 % 1000);
	} else {
		printf("Error: Failed to set CPU clock to %lu MHz\n", cpu_clk);
	}
}

static void tx6_init_mac(void)
{
	u8 mac[ETH_ALEN];

	imx_get_mac_from_fuse(-1, mac);
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

	tx6qdl_set_cpu_clock();
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
			const char *otg_mode = getenv("otg_mode");

			if (otg_mode && strcmp(otg_mode, "host") == 0) {
				printf("otg_mode='%s' is incompatible with baseboard %s; setting to 'none'\n",
					otg_mode, baseboard);
				setenv("otg_mode", "none");
			}
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

	gpio_set_value(TX6_RESET_OUT_GPIO, 1);
	clear_ctrlc();
	return ret;
}

int checkboard(void)
{
	u32 cpurev = get_cpu_rev();
	int cpu_variant = (cpurev >> 12) & 0xff;

	tx6qdl_print_cpuinfo();

	printf("Board: Ka-Ro TX6%c-%d%d1%d\n",
		cpu_variant == MXC_CPU_MX6Q ? 'Q' : 'U',
		cpu_variant == MXC_CPU_MX6Q ? 1 : 8,
		is_lvds(), 1 - PHYS_SDRAM_1_WIDTH / 64);

	return 0;
}

#ifdef CONFIG_SERIAL_TAG
void get_board_serial(struct tag_serialnr *serialnr)
{
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank0_regs *fuse = (void *)ocotp->bank[0].fuse_regs;

	serialnr->low = readl(&fuse->cfg0);
	serialnr->high = readl(&fuse->cfg1);
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
#include <jffs2/jffs2.h>
#include <mtd_node.h>
static struct node_info nodes[] = {
	{ "fsl,imx6q-gpmi-nand", MTD_DEV_TYPE_NAND, },
};
#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif

static const char *tx6_touchpanels[] = {
	"ti,tsc2007",
	"edt,edt-ft5x06",
	"eeti,egalax_ts",
};

void ft_board_setup(void *blob, bd_t *bd)
{
	const char *baseboard = getenv("baseboard");
	int stk5_v5 = baseboard != NULL && (strcmp(baseboard, "stk5-v5") == 0);
	const char *video_mode = karo_get_vmode(getenv("video_mode"));

	if (stk5_v5)
		karo_fdt_enable_node(blob, "stk5led", 0);

	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
	fdt_fixup_ethernet(blob);

	karo_fdt_fixup_touchpanel(blob, tx6_touchpanels,
				ARRAY_SIZE(tx6_touchpanels));
	karo_fdt_fixup_usb_otg(blob, "usbotg", "fsl,usbphy");
	karo_fdt_fixup_flexcan(blob, stk5_v5);

	karo_fdt_update_fb_mode(blob, video_mode);
}
#endif /* CONFIG_OF_BOARD_SETUP */
