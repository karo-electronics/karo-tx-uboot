// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
#include <errno.h>
#include <fdtdec.h>
#include <fsl_esdhc_imx.h>
#include <hang.h>
#include <i2c.h>
#include <mmc.h>
#include <spl.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#if defined(CONFIG_IMX8MM)
#include <asm/arch/imx8mm_pins.h>
#elif defined(CONFIG_IMX8MN)
#include <asm/arch/imx8mn_pins.h>
#else
#error Invalid SOC type selection
#endif
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <dm/device.h>
#include <dm/device_compat.h>
#include <dm/ofnode.h>
#include <dm/read.h>
#include <power/pmic.h>
#include <asm/arch/ddr.h>

DECLARE_GLOBAL_DATA_PTR;

#define UART_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_FSEL1 |	\
					     PAD_CTL_DSE6)

#if defined(CONFIG_IMX8MM)
static const iomux_v3_cfg_t uart_pads[] = {
	IMX8MM_PAD_UART1_RXD_UART1_RX | UART_PAD_CTRL,
	IMX8MM_PAD_UART1_TXD_UART1_TX | UART_PAD_CTRL,
};
#elif defined(CONFIG_IMX8MN)
static const iomux_v3_cfg_t uart_pads[] = {
	IMX8MN_PAD_UART1_RXD__UART1_DCE_RX | UART_PAD_CTRL,
	IMX8MN_PAD_UART1_TXD__UART1_DCE_TX | UART_PAD_CTRL,
};
#endif

/* called before debug_uart is initialized */
int board_early_init_f(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));
	init_uart_clk(0);

	return 0;
}

/* called after debug_uart initialization */
enum boot_device spl_board_boot_device(enum boot_device boot_device_spl)
{
	enum boot_device ret;

	debug("%s@%d: boot_device_spl=%d ", __func__, __LINE__,
	      boot_device_spl);

	switch (boot_device_spl) {
	case SD1_BOOT: // 6
	case MMC1_BOOT: // 10
		ret = BOOT_DEVICE_MMC1; // 1
		break;
	case SD2_BOOT: // 7
	case MMC2_BOOT: // 11
		ret = BOOT_DEVICE_MMC2; // 2
		break;
	case SD3_BOOT: // 8
	case MMC3_BOOT: // 12
		ret = BOOT_DEVICE_MMC2; // 3
		break;
	case USB_BOOT: // 17
		ret = BOOT_DEVICE_BOARD; // 12
		break;
	default:
		ret = BOOT_DEVICE_NONE; // 16
	}

	debug("-> %d\n", ret);
	return ret;
}

void spl_dram_init(void)
{
	debug("%s@%d:\n", __func__, __LINE__);
	ddr_init(&dram_timing);
}

#ifdef CONFIG_SPL_BOARD_INIT
void spl_board_init(void)
{
	if (IS_ENABLED(CONFIG_SPL_BANNER_PRINT))
		puts("Normal Boot\n");
}
#endif

#define I2C_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_PE |	\
					     PAD_CTL_HYS |	\
					     PAD_CTL_PUE |	\
					     PAD_CTL_DSE6)

#if defined(CONFIG_IMX8MM)
struct i2c_pads_info i2c_pad_info[] = {
	{
		.scl = {
			.i2c_mode = IMX8MM_PAD_I2C1_SCL_I2C1_SCL | I2C_PAD_CTRL,
			.gpio_mode = IMX8MM_PAD_I2C1_SCL_GPIO5_IO14 | I2C_PAD_CTRL,
			.gp = IMX_GPIO_NR(5, 14),
		},
		.sda = {
			.i2c_mode = IMX8MM_PAD_I2C1_SDA_I2C1_SDA | I2C_PAD_CTRL,
			.gpio_mode = IMX8MM_PAD_I2C1_SDA_GPIO5_IO15 | I2C_PAD_CTRL,
			.gp = IMX_GPIO_NR(5, 15),
		},
	},
};
#elif defined(CONFIG_IMX8MN)
struct i2c_pads_info i2c_pad_info[] = {
	{
		.scl = {
			.i2c_mode = IMX8MN_PAD_I2C1_SCL__I2C1_SCL | I2C_PAD_CTRL,
			.gpio_mode = IMX8MN_PAD_I2C1_SCL__GPIO5_IO14 | I2C_PAD_CTRL,
			.gp = IMX_GPIO_NR(5, 14),
		},
		.sda = {
			.i2c_mode = IMX8MN_PAD_I2C1_SDA__I2C1_SDA | I2C_PAD_CTRL,
			.gpio_mode = IMX8MN_PAD_I2C1_SDA__GPIO5_IO15 | I2C_PAD_CTRL,
			.gp = IMX_GPIO_NR(5, 15),
		},
	},
};
#endif

