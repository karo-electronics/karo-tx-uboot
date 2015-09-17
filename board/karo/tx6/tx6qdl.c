/*
 * Copyright (C) 2012-2015 Lothar Waßmann <LW@KARO-electronics.de>
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
#include <asm/arch/hab.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/sys_proto.h>

#include "../common/karo.h"
#include "pmic.h"

#define __data __attribute__((section(".data")))

#define TX6_FEC_RST_GPIO		IMX_GPIO_NR(7, 6)
#define TX6_FEC_PWR_GPIO		IMX_GPIO_NR(3, 20)
#define TX6_FEC_INT_GPIO		IMX_GPIO_NR(7, 1)
#define TX6_LED_GPIO			IMX_GPIO_NR(2, 20)

#define TX6_LCD_PWR_GPIO		IMX_GPIO_NR(2, 31)
#define TX6_LCD_RST_GPIO		IMX_GPIO_NR(3, 29)
#define TX6_LCD_BACKLIGHT_GPIO		IMX_GPIO_NR(1, 1)

#define TX6_RESET_OUT_GPIO		IMX_GPIO_NR(7, 12)
#define TX6_I2C1_SCL_GPIO		IMX_GPIO_NR(3, 21)
#define TX6_I2C1_SDA_GPIO		IMX_GPIO_NR(3, 28)

#ifdef CONFIG_MX6_TEMPERATURE_MIN
#define TEMPERATURE_MIN			CONFIG_MX6_TEMPERATURE_MIN
#else
#define TEMPERATURE_MIN			(-40)
#endif
#ifdef CONFIG_MX6_TEMPERATURE_HOT
#define TEMPERATURE_HOT			CONFIG_MX6_TEMPERATURE_HOT
#else
#define TEMPERATURE_HOT			80
#endif

DECLARE_GLOBAL_DATA_PTR;

#define MUX_CFG_SION			IOMUX_PAD(0, 0, IOMUX_CONFIG_SION, 0, 0, 0)

enum {
	MX6_PAD_DECL(GARBAGE, 0, 0, 0, 0, 0, 0)
};

char __uboot_img_end[0] __attribute__((section(".__uboot_img_end")));
#ifdef CONFIG_SECURE_BOOT
char __csf_data[0] __attribute__((section(".__csf_data")));
#endif

static const iomux_v3_cfg_t const tx6qdl_pads[] = {
	/* RESET_OUT */
	MX6_PAD_GPIO_17__GPIO7_IO12,

	/* UART pads */
#if CONFIG_MXC_UART_BASE == UART1_BASE
	MX6_PAD_SD3_DAT7__UART1_TX_DATA,
	MX6_PAD_SD3_DAT6__UART1_RX_DATA,
	MX6_PAD_SD3_DAT1__UART1_RTS_B,
	MX6_PAD_SD3_DAT0__UART1_CTS_B,
#endif
#if CONFIG_MXC_UART_BASE == UART2_BASE
	MX6_PAD_SD4_DAT4__UART2_RX_DATA,
	MX6_PAD_SD4_DAT7__UART2_TX_DATA,
	MX6_PAD_SD4_DAT5__UART2_RTS_B,
	MX6_PAD_SD4_DAT6__UART2_CTS_B,
#endif
#if CONFIG_MXC_UART_BASE == UART3_BASE
	MX6_PAD_EIM_D24__UART3_TX_DATA,
	MX6_PAD_EIM_D25__UART3_RX_DATA,
	MX6_PAD_SD3_RST__UART3_RTS_B,
	MX6_PAD_SD3_DAT3__UART3_CTS_B,
#endif
	/* internal I2C */
	MX6_PAD_EIM_D28__I2C1_SDA,
	MX6_PAD_EIM_D21__I2C1_SCL,

	/* FEC PHY GPIO functions */
	MX6_PAD_EIM_D20__GPIO3_IO20 | MUX_CFG_SION, /* PHY POWER */
	MX6_PAD_SD3_DAT2__GPIO7_IO06 | MUX_CFG_SION, /* PHY RESET */
	MX6_PAD_SD3_DAT4__GPIO7_IO01, /* PHY INT */
};

