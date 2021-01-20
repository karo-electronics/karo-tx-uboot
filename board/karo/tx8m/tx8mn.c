// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Lothar Waßmann <LW@KARO-electronics.de>
 *
 */

#include <common.h>
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
#include <mipi_dsi.h>
#include <mmc.h>
#include <mxsfb.h>
#include <netdev.h>
#include <spl.h>
#include <thermal.h>
#include <usb.h>
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
#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#define GPIO_PAD_CTRL		MUX_PAD_CTRL(PAD_CTL_PE |	\
					     PAD_CTL_PUE |	\
					     PAD_CTL_DSE6)

#ifdef CONFIG_USB
int board_usb_init(int index, enum usb_init_type init)
{
	imx8m_usb_power(index, true);
	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	imx8m_usb_power(index, false);
	return 0;
}
#endif

enum tx8m_boardtype {
	TX8MN,
	QS8M_QSBASE,
	NUM_BOARD_TYPES
};

#ifdef CONFIG_FEC_MXC
static void tx8mn_setup_fec(enum tx8m_boardtype board)
{
	struct iomuxc_gpr_base_regs *iomuxc_gpr_regs =
		(void *)IOMUXC_GPR_BASE_ADDR;
	unsigned char mac[6];

	if (board == TX8MN) {
		set_clk_enet(ENET_50MHZ);

		/* Use 50M anatop REF_CLK1 for ENET1, not from external */
		setbits_le32(&iomuxc_gpr_regs->gpr[1],
			     IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK);
	} else {
		set_clk_enet(ENET_125MHZ);

		/* Use 125M anatop REF_CLK1 for ENET1, not from external */
		setbits_le32(&iomuxc_gpr_regs->gpr[1],
			     IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK);
	}

	if (!env_get("ethaddr")) {
		imx_get_mac_from_fuse(0, mac);
		printf("MAC addr: %pM\n", mac);
	} else {
		printf("MAC addr: %s\n", env_get("ethaddr"));
	}
}
#else
static inline void tx8mn_setup_fec(enum tx8m_boardtype board)
{
}
#endif /* FEX_MXC */

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

#ifdef CONFIG_DM_VIDEO
#define TX8M_DSI83_I2C_BUS		1

#define DSI83_SLAVE_ADDR		0x2c

#define DISPLAY_MIX_SFT_RSTN_CSR	0x00
#define DISPLAY_MIX_CLK_EN_CSR		0x04

/* 'DISP_MIX_SFT_RSTN_CSR' bit fields */
#define BUS_RSTN_BLK_SYNC_SFT_EN	BIT(8)
#define LCDIF_APB_CLK_RSTN		BIT(5)
#define LCDIF_PIXEL_CLK_RSTN		BIT(4)

/* 'DISP_MIX_CLK_EN_CSR' bit fields */
#define BUS_BLK_CLK_SFT_EN		BIT(8)
#define LCDIF_APB_CLK_SFT_EN		BIT(5)
#define LCDIF_PIXEL_CLK_SFT_EN		BIT(4)

#define DSI_DDR_MODE			0

static void disp_mix_bus_rstn_reset(ulong gpr_base, bool reset)
{
	if (!reset)
		/* release reset */
		setbits_le32(gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
			     BUS_RSTN_BLK_SYNC_SFT_EN | LCDIF_APB_CLK_RSTN |
			     LCDIF_PIXEL_CLK_RSTN);
	else
		/* hold reset */
		clrbits_le32(gpr_base + DISPLAY_MIX_SFT_RSTN_CSR,
			     BUS_RSTN_BLK_SYNC_SFT_EN | LCDIF_APB_CLK_RSTN |
			     LCDIF_PIXEL_CLK_RSTN);
}

static void disp_mix_lcdif_clks_enable(ulong gpr_base, bool enable)
{
	if (enable)
		/* enable lcdif clks */
		setbits_le32(gpr_base + DISPLAY_MIX_CLK_EN_CSR,
			     BUS_BLK_CLK_SFT_EN | LCDIF_PIXEL_CLK_SFT_EN |
			     LCDIF_APB_CLK_SFT_EN);
	else
		/* disable lcdif clks */
		clrbits_le32(gpr_base + DISPLAY_MIX_CLK_EN_CSR,
			     BUS_BLK_CLK_SFT_EN | LCDIF_PIXEL_CLK_SFT_EN |
			     LCDIF_APB_CLK_SFT_EN);
}

#define LINE_LENGTH	1280
#define H_FRONT_PORCH	64
#define H_BACK_PORCH	4
#define HSYNC_LEN	1
#define VERT_SIZE	800
#define V_FRONT_PORCH	40
#define V_BACK_PORCH	1
#define VSYNC_LEN	1
#define VREFRESH	60

