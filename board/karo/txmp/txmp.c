// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019, Lothar Waßmann <LW@KARO-electronics.de>
 *     based on: board/st/stmp32mp1/board.c
 *     Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 */

#include <config.h>
#include <common.h>
#include <adc.h>
#include <clk.h>
#include <console.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <fdt_support.h>
#include <fuse.h>
#include <generic-phy.h>
#include <i2c.h>
#include <image.h>
#include <led.h>
#include <misc.h>
#include <mmc.h>
#include <mtd_node.h>
#include <net.h>
#include <phy.h>
#include <rand.h>
#include <remoteproc.h>
#include <reset.h>
#include <syscon.h>
#include <usb.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/stm32.h>
#include <asm/arch/stm32mp1_smc.h>
#include <jffs2/load_kernel.h>
#include <linux/delay.h>
#include <linux/if_ether.h>
#include <power/regulator.h>
#include <usb/dwc2_udc.h>

#ifdef CONFIG_VIDEO_LOGO
#include <video.h>
#include <bmp_logo.h>
#endif

#include "../common/karo.h"

/* SYSCFG registers */
#define SYSCFG_BOOTR				0x00
#define SYSCFG_PMCSETR				0x04
#define SYSCFG_IOCTRLSETR			0x18
#define SYSCFG_ICNR				0x1C
#define SYSCFG_CMPCR				0x20
#define SYSCFG_CMPENSETR			0x24
#define SYSCFG_PMCCLRR				0x44

#define SYSCFG_BOOTR_BOOT_MASK			GENMASK(2, 0)
#define SYSCFG_BOOTR_BOOTPD_SHIFT		4

#define SYSCFG_IOCTRLSETR_HSLVEN_TRACE		BIT(0)
#define SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI	BIT(1)
#define SYSCFG_IOCTRLSETR_HSLVEN_ETH		BIT(2)
#define SYSCFG_IOCTRLSETR_HSLVEN_SDMMC		BIT(3)
#define SYSCFG_IOCTRLSETR_HSLVEN_SPI		BIT(4)

#define SYSCFG_CMPCR_SW_CTRL			BIT(1)
#define SYSCFG_CMPCR_READY			BIT(8)

#define SYSCFG_CMPENSETR_MPU_EN			BIT(0)

#define SYSCFG_PMCSETR_ETH_CLK_SEL		BIT(16)
#define SYSCFG_PMCSETR_ETH_REF_CLK_SEL		BIT(17)

#define SYSCFG_PMCSETR_ETH_SELMII		BIT(20)

#define SYSCFG_PMCSETR_ETH_SEL_MASK		GENMASK(23, 21)
#define SYSCFG_PMCSETR_ETH_SEL_GMII_MII		(0x0 << 21)
#define SYSCFG_PMCSETR_ETH_SEL_RGMII		(0x1 << 21)
#define SYSCFG_PMCSETR_ETH_SEL_RMII		(0x4 << 21)

/*
 * Get a global data pointer
 */
DECLARE_GLOBAL_DATA_PTR;

#define USB_WARNING_LOW_THRESHOLD_UV		 660000
#define USB_START_LOW_THRESHOLD_UV		1230000
#define USB_START_HIGH_THRESHOLD_UV		2100000

#ifdef CONFIG_VIDEO_LOGO
static void show_bmp_logo(void)
{
	int ret;
	struct udevice *dev;

	ret = uclass_get_device(UCLASS_VIDEO, 0, &dev);
	debug("uclass_get_device(UCLASS_VIDEO) returned %d\n", ret);
	if (ret)
		return;

	video_bmp_display(dev, (uintptr_t)bmp_logo_bitmap,
			  0, 0, false);
}
#else
static inline void show_bmp_logo(void)
{
}
#endif