#if !CONFIG_IS_ENABLED(DM_MMC) && CONFIG_IS_ENABLED(MMC_SUPPORT)
#define USDHC_GPIO_PAD_CTRL	MUX_PAD_CTRL(PAD_CTL_PE |	\
					     PAD_CTL_PUE |	\
					     PAD_CTL_FSEL2 |	\
					     PAD_CTL_DSE6)

#define USDHC_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_PE |	\
					     PAD_CTL_HYS |	\
					     PAD_CTL_PUE |	\
					     PAD_CTL_FSEL2 |	\
					     PAD_CTL_DSE0)

#if defined(CONFIG_IMX8MM)
static const iomux_v3_cfg_t tx8m_usdhc1_pads[] = {
	IMX8MM_PAD_SD1_CLK_USDHC1_CLK | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_CMD_USDHC1_CMD | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA0_USDHC1_DATA0 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA1_USDHC1_DATA1 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA2_USDHC1_DATA2 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA3_USDHC1_DATA3 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA4_USDHC1_DATA4 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA5_USDHC1_DATA5 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA6_USDHC1_DATA6 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_DATA7_USDHC1_DATA7 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD1_STROBE_USDHC1_STROBE | USDHC_GPIO_PAD_CTRL,
	IMX8MM_PAD_SD1_RESET_B_USDHC1_RESET_B | USDHC_GPIO_PAD_CTRL,
};

static const iomux_v3_cfg_t tx8m_usdhc2_pads[] = {
	IMX8MM_PAD_SD2_CLK_USDHC2_CLK | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD2_CMD_USDHC2_CMD | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD2_DATA0_USDHC2_DATA0 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD2_DATA1_USDHC2_DATA1 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD2_DATA2_USDHC2_DATA2 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD2_DATA3_USDHC2_DATA3 | USDHC_PAD_CTRL,
	IMX8MM_PAD_SD2_CD_B_GPIO2_IO12 | USDHC_GPIO_PAD_CTRL,
};

#ifndef CONFIG_KARO_QSXM
static const iomux_v3_cfg_t tx8m_usdhc3_pads[] = {
	IMX8MM_PAD_NAND_WE_B_USDHC3_CLK | USDHC_PAD_CTRL,
	IMX8MM_PAD_NAND_WP_B_USDHC3_CMD | USDHC_PAD_CTRL,
	IMX8MM_PAD_NAND_DATA04_USDHC3_DATA0 | USDHC_PAD_CTRL,
	IMX8MM_PAD_NAND_DATA05_USDHC3_DATA1 | USDHC_PAD_CTRL,
	IMX8MM_PAD_NAND_DATA06_USDHC3_DATA2 | USDHC_PAD_CTRL,
	IMX8MM_PAD_NAND_DATA07_USDHC3_DATA3 | USDHC_PAD_CTRL,
	IMX8MM_PAD_NAND_DATA02_GPIO3_IO8 | USDHC_GPIO_PAD_CTRL,
};
#endif
#elif defined(CONFIG_IMX8MN)
static const iomux_v3_cfg_t tx8m_usdhc1_pads[] = {
	IMX8MN_PAD_SD1_CLK__USDHC1_CLK | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_CMD__USDHC1_CMD | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA0__USDHC1_DATA0 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA1__USDHC1_DATA1 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA2__USDHC1_DATA2 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA3__USDHC1_DATA3 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA4__USDHC1_DATA4 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA5__USDHC1_DATA5 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA6__USDHC1_DATA6 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_DATA7__USDHC1_DATA7 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD1_STROBE__USDHC1_STROBE | USDHC_GPIO_PAD_CTRL,
	IMX8MN_PAD_SD1_RESET_B__USDHC1_RESET_B | USDHC_GPIO_PAD_CTRL,
};