static const iomux_v3_cfg_t const tx6qdl_fec_pads[] = {
	/* FEC functions */
	MX6_PAD_ENET_MDC__ENET_MDC,
	MX6_PAD_ENET_MDIO__ENET_MDIO,
	MX6_PAD_GPIO_16__ENET_REF_CLK,
	MX6_PAD_ENET_RX_ER__ENET_RX_ER,
	MX6_PAD_ENET_CRS_DV__ENET_RX_EN,
	MX6_PAD_ENET_RXD1__ENET_RX_DATA1,
	MX6_PAD_ENET_RXD0__ENET_RX_DATA0,
	MX6_PAD_ENET_TX_EN__ENET_TX_EN,
	MX6_PAD_ENET_TXD1__ENET_TX_DATA1,
	MX6_PAD_ENET_TXD0__ENET_TX_DATA0,
};

#define TX6_I2C_GPIO_PAD_CTRL	(PAD_CTL_PUS_22K_UP |	\
				PAD_CTL_SPEED_MED |	\
				PAD_CTL_DSE_34ohm |	\
				PAD_CTL_SRE_FAST)

static const iomux_v3_cfg_t const tx6_i2c_gpio_pads[] = {
	/* internal I2C */
	MX6_PAD_EIM_D28__GPIO3_IO28 | MUX_CFG_SION | MUX_PAD_CTRL(TX6_I2C_GPIO_PAD_CTRL),
	MX6_PAD_EIM_D21__GPIO3_IO21 | MUX_CFG_SION | MUX_PAD_CTRL(TX6_I2C_GPIO_PAD_CTRL),
};

static const iomux_v3_cfg_t const tx6_i2c_pads[] = {
	/* internal I2C */
	MX6_PAD_EIM_D28__I2C1_SDA,
	MX6_PAD_EIM_D21__I2C1_SCL,
};

static const struct gpio const tx6qdl_gpios[] = {
	/* These two entries are used to forcefully reinitialize the I2C bus */
	{ TX6_I2C1_SCL_GPIO, GPIOFLAG_INPUT, "I2C1 SCL", },
	{ TX6_I2C1_SDA_GPIO, GPIOFLAG_INPUT, "I2C1 SDA", },

	{ TX6_RESET_OUT_GPIO, GPIOFLAG_OUTPUT_INIT_HIGH, "#RESET_OUT", },
	{ TX6_FEC_PWR_GPIO, GPIOFLAG_OUTPUT_INIT_HIGH, "FEC PHY PWR", },
	{ TX6_FEC_RST_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "FEC PHY RESET", },
	{ TX6_FEC_INT_GPIO, GPIOFLAG_INPUT, "FEC PHY INT", },
};

static int pmic_addr __data;

#if defined(CONFIG_SOC_MX6Q)
#define IOMUXC_SW_MUX_CTL_PAD_EIM_DATA21	0x020e00a4
#define IOMUXC_SW_MUX_CTL_PAD_EIM_DATA28	0x020e00c4
#define IOMUXC_SW_PAD_CTL_PAD_EIM_DATA21	0x020e03b8
#define IOMUXC_SW_PAD_CTL_PAD_EIM_DATA28	0x020e03d8
#define IOMUXC_SW_SEL_INPUT_PAD_EIM_DATA21	0x020e0898
#define IOMUXC_SW_SEL_INPUT_PAD_EIM_DATA28	0x020e089c
#define I2C1_SEL_INPUT_VAL			0
#endif
#if defined(CONFIG_SOC_MX6DL) || defined(CONFIG_SOC_MX6S)
#define IOMUXC_SW_MUX_CTL_PAD_EIM_DATA21	0x020e0158
#define IOMUXC_SW_MUX_CTL_PAD_EIM_DATA28	0x020e0174
#define IOMUXC_SW_PAD_CTL_PAD_EIM_DATA21	0x020e0528
#define IOMUXC_SW_PAD_CTL_PAD_EIM_DATA28	0x020e0544
#define IOMUXC_SW_SEL_INPUT_PAD_EIM_DATA21	0x020e0868
#define IOMUXC_SW_SEL_INPUT_PAD_EIM_DATA28	0x020e086c
#define I2C1_SEL_INPUT_VAL			1
#endif

#define GPIO_DR 0
#define GPIO_DIR 4
#define GPIO_PSR 8