#define BPP		24
#define SYNC_DELAY	64
#define HTOTAL		(HSYNC_LEN + H_BACK_PORCH + LINE_LENGTH + H_FRONT_PORCH)
#define VTOTAL		(VSYNC_LEN + V_BACK_PORCH + VERT_SIZE + V_FRONT_PORCH)
#define PCLK		(HTOTAL * VTOTAL * VREFRESH)
#define DSI_CLK		(PCLK * BPP / 4 / (!!DSI_DDR_MODE + 1))
#define DSI_CLK_DIV	((DSI_CLK + PCLK - 1) / PCLK)
#define LVDS_CLK	(DSI_CLK / DSI_CLK_DIV)
#define LVDS_CLK_DIV	((LVDS_CLK + 12500000) / 25000000 - 1)

static struct dsi83_data {
	u8 addr;
	u8 val;
	u8 mask;
} dsi83_data[] = {
	{ 0x09, 1, },
	{ 0x0d, 0x00, 0x01, },
	{ 0x10, 0x00, 0x18, }, // DSI lanes 0x00: 4 lanes; 0x08: 3 lanes
	{ 0x10, 0x00, 0x01, }, // SOT_ERR_TOL_DIS
	{ 0x11, 0xc0, 0xc0, }, // DSI DATA equalization
	{ 0x11, 0x0c, 0x0c, }, // DSI clock equalization
	{ 0x12, DSI_CLK / 1000000 / 5, }, // DSI clk range 8: 40..45MHz; 9: 45..50MHz; ...
	{ 0x0a, 0x01, 0x01, },
	{ 0x0a, LVDS_CLK_DIV << 1, 0x0e, },
	{ 0x0b, 0x00, 0x03, },
	{ 0x0b, (DSI_CLK_DIV - 1) << 3, 0x7c, },
	{ 0x18, 0x60, 0xe0, }, // DE_NEG HS_NEG VS_NEG
	{ 0x18, 0x08, 0x0a, }, // CHA_24BPP_MODE CHA24BPP_FORMAT1
	{ 0x20, LINE_LENGTH % 256, },
	{ 0x21, LINE_LENGTH / 256, },
	{ 0x24, VERT_SIZE % 256, },
	{ 0x25, VERT_SIZE / 256, },
	{ 0x28, SYNC_DELAY % 256, },
	{ 0x29, SYNC_DELAY / 256, },
	{ 0x2c, HSYNC_LEN % 256, },
	{ 0x2d, HSYNC_LEN / 256, },
	{ 0x30, VSYNC_LEN % 256, },
	{ 0x31, VSYNC_LEN / 256, },
	{ 0x34, H_BACK_PORCH + HSYNC_LEN, },
	{ 0x36, V_BACK_PORCH + VSYNC_LEN, },
	{ 0x38, H_FRONT_PORCH, },
	{ 0x3a, V_FRONT_PORCH, },
	//{ 0x3c, 0x10, 0x10, }, // enable CHA_TEST_PATTERN
	{ 0x3c, 0x00, },
	{ 0x0d, 0x01, 0x01, }, // enable PLL
	{ 0xe5, 0xfd, 0xfd, }, // clear error status
};

static int dsi83_init(void)
{
	int ret;
	struct udevice *dev;
	struct udevice *chip;
	u8 val;

	debug("DSI clock: %u.%03uMHz dsi_clk_div=%u\n",
	      DSI_CLK / 1000000, DSI_CLK / 1000 % 1000, DSI_CLK_DIV);
	debug("LVDS clock: %u.%03uMHz lvds_clk_range=%u\n",
	      LVDS_CLK / 1000000, LVDS_CLK / 1000 % 1000, LVDS_CLK_DIV);

	ret = uclass_get_device_by_seq(UCLASS_I2C, TX8M_DSI83_I2C_BUS, &dev);
	if (ret) {
		printf("%s: Failed to find I2C bus device: %d\n",
		       __func__, ret);
		return ret;
	}
	ret = dm_i2c_probe(dev, DSI83_SLAVE_ADDR, 0x0, &chip);
	if (ret) {
		printf("%s: I2C probe failed for slave addr %02x: %d\n",
		       __func__, DSI83_SLAVE_ADDR, ret);
		return ret;
	}
	for (size_t i = 0; i < ARRAY_SIZE(dsi83_data); i++) {
		struct dsi83_data *p = &dsi83_data[i];

		ret = dm_i2c_read(chip, p->addr, &val, 1);
		if (ret) {
			printf("%s: Failed to read reg %02x\n",
			       __func__, p->val);
			return ret;
		}
		debug("%s@%d: Read %02x from reg %02x\n",
		      __func__, __LINE__, val, p->addr);
		val = (val & ~p->mask) | p->val;
		debug("%s@%d: Writing %02x to reg %02x\n",
		      __func__, __LINE__, val, p->addr);
		ret = dm_i2c_write(chip, p->addr, &val, 1);
		if (ret) {
			printf("%s: Failed to write %02x to reg %02x\n",
			       __func__, p->val, p->addr);
			return ret;
		}
	}
	return 0;
}