static const iomux_v3_cfg_t tx8m_usdhc2_pads[] = {
	IMX8MN_PAD_SD2_CLK__USDHC2_CLK | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD2_CMD__USDHC2_CMD | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD2_DATA0__USDHC2_DATA0 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD2_DATA1__USDHC2_DATA1 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD2_DATA2__USDHC2_DATA2 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD2_DATA3__USDHC2_DATA3 | USDHC_PAD_CTRL,
	IMX8MN_PAD_SD2_CD_B__GPIO2_IO12 | USDHC_GPIO_PAD_CTRL,
};

static const iomux_v3_cfg_t tx8m_usdhc3_pads[] = {
	IMX8MN_PAD_NAND_WE_B__USDHC3_CLK | USDHC_PAD_CTRL,
	IMX8MN_PAD_NAND_WP_B__USDHC3_CMD | USDHC_PAD_CTRL,
	IMX8MN_PAD_NAND_DATA04__USDHC3_DATA0 | USDHC_PAD_CTRL,
	IMX8MN_PAD_NAND_DATA05__USDHC3_DATA1 | USDHC_PAD_CTRL,
	IMX8MN_PAD_NAND_DATA06__USDHC3_DATA2 | USDHC_PAD_CTRL,
	IMX8MN_PAD_NAND_DATA07__USDHC3_DATA3 | USDHC_PAD_CTRL,
	IMX8MN_PAD_NAND_DATA02__GPIO3_IO8 | USDHC_GPIO_PAD_CTRL,
};
#endif /* CONFIG_IMX8MM */

static struct tx8m_esdhc_cfg {
	struct fsl_esdhc_cfg cfg;
	int clk;
	const iomux_v3_cfg_t *pads;
	size_t num_pads;
	int cd_gpio;
} tx8m_sdhc_cfgs[] = {
	{
		.cfg = {
			.esdhc_base = USDHC1_BASE_ADDR,
			.max_bus_width = 8,
		},
		.clk = MXC_ESDHC_CLK,
		.pads = tx8m_usdhc1_pads,
		.num_pads = ARRAY_SIZE(tx8m_usdhc1_pads),
		.cd_gpio = -EINVAL,
	},
	{
		.cfg = {
			.esdhc_base = USDHC2_BASE_ADDR,
			.max_bus_width = 4,
		},
		.clk = MXC_ESDHC2_CLK,
		.pads = tx8m_usdhc2_pads,
		.num_pads = ARRAY_SIZE(tx8m_usdhc2_pads),
		.cd_gpio = IMX_GPIO_NR(2, 12),
	},
#ifndef CONFIG_KARO_QSXM
	{
		.cfg = {
			.esdhc_base = USDHC3_BASE_ADDR,
			.max_bus_width = 4,
		},
		.clk = MXC_ESDHC3_CLK,
		.pads = tx8m_usdhc3_pads,
		.num_pads = ARRAY_SIZE(tx8m_usdhc3_pads),
		.cd_gpio = IMX_GPIO_NR(3, 8),
	},
#endif
};