static void tx6_i2c_recover(void)
{
	int i;
	int bad = 0;
#define SCL_BIT		(1 << (TX6_I2C1_SCL_GPIO % 32))
#define SDA_BIT		(1 << (TX6_I2C1_SDA_GPIO % 32))

	if ((readl(GPIO3_BASE_ADDR + GPIO_PSR) &
			(SCL_BIT | SDA_BIT)) == (SCL_BIT | SDA_BIT))
		return;

	debug("Clearing I2C bus\n");
	if (!(readl(GPIO3_BASE_ADDR + GPIO_PSR) & SCL_BIT)) {
		printf("I2C SCL stuck LOW\n");
		bad++;

		writel(readl(GPIO3_BASE_ADDR + GPIO_DR) | SCL_BIT,
			GPIO3_BASE_ADDR + GPIO_DR);
		writel(readl(GPIO3_BASE_ADDR + GPIO_DIR) | SCL_BIT,
			GPIO3_BASE_ADDR + GPIO_DIR);
	}
	if (!(readl(GPIO3_BASE_ADDR + GPIO_PSR) & SDA_BIT)) {
		printf("I2C SDA stuck LOW\n");
		bad++;

		writel(readl(GPIO3_BASE_ADDR + GPIO_DIR) & ~SDA_BIT,
			GPIO3_BASE_ADDR + GPIO_DIR);
		writel(readl(GPIO3_BASE_ADDR + GPIO_DR) | SCL_BIT,
			GPIO3_BASE_ADDR + GPIO_DR);
		writel(readl(GPIO3_BASE_ADDR + GPIO_DIR) | SCL_BIT,
			GPIO3_BASE_ADDR + GPIO_DIR);

		imx_iomux_v3_setup_multiple_pads(tx6_i2c_gpio_pads,
						ARRAY_SIZE(tx6_i2c_gpio_pads));
		udelay(10);

		for (i = 0; i < 18; i++) {
			u32 reg = readl(GPIO3_BASE_ADDR + GPIO_DR) ^ SCL_BIT;

			debug("%sing SCL\n", (reg & SCL_BIT) ? "Sett" : "Clear");
			writel(reg, GPIO3_BASE_ADDR + GPIO_DR);
			udelay(10);
			if (reg & SCL_BIT &&
				readl(GPIO3_BASE_ADDR + GPIO_PSR) & SDA_BIT)
				break;
		}
	}
	if (bad) {
		u32 reg = readl(GPIO3_BASE_ADDR + GPIO_PSR);

		if ((reg & (SCL_BIT | SDA_BIT)) == (SCL_BIT | SDA_BIT)) {
			printf("I2C bus recovery succeeded\n");
		} else {
			printf("I2C bus recovery FAILED: %08x:%08x\n", reg,
				SCL_BIT | SDA_BIT);
		}
	}
	debug("Setting up I2C Pads\n");
	imx_iomux_v3_setup_multiple_pads(tx6_i2c_pads,
					ARRAY_SIZE(tx6_i2c_pads));
}

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

static const char __data *tx6_mod_suffix;

int checkboard(void)
{
	u32 cpurev = get_cpu_rev();
	char *cpu_str = "?";

	switch ((cpurev >> 12) & 0xff) {
	case MXC_CPU_MX6SL:
		cpu_str = "SL";
		tx6_mod_suffix = "?";
		break;
	case MXC_CPU_MX6DL:
		cpu_str = "DL";
		tx6_mod_suffix = "U";
		break;
	case MXC_CPU_MX6SOLO:
		cpu_str = "SOLO";
		tx6_mod_suffix = "S";
		break;
	case MXC_CPU_MX6Q:
		cpu_str = "Q";
		tx6_mod_suffix = "Q";
		break;
	}

	printf("CPU:   Freescale i.MX6%s rev%d.%d at %d MHz\n",
		cpu_str,
		(cpurev & 0x000F0) >> 4,
		(cpurev & 0x0000F) >> 0,
		mxc_get_clock(MXC_ARM_CLK) / 1000000);

	print_reset_cause();
#ifdef CONFIG_MX6_TEMPERATURE_HOT
	check_cpu_temperature(1);
#endif
	tx6_i2c_recover();
	return 0;
}

int board_early_init_f(void)
{
	debug("%s@%d: \n", __func__, __LINE__);

	return 0;
}

#ifndef CONFIG_MX6_TEMPERATURE_HOT
static bool tx6_temp_check_enabled = true;
#else
#define tx6_temp_check_enabled	0
#endif

#ifdef CONFIG_TX6_NAND
#define TX6_FLASH_SZ	(CONFIG_SYS_NAND_BLOCKS / 1024 - 1)
#else
#ifdef CONFIG_MMC_BOOT_SIZE
#define TX6_FLASH_SZ	(CONFIG_MMC_BOOT_SIZE / 4096 + 2)
#else
#define TX6_FLASH_SZ	2
#endif
#endif /* CONFIG_TX6_NAND */