static const iomux_v3_cfg_t tx8mn_lcd_pads[] = {
	IMX8MN_PAD_GPIO1_IO01__GPIO1_IO1 | GPIO_PAD_CTRL,
	IMX8MN_PAD_GPIO1_IO04__GPIO1_IO4 | GPIO_PAD_CTRL,
};

static void tx8m_backlight_enable(void)
{
	int ret;
	struct gpio_desc backlight_control;
	struct gpio_desc lcd_enable;

	ret = dm_gpio_lookup_name("gpio1_1", &backlight_control);
	if (ret == 0) {
		ret = dm_gpio_request(&backlight_control, "BACKLIGHT_CONTROL");
		if (ret == 0) {
			dm_gpio_set_dir_flags(&backlight_control,
					      GPIOD_IS_OUT);
			dm_gpio_set_value(&backlight_control, 1);
		} else {
			printf("Failed to request BACKLIGHT_CONTROL GPIO: %d\n",
			       ret);
		}
	} else {
		printf("Failed to lookup BACKLIGHT_CONTROL GPIO: %d\n", ret);
	}

	ret = dm_gpio_lookup_name("gpio1_4", &lcd_enable);
	if (ret == 0) {
		ret = dm_gpio_request(&lcd_enable, "LCD_ENABLE");
		if (ret == 0) {
			dm_gpio_set_dir_flags(&lcd_enable, GPIOD_IS_OUT);
			dm_gpio_set_value(&lcd_enable, 1);
		} else {
			printf("Failed to request LCD_ENABLE GPIO: %d\n", ret);
		}
	} else {
		printf("Failed to lookup LCD_ENABLE GPIO: %d\n", ret);
	}

	imx_iomux_v3_setup_multiple_pads(tx8mn_lcd_pads,
					 ARRAY_SIZE(tx8mn_lcd_pads));
}

#define FSL_SIP_GPC			0xC2000000
#define FSL_SIP_CONFIG_GPC_PM_DOMAIN	0x3
#define DISPMIX				9
#define MIPI				10

static void do_enable_mipi2lvds(struct display_info_t const *disp)
{
	/* enable the dispmix & mipi phy power domain */
pr_info("%s@%d: \n", __func__, __LINE__);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN,
		     DISPMIX, true, 0);
pr_info("%s@%d: \n", __func__, __LINE__);
	call_imx_sip(FSL_SIP_GPC, FSL_SIP_CONFIG_GPC_PM_DOMAIN,
		     MIPI, true, 0);

	/* Get lcdif out of reset */
pr_info("%s@%d: \n", __func__, __LINE__);
	disp_mix_bus_rstn_reset(CSI_BASE_ADDR + 0x8000, false);
pr_info("%s@%d: \n", __func__, __LINE__);
	disp_mix_lcdif_clks_enable(CSI_BASE_ADDR + 0x8000, true);

	dsi83_init();
	tx8m_backlight_enable();
}

static struct display_info_t const panel_info[] = {
	{
		.bus = LCDIF_BASE_ADDR,
		.pixfmt = BPP,
		.enable	= do_enable_mipi2lvds,
		.mode	= {
			.name		= "MIPI2LVDS",
			.refresh	= VREFRESH,
			.xres		= LINE_LENGTH,
			.yres		= VERT_SIZE,
			.pixclock	= KHZ2PICOS(PCLK / 1000),
			.left_margin	= H_BACK_PORCH,
			.hsync_len	= HSYNC_LEN,
			.right_margin	= H_FRONT_PORCH,
			.upper_margin	= V_BACK_PORCH,
			.vsync_len	= VSYNC_LEN,
			.lower_margin	= V_FRONT_PORCH,
			.sync		= FB_SYNC_EXT,
			.vmode		= FB_VMODE_NONINTERLACED,
		},
	},
};
#else /* CONFIG_DM_VIDEO */
static inline int dsi83_init(void)
{
	return 0;
}
#endif /* CONFIG_DM_VIDEO */

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

#if defined(CONFIG_DM_VIDEO)
	if (env_get("panel"))
		do_enable_mipi2lvds(panel_info);
#endif

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

	tx8mn_setup_fec(board);

	clear_ctrlc();
	return 0;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno + 1;
}