int checkboard(void)
{
	int ret;
	char *mode;
	u32 otp;
	struct udevice *dev;
	const char *fdt_compat;
	int fdt_compat_len;

	debug("%s@%d:\n", __func__, __LINE__);

	if (IS_ENABLED(CONFIG_STM32MP1_TRUSTED))
		mode = "trusted";
	else
		mode = "basic";

#if defined(CONFIG_KARO_TXMP_1530)
	printf("Board: TXMP-1530 in %s mode", mode);
#elif defined(CONFIG_KARO_TXMP_1570)
	printf("Board: TXMP-1570 in %s mode", mode);
#elif defined(CONFIG_KARO_QSMP_1510)
	printf("Board: QSMP-1510 in %s mode", mode);
#elif defined(CONFIG_KARO_QSMP_1530)
	printf("Board: QSMP-1530 in %s mode", mode);
#elif defined(CONFIG_KARO_QSMP_1570)
	printf("Board: QSMP-1570 in %s mode", mode);
#else
#error Unsupported Board type
#endif
	fdt_compat = fdt_getprop(gd->fdt_blob, 0, "compatible",
				 &fdt_compat_len);
	if (fdt_compat && fdt_compat_len)
		printf(" (%s)", fdt_compat);
	puts("\n");

	ret = uclass_get_device_by_driver(UCLASS_MISC,
					  DM_GET_DRIVER(stm32mp_bsec),
					  &dev);

	if (!ret)
		ret = misc_read(dev, STM32_BSEC_SHADOW(BSEC_OTP_BOARD),
				&otp, sizeof(otp));
	if (!ret && otp) {
		printf("Board: MB%04x Var%d Rev.%c-%02d\n",
		       otp >> 16,
		       (otp >> 12) & 0xF,
		       ((otp >> 8) & 0xF) - 1 + 'A',
		       otp & 0xF);
	}

	return 0;
}

static void board_key_check(void)
{
#if defined(CONFIG_FASTBOOT) || defined(CONFIG_CMD_STM32PROG)
	ofnode node;
	struct gpio_desc gpio;
	enum forced_boot_mode boot_mode = BOOT_NORMAL;

	debug("%s@%d:\n", __func__, __LINE__);

	node = ofnode_path("/config");
	if (!ofnode_valid(node)) {
		pr_debug("%s: no /config node?\n", __func__);
		return;
	}
#ifdef CONFIG_FASTBOOT
	if (gpio_request_by_name_nodev(node, "st,fastboot-gpios", 0,
				       &gpio, GPIOD_IS_IN)) {
		pr_debug("%s: could not find /config/st,fastboot-gpios\n",
			 __func__);
	} else {
		if (dm_gpio_get_value(&gpio)) {
			puts("Fastboot key pressed, ");
			boot_mode = BOOT_FASTBOOT;
		}

		dm_gpio_free(NULL, &gpio);
	}
#endif
#ifdef CONFIG_CMD_STM32PROG
	if (gpio_request_by_name_nodev(node, "st,stm32prog-gpios", 0,
				       &gpio, GPIOD_IS_IN)) {
		pr_debug("%s: could not find /config/st,stm32prog-gpios\n",
			 __func__);
	} else {
		if (dm_gpio_get_value(&gpio)) {
			puts("STM32Programmer key pressed, ");
			boot_mode = BOOT_STM32PROG;
		}
		dm_gpio_free(NULL, &gpio);
	}
#endif

	if (boot_mode != BOOT_NORMAL) {
		puts("entering download mode...\n");
		clrsetbits_le32(TAMP_BOOT_CONTEXT,
				TAMP_BOOT_FORCED_MASK,
				boot_mode);
	}
#endif
}