#define TX6_DDR_SZ	(ffs(PHYS_SDRAM_1_WIDTH / 16) - 1)

static char tx6_mem_table[] = {
	'4', /* 256MiB SDRAM 16bit; 128MiB NAND */
	'1', /* 512MiB SDRAM 32bit; 128MiB NAND */
	'0', /* 1GiB SDRAM 64bit; 128MiB NAND */
	'?', /* 256MiB SDRAM 16bit; 256MiB NAND */
	'?', /* 512MiB SDRAM 32bit; 256MiB NAND */
	'2', /* 1GiB SDRAM 64bit; 256MiB NAND */
	'?', /* 256MiB SDRAM 16bit; 4GiB eMMC */
	'5', /* 512MiB SDRAM 32bit; 4GiB eMMC */
	'3', /* 1GiB SDRAM 64bit; 4GiB eMMC */
	'?', /* 256MiB SDRAM 16bit; 8GiB eMMC */
	'?', /* 512MiB SDRAM 32bit; 8GiB eMMC */
	'0', /* 1GiB SDRAM 64bit; 8GiB eMMC */
};

static struct {
	uchar addr;
	uchar rev;
} tx6_mod_revs[] = {
	{ 0x3c, 1, },
	{ 0x32, 2, },
	{ 0x33, 3, },
};

static inline char tx6_mem_suffix(void)
{
	size_t mem_idx = (TX6_FLASH_SZ * 3) + TX6_DDR_SZ;

	debug("TX6_DDR_SZ=%d TX6_FLASH_SZ=%d idx=%d\n",
		TX6_DDR_SZ, TX6_FLASH_SZ, mem_idx);

	if (mem_idx >= ARRAY_SIZE(tx6_mem_table))
		return '?';

	return tx6_mem_table[mem_idx];
};

static int tx6_get_mod_rev(unsigned int pmic_id)
{
	if (pmic_id < ARRAY_SIZE(tx6_mod_revs))
		return tx6_mod_revs[pmic_id].rev;

	return 0;
}

static int tx6_pmic_probe(void)
{
	int i;

	debug("%s@%d: \n", __func__, __LINE__);

//	i2c_init_all();

	for (i = 0; i < ARRAY_SIZE(tx6_mod_revs); i++) {
		u8 i2c_addr = tx6_mod_revs[i].addr;
		int ret = i2c_probe(i2c_addr);

		if (ret == 0) {
			debug("I2C probe succeeded for addr 0x%02x\n", i2c_addr);
			return i;
		}
		debug("I2C probe returned %d for addr 0x%02x\n", ret, i2c_addr);
	}
	return -EINVAL;
}

static inline int __checkboard(void)
{
	u32 cpurev = get_cpu_rev();
	int cpu_variant = (cpurev >> 12) & 0xff;
	int pmic_id;

	debug("%s@%d: \n", __func__, __LINE__);

	pmic_id = tx6_pmic_probe();
	if (pmic_id >= 0)
		pmic_addr = tx6_mod_revs[pmic_id].addr;

	printf("Board: Ka-Ro TX6%s-%d%d%d%c\n",
		tx6_mod_suffix,
		cpu_variant == MXC_CPU_MX6Q ? 1 : 8,
		is_lvds(), tx6_get_mod_rev(pmic_id),
		tx6_mem_suffix());

	get_hab_status();

	debug("%s@%d: done\n", __func__, __LINE__);
	return 0;
}

int board_init(void)
{
	int ret;
	u32 cpurev = get_cpu_rev();
	int cpu_variant = (cpurev >> 12) & 0xff;
	int pmic_id;

	debug("%s@%d: \n", __func__, __LINE__);

	pmic_id = tx6_pmic_probe();
	if (pmic_id >= 0)
		pmic_addr = tx6_mod_revs[pmic_id].addr;

	printf("Board: Ka-Ro TX6%s-%d%d%d%c\n",
		tx6_mod_suffix,
		cpu_variant == MXC_CPU_MX6Q ? 1 : 8,
		is_lvds(), tx6_get_mod_rev(pmic_id),
		tx6_mem_suffix());

	get_hab_status();

	ret = gpio_request_array(tx6qdl_gpios, ARRAY_SIZE(tx6qdl_gpios));
	if (ret < 0) {
		printf("Failed to request tx6qdl_gpios: %d\n", ret);
	}
	imx_iomux_v3_setup_multiple_pads(tx6qdl_pads, ARRAY_SIZE(tx6qdl_pads));

	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x1000;
	gd->bd->bi_arch_number = -1;

	if (ctrlc() || (wrsr & WRSR_TOUT)) {
		if (wrsr & WRSR_TOUT)
			printf("WDOG RESET detected; Skipping PMIC setup\n");
		else
			printf("<CTRL-C> detected; safeboot enabled\n");
#ifndef CONFIG_MX6_TEMPERATURE_HOT
		tx6_temp_check_enabled = false;
#endif
		return 0;
	}

	ret = tx6_pmic_init(pmic_addr);
	if (ret) {
		printf("Failed to setup PMIC voltages: %d\n", ret);
		hang();
	}
	return 0;
}