int board_mmc_init(bd_t *bis)
{
	int ret;
	/*
	 * According to the board_mmc_init() the following map is done:
	 * (U-Boot device node)    (Physical Port)
	 * mmc0                    USDHC1 (eMMC)
	 * mmc1                    USDHC2
	 * mmc2                    USDHC3
	 */

	for (size_t i = 0; i < CONFIG_SYS_FSL_USDHC_NUM; i++) {
		struct mmc *mmc;
		struct tx8m_esdhc_cfg *cfg;

		if (i >= ARRAY_SIZE(tx8m_sdhc_cfgs)) {
			printf("Warning: more USDHC controllers configured (%u) than supported by the board: %zu\n",
			       CONFIG_SYS_FSL_USDHC_NUM,
			       ARRAY_SIZE(tx8m_sdhc_cfgs));
			return -EINVAL;
		}

		cfg = &tx8m_sdhc_cfgs[i];
		cfg->cfg.sdhc_clk = mxc_get_clock(cfg->clk);
		imx_iomux_v3_setup_multiple_pads(cfg->pads, cfg->num_pads);

		ret = fsl_esdhc_initialize(bis, &cfg->cfg);
		if (ret) {
			printf("Failed to initialize MMC%zu: %d\n", i, ret);
			continue;
		}

		mmc = find_mmc_device(i);
		if (!mmc) {
			printf("mmc device %zi not found\n", i);
			continue;
		}
		if (board_mmc_getcd(mmc)) {
			ret = mmc_init(mmc);
			if (ret && ret != -EOPNOTSUPP)
				printf("%s@%d: mmc_init(mmc%zi) failed: %d\n",
				       __func__, __LINE__, i, ret);
		} else {
			debug("No Medium found in MMC slot %zi\n", i);
		}
	}
	return 0;
}

static inline struct tx8m_esdhc_cfg *to_tx8m_esdhc_cfg(struct fsl_esdhc_cfg *priv)
{
	for (size_t i = 0; i < ARRAY_SIZE(tx8m_sdhc_cfgs); i++) {
		struct tx8m_esdhc_cfg *cfg = &tx8m_sdhc_cfgs[i];

		if (priv->esdhc_base == cfg->cfg.esdhc_base)
			return cfg;
	}
	return NULL;
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct tx8m_esdhc_cfg *cfg = to_tx8m_esdhc_cfg(mmc->priv);

	if (!cfg) {
		printf("Failed to lookup CD GPIO for MMC dev %p\n", mmc->priv);
		return 0;
	}
	if (cfg->cd_gpio < 0) {
		debug("%s@%d: 1\n", __func__, __LINE__);
		return 1;
	}
	debug("%s@%d: %d\n", __func__, __LINE__,
	      !gpio_get_value(cfg->cd_gpio));

	return !gpio_get_value(cfg->cd_gpio);
}
#endif

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	debug("%s: %s\n", __func__, name);
	return 0;
}
#endif

#if !IS_ENABLED(CONFIG_DM_I2C)
static void scan_i2c(int bus)
{
	int addr;
	int ret;

return;

	ret = i2c_set_bus_num(bus);
	if (ret) {
		printf("Failed to activate I2C bus %d: %d\n", bus, ret);
		return;
	}
	for (addr = 3; addr < 0x7c; addr++) {
		ret = i2c_probe(addr);
		if (ret != -EREMOTEIO)
			printf("I2C probe(%02x) returned %d\n", addr, ret);
	}
}

static void tx8m_spl_i2c_setup(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(i2c_pad_info); i++) {
		setup_i2c(i, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info[i]);
		scan_i2c(i);
	}
}
#else
static inline void tx8m_spl_i2c_setup(void)
{
}
#endif

#ifndef CONFIG_SPL_FRAMEWORK_BOARD_INIT_F
void board_init_f(ulong dummy)
{
	int ret;

	/* Clear the BSS. */
	memset(__bss_start, 0, __bss_end - __bss_start);

	arch_cpu_init();

	board_early_init_f();

	timer_init();

	preloader_console_init();

	ret = spl_init();
	if (ret) {
		debug("spl_init() failed: %d\n", ret);
		hang();
	}

	enable_tzc380();

	tx8m_spl_i2c_setup();
#if !IS_ENABLED(CONFIG_DM_PMIC)
	power_init_board();
#endif
	/* DDR initialization */
	spl_dram_init();

	board_init_r(NULL, 0);
}
#endif