static void sysconf_init(void)
{
#ifndef CONFIG_STM32MP1_TRUSTED
	void *syscfg;
#ifdef CONFIG_DM_REGULATOR
	struct udevice *pwr_dev;
	struct udevice *pwr_reg;
	struct udevice *dev;
	int ret;
	u32 otp = 0;
#endif
	u32 bootr;

	debug("%s@%d:\n", __func__, __LINE__);

	syscfg = syscon_get_first_range(STM32MP_SYSCON_SYSCFG);
	debug("SYSCFG: init @0x%p\n", syscfg);

	/* interconnect update : select master using the port 1 */
	/* LTDC = AXI_M9 */
	/* GPU  = AXI_M8 */
	/* today information is hardcoded in U-Boot */
	writel(BIT(9), syscfg + SYSCFG_ICNR);
	debug("[%p] SYSCFG.icnr = 0x%08x (LTDC and GPU)\n",
	      syscfg + SYSCFG_ICNR, readl(syscfg + SYSCFG_ICNR));

	/* disable Pull-Down for boot pin connected to VDD */
	bootr = readl(syscfg + SYSCFG_BOOTR);
	bootr &= ~(SYSCFG_BOOTR_BOOT_MASK << SYSCFG_BOOTR_BOOTPD_SHIFT);
	bootr |= (bootr & SYSCFG_BOOTR_BOOT_MASK) << SYSCFG_BOOTR_BOOTPD_SHIFT;
	writel(bootr, syscfg + SYSCFG_BOOTR);
	debug("[%p] SYSCFG.bootr = 0x%08x\n",
	      syscfg + SYSCFG_BOOTR, readl(syscfg + SYSCFG_BOOTR));

#ifdef CONFIG_DM_REGULATOR
	/* High Speed Low Voltage Pad mode Enable for SPI, SDMMC, ETH, QSPI
	 * and TRACE. Needed above ~50MHz and conditioned by AFMUX selection.
	 * The customer will have to disable this for low frequencies
	 * or if AFMUX is selected but the function not used, typically for
	 * TRACE. Otherwise, impact on power consumption.
	 *
	 * WARNING:
	 *   enabling High Speed mode while VDD>2.7V
	 *   with the OTP product_below_2v5 (OTP 18, BIT 13)
	 *   erroneously set to 1 can damage the IC!
	 *   => U-Boot set the register only if VDD < 2.7V (in DT)
	 *      but this value need to be consistent with board design
	 */
	ret = uclass_get_device_by_driver(UCLASS_PMIC,
					  DM_GET_DRIVER(stm32mp_pwr_pmic),
					  &pwr_dev);
	if (!ret) {
		ret = uclass_get_device_by_driver(UCLASS_MISC,
						  DM_GET_DRIVER(stm32mp_bsec),
						  &dev);
		if (ret) {
			pr_err("Can't find stm32mp_bsec driver\n");
			return;
		}

		ret = misc_read(dev, STM32_BSEC_SHADOW(18), &otp, 4);
		if (!ret)
			otp = otp & BIT(13);

		/* get VDD = pwr-supply */
		ret = device_get_supply_regulator(pwr_dev, "pwr-supply",
						  &pwr_reg);

		/* check if VDD is Low Voltage */
		if (!ret) {
			if (regulator_get_value(pwr_reg) < 2700000) {
				writel(SYSCFG_IOCTRLSETR_HSLVEN_TRACE |
				       SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI |
				       SYSCFG_IOCTRLSETR_HSLVEN_ETH |
				       SYSCFG_IOCTRLSETR_HSLVEN_SDMMC |
				       SYSCFG_IOCTRLSETR_HSLVEN_SPI,
				       syscfg + SYSCFG_IOCTRLSETR);

				if (!otp)
					pr_err("product_below_2v5=0: HSLVEN protected by HW\n");
			} else {
				if (otp)
					pr_err("product_below_2v5=1: HSLVEN update is destructive, no update as VDD>2.7V\n");
			}
		} else {
			debug("VDD unknown");
		}
	}
#endif
	debug("[%p] SYSCFG.IOCTRLSETR = 0x%08x\n",
	      syscfg + SYSCFG_IOCTRLSETR,
	      readl(syscfg + SYSCFG_IOCTRLSETR));

	/* activate automatic I/O compensation
	 * warning: need to ensure CSI enabled and ready in clock driver
	 */
	writel(SYSCFG_CMPENSETR_MPU_EN, syscfg + SYSCFG_CMPENSETR);

	while (!(readl(syscfg + SYSCFG_CMPCR) & SYSCFG_CMPCR_READY))
		;
	clrbits_le32(syscfg + SYSCFG_CMPCR, SYSCFG_CMPCR_SW_CTRL);

	debug("[%p] SYSCFG.cmpcr = 0x%08x\n",
	      syscfg + SYSCFG_CMPCR, readl(syscfg + SYSCFG_CMPCR));
#endif
}

static void print_mac_from_fuse(void)
{
	u32 fuse[2];
	u8 mac[ETH_ALEN];

	fuse_read(0, 57, &fuse[0]);
	fuse_read(0, 58, &fuse[1]);
	memcpy(mac, fuse, ETH_ALEN);

	if (is_valid_ethaddr(mac))
		printf("MAC addr from fuse: %pM\n", mac);
}