int dram_init(void)
{
	debug("%s@%d: \n", __func__, __LINE__);

	/* dram_init must store complete ramsize in gd->ram_size */
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				PHYS_SDRAM_1_SIZE * CONFIG_NR_DRAM_BANKS);
	return 0;
}

void dram_init_banksize(void)
{
	debug("%s@%d: \n", __func__, __LINE__);

	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = get_ram_size((void *)PHYS_SDRAM_1,
			PHYS_SDRAM_1_SIZE);
#if CONFIG_NR_DRAM_BANKS > 1
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = get_ram_size((void *)PHYS_SDRAM_2,
			PHYS_SDRAM_2_SIZE);
#endif
}

#ifdef	CONFIG_FSL_ESDHC
#define SD_PAD_CTRL (PAD_CTL_PUS_47K_UP |		\
	PAD_CTL_SPEED_MED | PAD_CTL_DSE_40ohm |		\
	PAD_CTL_SRE_FAST)

static const iomux_v3_cfg_t mmc0_pads[] = {
	MX6_PAD_SD1_CMD__SD1_CMD | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD1_CLK__SD1_CLK | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD1_DAT0__SD1_DATA0 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD1_DAT1__SD1_DATA1 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD1_DAT2__SD1_DATA2 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD1_DAT3__SD1_DATA3 | MUX_PAD_CTRL(SD_PAD_CTRL),
	/* SD1 CD */
	MX6_PAD_SD3_CMD__GPIO7_IO02,
};

static const iomux_v3_cfg_t mmc1_pads[] = {
	MX6_PAD_SD2_CMD__SD2_CMD | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD2_CLK__SD2_CLK | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD2_DAT0__SD2_DATA0 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD2_DAT1__SD2_DATA1 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD2_DAT2__SD2_DATA2 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD2_DAT3__SD2_DATA3 | MUX_PAD_CTRL(SD_PAD_CTRL),
	/* SD2 CD */
	MX6_PAD_SD3_CLK__GPIO7_IO03,
};

#ifdef CONFIG_TX6_EMMC
static const iomux_v3_cfg_t mmc3_pads[] = {
	MX6_PAD_SD4_CMD__SD4_CMD | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD4_CLK__SD4_CLK | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD4_DAT0__SD4_DATA0 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD4_DAT1__SD4_DATA1 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD4_DAT2__SD4_DATA2 | MUX_PAD_CTRL(SD_PAD_CTRL),
	MX6_PAD_SD4_DAT3__SD4_DATA3 | MUX_PAD_CTRL(SD_PAD_CTRL),
	/* eMMC RESET */
	MX6_PAD_NANDF_ALE__SD4_RESET | MUX_PAD_CTRL(PAD_CTL_PUS_47K_UP |
						PAD_CTL_DSE_40ohm),
};
#endif

static struct tx6_esdhc_cfg {
	const iomux_v3_cfg_t *pads;
	int num_pads;
	enum mxc_clock clkid;
	struct fsl_esdhc_cfg cfg;
	int cd_gpio;
} tx6qdl_esdhc_cfg[] = {
#ifdef CONFIG_TX6_EMMC
	{
		.pads = mmc3_pads,
		.num_pads = ARRAY_SIZE(mmc3_pads),
		.clkid = MXC_ESDHC4_CLK,
		.cfg = {
			.esdhc_base = (void __iomem *)USDHC4_BASE_ADDR,
			.max_bus_width = 4,
		},
		.cd_gpio = -EINVAL,
	},
#endif
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
		return 1;

	debug("SD card %d is %spresent (GPIO %d)\n",
		cfg - tx6qdl_esdhc_cfg,
		gpio_get_value(cfg->cd_gpio) ? "NOT " : "",
		cfg->cd_gpio);
	return !gpio_get_value(cfg->cd_gpio);
}

