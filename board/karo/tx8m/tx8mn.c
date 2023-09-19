// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <console.h>
#include <debug_uart.h>
#include <errno.h>
#include <fdt_support.h>
#include <fsl_esdhc_imx.h>
#include <fsl_wdog.h>
#include <fuse.h>
#include <i2c.h>
#include <led.h>
#include <malloc.h>
#include <miiphy.h>
#include <mmc.h>
#include <netdev.h>
#include <spl.h>
#include <thermal.h>
#include <asm-generic/gpio.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx8mn_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/armv8/mmu.h>
#include <asm/mach-imx/dma.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/mach-imx/video.h>
#include <asm/setup.h>
#include <asm/bootm.h>
#include <dm/uclass.h>
#include <linux/arm-smccc.h>
#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#define GPIO_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_PE |	\
					     PAD_CTL_PUE |	\
					     PAD_CTL_DSE6)

enum tx8m_boardtype {
	TX8MN,
	QS8M_QSBASE,
	NUM_BOARD_TYPES
};

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	if (IS_ENABLED(CONFIG_DEBUG_UART)) {
		debug_uart_init();
		printascii("enabled\n");
	}

#if defined(CONFIG_KARO_TX8MN)
	printf("Board: Ka-Ro TX8M-ND00\n");
#elif defined(CONFIG_KARO_QS8M_ND00)
	printf("Board: Ka-Ro QS8M-ND00\n");
#else
#error Unsupported module variant
#endif
	return 0;
}
#endif

#ifdef CONFIG_OF_BOARD_FIXUP
int board_fix_fdt(void *blob)
{
	return 0;
}
#endif

#ifdef CONFIG_DM_I2C
static inline int tx8mn_i2c_init(void)
{
	int ret = 0;
	int i;

	for (i = 0; ret != -ENODEV; i++) {
		struct udevice *i2c_dev;
		u8 i2c_addr;

		ret = uclass_get_device_by_seq(UCLASS_I2C, i, &i2c_dev);
		if (ret == -ENODEV)
			break;

		for (i2c_addr = 0x07; i2c_addr < 0x78; i2c_addr++) {
			struct udevice *chip;

			ret = dm_i2c_probe(i2c_dev, i2c_addr, 0x0, &chip);
			if (ret == 0) {
				debug("Found an I2C device @ %u:%02x\n",
				      i, i2c_addr);
			} else if (ret != -EREMOTEIO) {
				printf("Error %d accessing device %u:%02x\n",
				       ret, i, i2c_addr);
				break;
			}
		}
	}
	return ret;
}
#else
static inline int tx8mn_i2c_init(void)
{
	return 0;
}
#endif

int board_init(void)
{
	if (ctrlc())
		printf("<CTRL-C> detected; safeboot enabled\n");

	if (IS_ENABLED(CONFIG_KARO_TX8M)) {
		int ret;
		struct gpio_desc reset_out;
		const iomux_v3_cfg_t tx8mn_gpio_pads[] = {
			IMX8MN_PAD_SD2_RESET_B__GPIO2_IO19 | GPIO_PAD_CTRL,
		};

		ret = dm_gpio_lookup_name("gpio2_19", &reset_out);
		if (ret) {
			printf("Failed to lookup ENET0_PWR GPIO: %d\n", ret);
			return ret;
		}
		ret = dm_gpio_request(&reset_out, "RESET_OUT");
		if (ret) {
			printf("Failed to request RESET_OUT GPIO: %d\n", ret);
			return ret;
		}

		imx_iomux_v3_setup_multiple_pads(tx8mn_gpio_pads,
						 ARRAY_SIZE(tx8mn_gpio_pads));
	}

	tx8mn_i2c_init();
	tx8m_led_init();

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_R
int board_early_init_r(void)
{
	return 0;
}
#endif

int board_late_init(void)
{
	int ret;
	struct src *src_regs = (void *)SRC_BASE_ADDR;
	struct watchdog_regs *wdog = (void *)WDOG1_BASE_ADDR;
	u32 srsr = readl(&src_regs->srsr);
	u16 wrsr = readw(&wdog->wrsr);
	enum tx8m_boardtype board = TX8MN;
	const char *fdt_file;
	const char *baseboard;

	karo_env_cleanup();

	baseboard = env_get("baseboard");
	if (baseboard && strncmp(baseboard, "qsbase", 6) == 0)
		board = QS8M_QSBASE;

	if (srsr & 0x10 && !(wrsr & WRSR_SFTW)) {
		printf("Watchog reset detected; reboot required!\n");
		env_set("wdreset", "1");
	}
	if (had_ctrlc()) {
		env_set("safeboot", "1");
		fdt_file = NULL;
	} else {
		fdt_file = env_get("fdt_file");
	}
	if (fdt_file) {
		ret = karo_load_fdt(fdt_file);
		if (ret)
			printf("Failed to load FDT from '%s': %d\n",
			       fdt_file, ret);
	}

	karo_set_ethaddr(0);

	clear_ctrlc();
	return 0;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno + 1;
}