/* board interface eth init */
/* this is a weak define that we are overriding */
int board_interface_eth_init(struct udevice *dev,
			     phy_interface_t interface_type)
{
	u32 value;
	u8 *syscfg = syscon_get_first_range(STM32MP_SYSCON_SYSCFG);
	bool eth_clk_sel_reg;
	bool eth_ref_clk_sel_reg;

	debug("%s@%d:\n", __func__, __LINE__);

	if (!syscfg)
		return -ENODEV;

	/* Gigabit Ethernet 125MHz clock selection. */
	eth_clk_sel_reg = dev_read_bool(dev, "st,eth-clk-sel");

	/* Ethernet 50Mhz RMII clock selection */
	eth_ref_clk_sel_reg =
		dev_read_bool(dev, "st,eth-ref-clk-sel");

	print_mac_from_fuse();

	switch (interface_type) {
	case PHY_INTERFACE_MODE_MII:
		value = SYSCFG_PMCSETR_ETH_SEL_GMII_MII |
			SYSCFG_PMCSETR_ETH_REF_CLK_SEL;
		debug("%s: PHY_INTERFACE_MODE_MII %08x\n", __func__, value);
		break;
	case PHY_INTERFACE_MODE_GMII:
		if (eth_clk_sel_reg)
			value = SYSCFG_PMCSETR_ETH_SEL_GMII_MII |
				SYSCFG_PMCSETR_ETH_CLK_SEL;
		else
			value = SYSCFG_PMCSETR_ETH_SEL_GMII_MII;
		debug("%s: PHY_INTERFACE_MODE_GMII %08x\n", __func__, value);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if (eth_ref_clk_sel_reg)
			value = SYSCFG_PMCSETR_ETH_SEL_RMII |
				SYSCFG_PMCSETR_ETH_REF_CLK_SEL;
		else
			value = SYSCFG_PMCSETR_ETH_SEL_RMII;
		debug("%s: PHY_INTERFACE_MODE_RMII %08x\n", __func__, value);
		break;
	case PHY_INTERFACE_MODE_RGMII:
		if (eth_clk_sel_reg)
			value = SYSCFG_PMCSETR_ETH_SEL_RGMII |
				SYSCFG_PMCSETR_ETH_CLK_SEL;
		else
			value = SYSCFG_PMCSETR_ETH_SEL_RGMII;
		debug("%s: PHY_INTERFACE_MODE_RGMII %08x\n", __func__, value);
		break;
	default:
		pr_warn("%s: Unsupported interface type %d\n",
			__func__, interface_type);
		/* Do not manage others interfaces */
		return -EINVAL;
	}

	/* clear and set ETH configuration bits */
	writel(SYSCFG_PMCSETR_ETH_SEL_MASK | SYSCFG_PMCSETR_ETH_SELMII |
	       SYSCFG_PMCSETR_ETH_REF_CLK_SEL | SYSCFG_PMCSETR_ETH_CLK_SEL,
	       syscfg + SYSCFG_PMCCLRR);
	writel(value, syscfg + SYSCFG_PMCSETR);

	debug("%s: SYSCFG_PMC=%08x\n", __func__,
	      readl(syscfg + SYSCFG_PMCSETR));

	return 0;
}

#if defined(CONFIG_LED)
#define TEMPERATURE_MIN			(-40)
#define TEMPERATURE_HOT			80

enum {
	LED_STATE_INIT = -1,
	LED_STATE_OFF,
	LED_STATE_ON,
	LED_STATE_DISABLED,
};

static int led_state = LED_STATE_INIT;
static struct udevice *led_dev;