int board_mmc_init(bd_t *bis)
{
	int i;

	debug("%s@%d: \n", __func__, __LINE__);

	for (i = 0; i < ARRAY_SIZE(tx6qdl_esdhc_cfg); i++) {
		struct mmc *mmc;
		struct tx6_esdhc_cfg *cfg = &tx6qdl_esdhc_cfg[i];
		int ret;

		cfg->cfg.sdhc_clk = mxc_get_clock(cfg->clkid);
		imx_iomux_v3_setup_multiple_pads(cfg->pads, cfg->num_pads);

		if (cfg->cd_gpio >= 0) {
			ret = gpio_request_one(cfg->cd_gpio,
					GPIOFLAG_INPUT, "MMC CD");
			if (ret) {
				printf("Error %d requesting GPIO%d_%d\n",
					ret, cfg->cd_gpio / 32, cfg->cd_gpio % 32);
				continue;
			}
		}

		debug("%s: Initializing MMC slot %d\n", __func__, i);
		fsl_esdhc_initialize(bis, &cfg->cfg);

		mmc = find_mmc_device(i);
		if (mmc == NULL)
			continue;
		if (board_mmc_getcd(mmc))
			mmc_init(mmc);
	}
	return 0;
}
#endif /* CONFIG_CMD_MMC */

#ifdef CONFIG_FEC_MXC

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif

int board_eth_init(bd_t *bis)
{
	int ret;

	debug("%s@%d: \n", __func__, __LINE__);

	/* delay at least 21ms for the PHY internal POR signal to deassert */
	udelay(22000);

	imx_iomux_v3_setup_multiple_pads(tx6qdl_fec_pads,
					ARRAY_SIZE(tx6qdl_fec_pads));

	/* Deassert RESET to the external phy */
	gpio_set_value(TX6_FEC_RST_GPIO, 1);

	ret = cpu_eth_init(bis);
	if (ret)
		printf("cpu_eth_init() failed: %d\n", ret);

	return ret;
}

static void tx6_init_mac(void)
{
	u8 mac[ETH_ALEN];

	imx_get_mac_from_fuse(-1, mac);
	if (!is_valid_ethaddr(mac)) {
		printf("No valid MAC address programmed\n");
		return;
	}

	printf("MAC addr from fuse: %pM\n", mac);
	eth_setenv_enetaddr("ethaddr", mac);
}
#else
static inline void tx6_init_mac(void)
{
}
#endif /* CONFIG_FEC_MXC */

enum {
	LED_STATE_INIT = -1,
	LED_STATE_OFF,
	LED_STATE_ON,
};

static inline int calc_blink_rate(void)
{
	if (!tx6_temp_check_enabled)
		return CONFIG_SYS_HZ;

	return CONFIG_SYS_HZ + CONFIG_SYS_HZ / 10 -
		(check_cpu_temperature(0) - TEMPERATURE_MIN) * CONFIG_SYS_HZ /
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
		blink_rate = calc_blink_rate();
	} else {
		if (get_timer(last) > blink_rate) {
			blink_rate = calc_blink_rate();
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
	MX6_PAD_EIM_A18__GPIO2_IO20,

	/* I2C bus on DIMM pins 40/41 */
	MX6_PAD_GPIO_6__I2C3_SDA,
	MX6_PAD_GPIO_3__I2C3_SCL,

	/* TSC200x PEN IRQ */
	MX6_PAD_EIM_D26__GPIO3_IO26,

	/* EDT-FT5x06 Polytouch panel */
	MX6_PAD_NANDF_CS2__GPIO6_IO15, /* IRQ */
	MX6_PAD_EIM_A16__GPIO2_IO22, /* RESET */
	MX6_PAD_EIM_A17__GPIO2_IO21, /* WAKE */

	/* USBH1 */
	MX6_PAD_EIM_D31__GPIO3_IO31, /* VBUSEN */
	MX6_PAD_EIM_D30__GPIO3_IO30, /* OC */
	/* USBOTG */
	MX6_PAD_EIM_D23__GPIO3_IO23, /* USBOTG ID */
	MX6_PAD_GPIO_7__GPIO1_IO07, /* VBUSEN */
	MX6_PAD_GPIO_8__GPIO1_IO08, /* OC */
};

static const struct gpio stk5_gpios[] = {
	{ TX6_LED_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "HEARTBEAT LED", },

	{ IMX_GPIO_NR(3, 23), GPIOFLAG_INPUT, "USBOTG ID", },
	{ IMX_GPIO_NR(1, 8), GPIOFLAG_INPUT, "USBOTG OC", },
	{ IMX_GPIO_NR(1, 7), GPIOFLAG_OUTPUT_INIT_LOW, "USBOTG VBUS enable", },
	{ IMX_GPIO_NR(3, 30), GPIOFLAG_INPUT, "USBH1 OC", },
	{ IMX_GPIO_NR(3, 31), GPIOFLAG_OUTPUT_INIT_LOW, "USBH1 VBUS enable", },
};

#ifdef CONFIG_LCD
static u16 tx6_cmap[256];
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 1920,
	.vl_row = 1080,

	.vl_bpix = LCD_COLOR32,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
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
static int lcd_bl_polarity;

static int lcd_backlight_polarity(void)
{
	return lcd_bl_polarity;
}

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
		gpio_set_value(TX6_LCD_PWR_GPIO, 1);
		udelay(100);
		gpio_set_value(TX6_LCD_RST_GPIO, 1);
		udelay(300000);
		gpio_set_value(TX6_LCD_BACKLIGHT_GPIO,
			lcd_backlight_polarity());
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
		gpio_set_value(TX6_LCD_BACKLIGHT_GPIO,
			!lcd_backlight_polarity());
		gpio_set_value(TX6_LCD_RST_GPIO, 0);
		gpio_set_value(TX6_LCD_PWR_GPIO, 0);
	}
}

static const iomux_v3_cfg_t stk5_lcd_pads[] = {
	/* LCD RESET */
	MX6_PAD_EIM_D29__GPIO3_IO29,
	/* LCD POWER_ENABLE */
	MX6_PAD_EIM_EB3__GPIO2_IO31,
	/* LCD Backlight (PWM) */
	MX6_PAD_GPIO_1__GPIO1_IO01,

#ifndef CONFIG_SYS_LVDS_IF
	/* Display */
	MX6_PAD_DISP0_DAT0__IPU1_DISP0_DATA00,
	MX6_PAD_DISP0_DAT1__IPU1_DISP0_DATA01,
	MX6_PAD_DISP0_DAT2__IPU1_DISP0_DATA02,
	MX6_PAD_DISP0_DAT3__IPU1_DISP0_DATA03,
	MX6_PAD_DISP0_DAT4__IPU1_DISP0_DATA04,
	MX6_PAD_DISP0_DAT5__IPU1_DISP0_DATA05,
	MX6_PAD_DISP0_DAT6__IPU1_DISP0_DATA06,
	MX6_PAD_DISP0_DAT7__IPU1_DISP0_DATA07,
	MX6_PAD_DISP0_DAT8__IPU1_DISP0_DATA08,
	MX6_PAD_DISP0_DAT9__IPU1_DISP0_DATA09,
	MX6_PAD_DISP0_DAT10__IPU1_DISP0_DATA10,
	MX6_PAD_DISP0_DAT11__IPU1_DISP0_DATA11,
	MX6_PAD_DISP0_DAT12__IPU1_DISP0_DATA12,
	MX6_PAD_DISP0_DAT13__IPU1_DISP0_DATA13,
	MX6_PAD_DISP0_DAT14__IPU1_DISP0_DATA14,
	MX6_PAD_DISP0_DAT15__IPU1_DISP0_DATA15,
	MX6_PAD_DISP0_DAT16__IPU1_DISP0_DATA16,
	MX6_PAD_DISP0_DAT17__IPU1_DISP0_DATA17,
	MX6_PAD_DISP0_DAT18__IPU1_DISP0_DATA18,
	MX6_PAD_DISP0_DAT19__IPU1_DISP0_DATA19,
	MX6_PAD_DISP0_DAT20__IPU1_DISP0_DATA20,
	MX6_PAD_DISP0_DAT21__IPU1_DISP0_DATA21,
	MX6_PAD_DISP0_DAT22__IPU1_DISP0_DATA22,
	MX6_PAD_DISP0_DAT23__IPU1_DISP0_DATA23,
	MX6_PAD_DI0_PIN2__IPU1_DI0_PIN02, /* HSYNC */
	MX6_PAD_DI0_PIN3__IPU1_DI0_PIN03, /* VSYNC */
	MX6_PAD_DI0_PIN15__IPU1_DI0_PIN15, /* OE_ACD */
	MX6_PAD_DI0_DISP_CLK__IPU1_DI0_DISP_CLK, /* LSCLK */
#endif
};