static int txmp_get_led(struct udevice **dev, char *led_string)
{
	char *led_name;
	int ret;

	if (led_state == LED_STATE_DISABLED)
		return -ENODEV;

	led_name = fdtdec_get_config_string(gd->fdt_blob, led_string);
	if (!led_name) {
		debug("%s: could not find %s config string\n",
		      __func__, led_string);
		return -ENOENT;
	}
	ret = led_get_by_label(led_name, dev);
	if (ret) {
		debug("%s: ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_SHOW_ACTIVITY
#ifdef CONFIG_ADC

#ifndef CONFIG_TFABOOT
static int ts_calib_read(void)
{
	int ret;
	struct udevice *dev;
	u32 ts_cal;

	ret = uclass_get_device_by_name(UCLASS_MISC,
					"efuse@5c005000", &dev);
	if (ret) {
		printf("Failed to get efuse@5c005000: %d\n", ret);
		return ret;
	}

	ret = misc_read(dev, 0x5c, &ts_cal, 4);
	if (ret < 0)
		return ret;
	debug("ts_cal1=%u[%04x] ts_cal2=%u[%04x]\n",
	      ts_cal & 0xffff, ts_cal & 0xffff,
	      ts_cal >> 16, ts_cal >> 16);
	return ts_cal;
}
#else
static inline int ts_calib_read(void)
{
	return readl(0x5c00525c);
}
#endif /* CONFIG_TFABOOT */

static int txmp_read_temp(void)
{
	int ret;
	static unsigned int last_temp = ~0;
	unsigned int temp, raw;
	static union {
		int i;
		struct {
			u16 cal1;
			u16 cal2;
		};
	} ts_cal;

	if (ts_cal.i == 0) {
		ret = ts_calib_read();
		if (ret < 0) {
			printf("ts_calib_read() returned %d\n", ret);
			return ret;
		}

		ts_cal.i = ret;
		debug("ts_cal=%04x %04x\n", ts_cal.cal1, ts_cal.cal2);
	}

	ret = adc_channel_single_shot("adc@100", 12, &raw);
	if (ret) {
		if (ret != -ENODEV)
			printf("Failed to read ADC: %d\n", ret);
		return ret;
	}
	/*
	 *                          110 °C – 30 °C
	 * Temperature ( in °C ) = ----------------- × ( TS_DATA – TS_CAL1 ) + 30 °C
	 *                         TS_CAL2 – TS_CAL1
	 */
	temp = raw + ts_cal.cal1;
	if (temp < ts_cal.cal1 || temp > ts_cal.cal2) {
		printf("measured temp raw value %u out of range [%u..%u]\n",
		       temp, ts_cal.cal1, ts_cal.cal2);
		return -EINVAL;
	}
	temp = (80000000 / (ts_cal.cal2 - ts_cal.cal1) * (temp - ts_cal.cal1) +
		50000) / 100000 + 300;
	if (abs(temp - last_temp) > 10) {
		debug("Temperature=%4d.%d raw=%08x\n", temp / 10, temp % 10,
		      raw);
		last_temp = temp;
	}
	return temp / 10 + 1000;
}
#else
static inline int txmp_read_temp(void)
{
	return -ENOTSUPP;
}
#endif /* CONFIG_ADC */

static inline int calc_blink_rate(void)
{
	static int temp;

	if (temp >= 0)
		temp = txmp_read_temp();

	if (temp < 0)
		return CONFIG_SYS_HZ;

	temp -= 1000;
	return CONFIG_SYS_HZ + CONFIG_SYS_HZ / 10 -
		(temp - TEMPERATURE_MIN) * CONFIG_SYS_HZ /
		(TEMPERATURE_HOT - TEMPERATURE_MIN);
}

void show_activity(int arg)
{
	static int blink_rate;
	static ulong last;
	int ret;

	if (arg || !led_dev || led_state == LED_STATE_DISABLED)
		return;

	if (led_state == LED_STATE_INIT) {
		last = get_timer(0);
		ret = led_set_state(led_dev, LEDST_ON);
		if (ret) {
			led_state = LED_STATE_DISABLED;
			return;
		}
		led_state = LED_STATE_ON;
		blink_rate = calc_blink_rate();
	}
	if (get_timer(last) > blink_rate) {
		blink_rate = calc_blink_rate();
		last = get_timer(0);
		if (led_state == LED_STATE_ON) {
			led_set_state(led_dev, LEDST_OFF);
			led_state = LED_STATE_OFF;
		} else {
			led_set_state(led_dev, LEDST_ON);
			led_state = LED_STATE_ON;
		}
	}
}
#endif /* CONFIG_SHOW_ACTIVITY */

static void txmp_setup_led(void)
{
	int ret;

	ret = txmp_get_led(&led_dev, "u-boot,boot-led");
	if (ret) {
		pr_err("No boot-led defined\n");
		led_state = LED_STATE_DISABLED;
	}
	ret = led_default_state();
	if (ret) {
		pr_err("Failed to initialize default LED state: %d\n", ret);
		led_state = LED_STATE_DISABLED;
	}
}
#else
static inline void txmp_setup_led(void)
{
}
#endif /* CONFIG_LED */

#ifdef CONFIG_USB_GADGET_DWC2_OTG
/*
 * DWC2 registers should be defined in drivers
 * OTG: drivers/usb/gadget/dwc2_udc_otg_regs.h
 * HOST: ./drivers/usb/host/dwc2.h
 */
#define DWC2_GOTGCTL_OFFSET		0x00
#define DWC2_GGPIO_OFFSET		0x38

#define DWC2_GGPIO_VBUS_SENSING		BIT(21)

#define DWC2_GOTGCTL_AVALIDOVEN		BIT(4)
#define DWC2_GOTGCTL_AVALIDOVVAL	BIT(5)
#define DWC2_GOTGCTL_BVALIDOVEN		BIT(6)
#define DWC2_GOTGCTL_BVALIDOVVAL	BIT(7)
#define DWC2_GOTGCTL_BSVLD		BIT(19)

#define STM32MP_GUSBCFG			0x40002407

static struct dwc2_plat_otg_data stm32mp_otg_data = {
	.regs_otg = FDT_ADDR_T_NONE,
	.usb_gusbcfg = STM32MP_GUSBCFG,
};

static struct reset_ctl usbotg_reset;

static void board_usbotg_init(void)
{
	ofnode node;
	struct ofnode_phandle_args args;
	struct udevice *dev;
	struct clk clk;

	debug("%s@%d:\n", __func__, __LINE__);

	/* find the usb otg node */
	node = ofnode_by_compatible(ofnode_null(), "snps,dwc2");
	if (!ofnode_valid(node)) {
		pr_err("usb_otg device not found\n");
		return;
	}

	if (!ofnode_is_available(node)) {
		pr_err("stm32 usbotg is disabled in the device tree\n");
		return;
	}

	/* Enable clock */
	if (ofnode_parse_phandle_with_args(node, "clocks",
					   "#clock-cells", 0, 0, &args)) {
		pr_err("usbotg has no clocks defined in the device tree\n");
		return;
	}

	if (uclass_get_device_by_ofnode(UCLASS_CLK, args.node, &dev)) {
		pr_err("Can't get clk device\n");
		return;
	}

	if (args.args_count != 1) {
		printf("Bad args count %d for usb-otg clk\n", args.args_count);
		return;
	}

	clk.dev = dev;
	clk.id = args.args[0];

	if (clk_enable(&clk)) {
		pr_err("Failed to enable usbotg clock\n");
		return;
	}

	/* Reset */
	if (ofnode_parse_phandle_with_args(node, "resets",
					   "#reset-cells", 0, 0, &args)) {
		pr_err("usbotg has no resets defined in the device tree\n");
		goto clk_err;
	}

	if ((uclass_get_device_by_ofnode(UCLASS_RESET, args.node, &dev)) ||
	    args.args_count != 1)
		goto clk_err;

	usbotg_reset.dev = dev;
	usbotg_reset.id = args.args[0];

	/* Phy */
	if (!(ofnode_parse_phandle_with_args(node, "phys",
					     "#phy-cells", 0, 0, &args))) {
		int __maybe_unused ret;

		stm32mp_otg_data.phy_of_node = ofnode_get_parent(args.node);
		if (!ofnode_valid(stm32mp_otg_data.phy_of_node)) {
			pr_err("USB0 PHY device not found\n");
			goto clk_err;
		}
		if (of_live_active()) {
			stm32mp_otg_data.regs_phy = ofnode_get_addr(args.node);
			if (stm32mp_otg_data.regs_phy == FDT_ADDR_T_NONE) {
				printf("Failed to get addr of usbotg phy from %s\n",
				       ofnode_get_name(args.node));
				goto clk_err;
			}
		} else {
			stm32mp_otg_data.regs_phy = ofnode_get_addr(args.node);
			if (stm32mp_otg_data.regs_phy == FDT_ADDR_T_NONE) {
				printf("Failed to get addr of usbotg phy from %s\n",
				       ofnode_get_name(args.node));
				goto clk_err;
			}
		}
		debug("usbotg PHY addr=%08lx\n", stm32mp_otg_data.regs_phy);
	}

	/* Parse and store data needed for gadget */
	stm32mp_otg_data.regs_otg = ofnode_get_addr(node);
	if (stm32mp_otg_data.regs_otg == FDT_ADDR_T_NONE) {
		printf("usbotg: can't get base address\n");
		goto clk_err;
	}

	stm32mp_otg_data.rx_fifo_sz = ofnode_read_u32_default(node,
							      "g-rx-fifo-size",
							      0);
	stm32mp_otg_data.np_tx_fifo_sz = ofnode_read_u32_default(node,
								 "g-np-tx-fifo-size", 0);
	stm32mp_otg_data.tx_fifo_sz = ofnode_read_u32_default(node,
							      "g-tx-fifo-size", 0);

	/* Enable voltage level detector */
	if (!(ofnode_parse_phandle_with_args(node, "usb33d-supply",
					     NULL, 0, 0, &args)))
		if (!uclass_get_device_by_ofnode(UCLASS_REGULATOR,
						 args.node, &dev))
			if (regulator_set_enable(dev, true)) {
				pr_err("Failed to enable usb33d\n");
				goto clk_err;
			}

	return;

clk_err:
	clk_disable(&clk);
}

int board_usb_init(int index, enum usb_init_type init)
{
	debug("%s@%d:\n", __func__, __LINE__);

	if (init == USB_INIT_HOST)
		return 0;

	if (stm32mp_otg_data.regs_otg == FDT_ADDR_T_NONE)
		return -EINVAL;

	/* Reset usbotg */
	reset_assert(&usbotg_reset);
	udelay(2);
	reset_deassert(&usbotg_reset);

	/* Enable vbus sensing */
	setbits_le32(stm32mp_otg_data.regs_otg + DWC2_GGPIO_OFFSET,
		     DWC2_GGPIO_VBUS_SENSING);

	return dwc2_udc_probe(&stm32mp_otg_data);
}

int g_dnl_board_usb_cable_connected(void)
{
	static int last;
	int conn = readl(stm32mp_otg_data.regs_otg + DWC2_GOTGCTL_OFFSET);

	if (conn != last) {
		printf("%s@%d: USB cable%s connected (GOTGCTL@%08lx=%08x)\n",
		       __func__, __LINE__,
		      (conn & DWC2_GOTGCTL_BSVLD) ? "" : " NOT",
		      stm32mp_otg_data.regs_otg, conn);
		last = conn;
	}
	return conn & DWC2_GOTGCTL_BSVLD;
}

#define STM32MP1_G_DNL_DFU_PRODUCT_NUM 0xdf11
int g_dnl_bind_fixup(struct usb_device_descriptor *dev, const char *name)
{
	debug("%s@%d:\n", __func__, __LINE__);

	if (!strcmp(name, "usb_dnl_dfu"))
		put_unaligned(STM32MP1_G_DNL_DFU_PRODUCT_NUM, &dev->idProduct);
	else
		put_unaligned(CONFIG_USB_GADGET_PRODUCT_NUM, &dev->idProduct);

	return 0;
}
#else
static inline void board_usbotg_init(void)
{
}
#endif /* CONFIG_USB_GADGET */

static inline u32 txmp_rng_init(void)
{
	unsigned int seed;
	unsigned int loop = 0;

	debug("%s@%d:\n", __func__, __LINE__);

	writel(0x40, 0x50000190);
	writel(0x40, 0x50000210);
	writel(0x40, 0x50000290);
	writel(0x40, 0x50000194);
	writel(0x4, 0x54003000);
	debug("Waiting for RNG to initialize\n");
	while (!(readl(0x54003004) & 0x1)) {
		loop++;
		if (loop > 100000) {
			printf("Timeout waiting for RNG to initialize\n");
			break;
		}
		udelay(1);
	}
	debug("RNG ready after %u loops\n", loop);
	seed = readl(0x54003008);
	return seed;
}

#include <linux/mtd/mtd.h>

/* board dependent setup after relocate */
int board_init(void)
{
	struct udevice *dev;

	debug("%s@%d:\n", __func__, __LINE__);

	//txmp_mtd_board_init();

	/* address of boot parameters */
	gd->bd->bi_boot_params = STM32_DDR_BASE + 0x100;

	/* probe all PINCTRL for hog */
	for (uclass_first_device(UCLASS_PINCTRL, &dev);
	     dev;
	     uclass_next_device(&dev)) {
		debug("probe pincontrol = %s\n", dev->name);
	}

	sysconf_init();

	if (ctrlc())
		printf("<CTRL-C> detected; safeboot enabled\n");

	show_bmp_logo();
	board_key_check();
	txmp_setup_led();

	board_usbotg_init();

	return 0;
}

static inline void txmp_set_bootdevice(void)
{
	const char *bootdev = env_get("boot_device");

	debug("%s@%d:\n", __func__, __LINE__);

	if (!bootdev) {
		printf("boot_device is not set\n");
		return;
	}
	debug("%s@%d: boot_device='%s' boot_instance='%s' preboot='%s'\n",
	      __func__, __LINE__, env_get("boot_device"),
	      env_get("boot_instance"), env_get("preboot"));
	if (strcmp(bootdev, "mmc") == 0) {
		unsigned long instance = env_get_ulong("boot_instance", 0, 0);

		if (instance == 1) {
			instance = 0;
			env_set_ulong("boot_instance", instance);
		}
	} else if (strcmp(bootdev, "usb") == 0) {
		const char *bootcmd = env_get("bootcmd");

		/* Save original 'bootcmd' for restoration after stm32prog */
		env_set(".bootcmd", bootcmd);
		env_set("bootcmd", "stm32prog ${boot_device} ${boot_instance}");
	}
}

static inline void rand_init(void)
{
	unsigned int seed;

	debug("%s@%d:\n", __func__, __LINE__);

	if (readl(0x50000000) & 1) {
		u64 ticks = get_ticks();
		u32 r = rand();

		seed = r ^ ticks;
		debug("RAND=%08x ticks=%08llx seed=%08x\n", r, ticks, seed);
	} else {
		seed = txmp_rng_init();
	}
	debug("RANDOM seed: %08x\n", seed);
	srand(seed);
}

int board_late_init(void)
{
#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	const void *fdt_compat;
	ofnode root = ofnode_path("/");

	debug("%s@%d:\n", __func__, __LINE__);

	rand_init();

	fdt_compat = ofnode_read_string(root, "compatible");

	if (fdt_compat) {
		if (strncmp(fdt_compat, "karo,", 5) != 0)
			env_set("board_name", fdt_compat);
		else
			env_set("board_name", fdt_compat + 5);
	}
#endif
	env_cleanup();

	if (had_ctrlc())
		env_set_hex("safeboot", 1);
	else if (!IS_ENABLED(CONFIG_KARO_UBOOT_MFG))
		karo_fdt_move_fdt();

	txmp_set_bootdevice();

	clear_ctrlc();
	return 0;
}

void board_quiesce_devices(void)
{
	debug("%s@%d:\n", __func__, __LINE__);
}

#if defined(CONFIG_OF_BOARD_SETUP)

#ifdef CONFIG_FDT_FIXUP_PARTITIONS
	struct node_info nodes[] = {
		{ "st,stm32f469-qspi",		MTD_DEV_TYPE_NOR,  },
		{ "stf1ge4u00m",		MTD_DEV_TYPE_SPINAND,  },
	};
#endif

int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret;
	ofnode node;

	debug("%s@%d:\n", __func__, __LINE__);

	ret = fdt_increase_size(blob, 4096);
	if (ret)
		printf("Warning: Failed to increase FDT size: %s\n",
		       fdt_strerror(ret));

#ifdef CONFIG_FDT_FIXUP_PARTITIONS
	karo_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
#endif

	/* Update DT if coprocessor started */
	node = ofnode_path("/m4");
	if (ofnode_valid(node)) {
		const char *s_copro = env_get("copro_state");
		ulong copro_rsc_addr, copro_rsc_size;

		copro_rsc_addr = env_get_hex("copro_rsc_addr", 0);
		copro_rsc_size = env_get_hex("copro_rsc_size", 0);

		if (s_copro) {
			ofnode_write_prop(node, "early-booted", 0, NULL);
			if (copro_rsc_addr)
				ofnode_write_prop(node, "rsc-address",
						  sizeof(copro_rsc_addr),
						  (void *)cpu_to_fdt32(copro_rsc_addr));
			if (copro_rsc_size)
				ofnode_write_prop(node, "rsc-size",
						  sizeof(copro_rsc_size),
						  (void *)cpu_to_fdt32(copro_rsc_size));
		} else {
			fdt_delprop(blob, ofnode_to_offset(node),
				    "early-booted");
		}
	}

	karo_fixup_lcd_panel(env_get("videomode"));

	return 0;
}
#endif

void board_copro_image_process(ulong fw_image, size_t fw_size)
{
	int ret, id = 0; /* Copro id fixed to 0 as only one coproc on mp1 */

	debug("%s@%d:\n", __func__, __LINE__);

	if (!rproc_is_initialized()) {
		if (rproc_init()) {
			printf("Remote Processor %d initialization failed\n",
			       id);
			return;
		}
	}

	ret = rproc_load(id, fw_image, fw_size);
	printf("Load Remote Processor %d with data@addr=0x%08lx %u bytes:%s\n",
	       id, fw_image, fw_size, ret ? " Failed!" : " Success!");

	if (!ret)
		rproc_start(id);
}

U_BOOT_FIT_LOADABLE_HANDLER(IH_TYPE_COPRO, board_copro_image_process);