static const struct gpio stk5_lcd_gpios[] = {
	{ TX6_LCD_RST_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "LCD RESET", },
	{ TX6_LCD_PWR_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "LCD POWER", },
	{ TX6_LCD_BACKLIGHT_GPIO, GPIOFLAG_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
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
	lcd_bl_polarity = karo_fdt_get_backlight_polarity(working_fdt);

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
		panel_info.vl_bpix = LCD_COLOR32;
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
		uint32_t gpr3;

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

		gpr3 = readl(IOMUXC_BASE_ADDR + 0xc);
		gpr3 &= ~((3 << 8) | (3 << 6));
		writel(gpr3, IOMUXC_BASE_ADDR + 0xc);
	}
	if (karo_load_splashimage(0) == 0) {
		int ret;

		debug("Initializing LCD controller\n");
		ret = ipuv3_fb_init(p, 0, pix_fmt,
				is_lvds() ? DI_PCLK_LDB : DI_PCLK_PLL3,
				di_clk_rate, -1);
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
	int ret;

	ret = gpio_request_array(stk5_gpios, ARRAY_SIZE(stk5_gpios));
	if (ret < 0) {
		printf("Failed to request stk5_gpios: %d\n", ret);
		return;
	}
	imx_iomux_v3_setup_multiple_pads(stk5_pads, ARRAY_SIZE(stk5_pads));
}

static void stk5v3_board_init(void)
{
	stk5_board_init();
}

static void stk5v5_board_init(void)
{
	int ret;

	stk5_board_init();

	ret = gpio_request_one(IMX_GPIO_NR(4, 21), GPIOFLAG_OUTPUT_INIT_HIGH,
			"Flexcan Transceiver");
	if (ret) {
		printf("Failed to request Flexcan Transceiver GPIO: %d\n", ret);
		return;
	}

	imx_iomux_v3_setup_pad(MX6_PAD_DISP0_DAT0__GPIO4_IO21);
}

static void tx6qdl_set_cpu_clock(void)
{
	unsigned long cpu_clk = getenv_ulong("cpu_clk", 10, 0);

	if (cpu_clk == 0 || cpu_clk == mxc_get_clock(MXC_ARM_CLK) / 1000000)
		return;

	if (had_ctrlc() || (wrsr & WRSR_TOUT)) {
		printf("%s detected; skipping cpu clock change\n",
			(wrsr & WRSR_TOUT) ? "WDOG RESET" : "<CTRL-C>");
		return;
	}
	if (mxc_set_clock(CONFIG_SYS_MX6_HCLK, cpu_clk, MXC_ARM_CLK) == 0) {
		cpu_clk = mxc_get_clock(MXC_ARM_CLK);
		printf("CPU clock set to %lu.%03lu MHz\n",
			cpu_clk / 1000000, cpu_clk / 1000 % 1000);
	} else {
		printf("Error: Failed to set CPU clock to %lu MHz\n", cpu_clk);
	}
}

int board_late_init(void)
{
	int ret = 0;
	const char *baseboard;
#if 1
	/* override secure_boot fuse */
	struct ocotp_regs *ocotp = (struct ocotp_regs *)OCOTP_BASE_ADDR;
	struct fuse_bank0_regs *fuse = (void *)ocotp->bank[0].fuse_regs;

	writel(0x12, &fuse->cfg5);
#endif

	debug("%s@%d: \n", __func__, __LINE__);

	env_cleanup();

	if (tx6_temp_check_enabled)
		check_cpu_temperature(1);

	tx6qdl_set_cpu_clock();

	if (had_ctrlc())
		setenv_ulong("safeboot", 1);
	else if (wrsr & WRSR_TOUT)
		setenv_ulong("wdreset", 1);
	else
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

int ft_board_setup(void *blob, bd_t *bd)
{
	const char *baseboard = getenv("baseboard");
	int stk5_v5 = baseboard != NULL && (strcmp(baseboard, "stk5-v5") == 0);
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	int ret;

	ret = fdt_increase_size(blob, 4096);
	if (ret) {
		printf("Failed to increase FDT size: %s\n", fdt_strerror(ret));
		return ret;
	}
	if (stk5_v5)
		karo_fdt_enable_node(blob, "stk5led", 0);

	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
	fdt_fixup_ethernet(blob);

	karo_fdt_fixup_touchpanel(blob, tx6_touchpanels,
				ARRAY_SIZE(tx6_touchpanels));
	karo_fdt_fixup_usb_otg(blob, "usbotg", "fsl,usbphy", "vbus-supply");
	karo_fdt_fixup_flexcan(blob, stk5_v5);

	karo_fdt_update_fb_mode(blob, video_mode);

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */
