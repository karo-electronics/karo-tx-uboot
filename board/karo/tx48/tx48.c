/*
 * Copyright (C) 2012-2013 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * based on evm.c
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <errno.h>
#include <miiphy.h>
#include <cpsw.h>
#include <serial.h>
#include <libfdt.h>
#include <lcd.h>
#include <fdt_support.h>
#include <nand.h>
#include <net.h>
#include <linux/mtd/nand.h>
#include <linux/fb.h>
#include <asm/gpio.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/mux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <video_fb.h>
#include <asm/arch/da8xx-fb.h>

#include "../common/karo.h"

DECLARE_GLOBAL_DATA_PTR;

#define TX48_LED_GPIO		AM33XX_GPIO_NR(1, 26)
#define TX48_ETH_PHY_RST_GPIO	AM33XX_GPIO_NR(3, 8)
#define TX48_LCD_RST_GPIO	AM33XX_GPIO_NR(1, 19)
#define TX48_LCD_PWR_GPIO	AM33XX_GPIO_NR(1, 22)
#define TX48_LCD_BACKLIGHT_GPIO	AM33XX_GPIO_NR(3, 14)
#define TX48_MMC_CD_GPIO	AM33XX_GPIO_NR(3, 15)

#define NO_OF_MAC_ADDR		1
#ifndef ETH_ALEN
#define ETH_ALEN		6
#endif

struct pin_mux {
	short reg_offset;
	uint8_t val;
};

#define PAD_CTRL_BASE	0x800
#define OFFSET(x)	(unsigned int) (&((struct pad_signals *) \
				(PAD_CTRL_BASE))->x)

/*
 * Configure the pin mux for the module
 */
static inline void tx48_set_pin_mux(const struct pin_mux *pin_mux,
			int num_pins)
{
	int i;

	for (i = 0; i < num_pins; i++)
		writel(pin_mux[i].val, CTRL_BASE + pin_mux[i].reg_offset);
}

#define PRM_RSTST_GLOBAL_COLD_RST	(1 << 0)
#define PRM_RSTST_GLOBAL_WARM_SW_RST	(1 << 1)
#define PRM_RSTST_WDT1_RST		(1 << 4)
#define PRM_RSTST_EXTERNAL_WARM_RST	(1 << 5)
#define PRM_RSTST_ICEPICK_RST		(1 << 9)

static u32 prm_rstst __attribute__((section(".data")));

/*
 * Basic board specific setup
 */
static const struct pin_mux tx48_pads[] = {
	{ OFFSET(i2c0_sda), MODE(7) | RXACTIVE | PULLUDEN | PULLUP_EN, },
	{ OFFSET(i2c0_scl), MODE(7) | RXACTIVE | PULLUDEN | PULLUP_EN, },
	{ OFFSET(emu1), MODE(7), }, /* ETH PHY Reset */
};

static const struct pin_mux tx48_i2c_pads[] = {
	{ OFFSET(i2c0_sda), MODE(0) | RXACTIVE | PULLUDEN | PULLUP_EN, },
	{ OFFSET(i2c0_scl), MODE(0) | RXACTIVE | PULLUDEN | PULLUP_EN, },
};

static const struct gpio tx48_gpios[] = {
	{ AM33XX_GPIO_NR(3, 5), GPIOFLAG_INPUT, "I2C1_SDA", },
	{ AM33XX_GPIO_NR(3, 6), GPIOFLAG_INPUT, "I2C1_SCL", },
	{ AM33XX_GPIO_NR(3, 8), GPIOFLAG_OUTPUT_INIT_LOW, "ETH_PHY_RESET", },
};

static const struct pin_mux stk5_pads[] = {
	/* heartbeat LED */
	{ OFFSET(gpmc_a10), MODE(7) | PULLUDEN, },
	/* LCD RESET */
	{ OFFSET(gpmc_a3), MODE(7) | PULLUDEN, },
	/* LCD POWER_ENABLE */
	{ OFFSET(gpmc_a6), MODE(7) | PULLUDEN, },
	/* LCD Backlight (PWM) */
	{ OFFSET(mcasp0_aclkx), MODE(7) | PULLUDEN, },
	/* MMC CD */
	{ OFFSET(mcasp0_fsx), MODE(7) | PULLUDEN | PULLUP_EN, },
};

static const struct gpio stk5_gpios[] = {
	{ TX48_LED_GPIO, GPIOFLAG_OUTPUT_INIT_LOW, "HEARTBEAT LED", },
	{ TX48_MMC_CD_GPIO, GPIOFLAG_INPUT, "MMC0 CD", },
};

static const struct pin_mux stk5_lcd_pads[] = {
	/* LCD data bus */
	{ OFFSET(lcd_data0), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data1), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data2), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data3), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data4), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data5), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data6), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data7), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data8), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data9), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data10), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data11), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data12), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data13), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data14), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_data15), MODE(0) | PULLUDEN, },
	/* LCD control signals */
	{ OFFSET(lcd_hsync), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_vsync), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_pclk), MODE(0) | PULLUDEN, },
	{ OFFSET(lcd_ac_bias_en), MODE(0) | PULLUDEN, },
};

static const struct gpio stk5_lcd_gpios[] = {
	{ AM33XX_GPIO_NR(1, 19), GPIOFLAG_OUTPUT_INIT_LOW, "LCD RESET", },
	{ AM33XX_GPIO_NR(1, 22), GPIOFLAG_OUTPUT_INIT_LOW, "LCD POWER", },
	{ AM33XX_GPIO_NR(3, 14), GPIOFLAG_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

static const struct pin_mux stk5v5_pads[] = {
	/* CAN transceiver control */
	{ OFFSET(gpmc_ad8), MODE(7) | PULLUDEN, },
};

static const struct gpio stk5v5_gpios[] = {
	{ AM33XX_GPIO_NR(0, 22), GPIOFLAG_OUTPUT_INIT_HIGH, "CAN XCVR", },
};

#ifdef CONFIG_LCD
static u16 tx48_cmap[256];
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 1366,
	.vl_row = 768,

	.vl_bpix = LCD_COLOR32,	   /* Bits per pixel, 0: 1bpp, 1: 2bpp, 2: 4bpp, 3: 8bpp ... */
	.cmap = tx48_cmap,
};

static struct lcd_ctrl_config lcd_cfg = {
	.bpp = 24,
};

#define FB_SYNC_OE_LOW_ACT	(1 << 31)
#define FB_SYNC_CLK_LAT_FALL	(1 << 30)

static struct fb_videomode tx48_fb_modes[] = {
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

void *lcd_base;			/* Start of framebuffer memory	*/
void *lcd_console_address;	/* Start of console buffer	*/

int lcd_color_fg;
int lcd_color_bg;

short console_col;
short console_row;

static int lcd_enabled = 1;
static int lcd_bl_polarity;

static int lcd_backlight_polarity(void)
{
	return lcd_bl_polarity;
}

void lcd_initcolregs(void)
{
}

void lcd_setcolreg(ushort regno, ushort red, ushort green, ushort blue)
{
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
		gpio_set_value(TX48_LCD_PWR_GPIO, 1);
		udelay(100);
		gpio_set_value(TX48_LCD_RST_GPIO, 1);
		udelay(300000);
		gpio_set_value(TX48_LCD_BACKLIGHT_GPIO,
			lcd_backlight_polarity());
	}
}

void lcd_disable(void)
{
	if (lcd_enabled) {
		printf("Disabling LCD\n");
		da8xx_fb_disable();
		lcd_enabled = 0;
	}
}

static void tx48_lcd_panel_setup(struct da8xx_panel *p,
				struct fb_videomode *fb)
{
	p->pxl_clk = PICOS2KHZ(fb->pixclock) * 1000;

	p->width = fb->xres;
	p->hbp = fb->left_margin;
	p->hsw = fb->hsync_len;
	p->hfp = fb->right_margin;

	p->height = fb->yres;
	p->vbp = fb->upper_margin;
	p->vsw = fb->vsync_len;
	p->vfp = fb->lower_margin;

	p->invert_pxl_clk = !!(fb->sync & FB_SYNC_CLK_LAT_FALL);
}

void lcd_panel_disable(void)
{
	if (lcd_enabled) {
		debug("Switching LCD off\n");
		gpio_set_value(TX48_LCD_BACKLIGHT_GPIO,
			!lcd_backlight_polarity());
		gpio_set_value(TX48_LCD_PWR_GPIO, 0);
		gpio_set_value(TX48_LCD_RST_GPIO, 0);
	}
}

void lcd_ctrl_init(void *lcdbase)
{
	int color_depth = 24;
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	const char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p = &tx48_fb_modes[0];
	struct fb_videomode fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;

	if (!lcd_enabled) {
		debug("LCD disabled\n");
		return;
	}

	if (had_ctrlc() || (prm_rstst & PRM_RSTST_WDT1_RST)) {
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

	lcd_bl_polarity = karo_fdt_get_backlight_polarity(working_fdt);
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
					case 24:
					case 16:
					case 8:
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
		for (p = &tx48_fb_modes[0]; p->name != NULL; p++) {
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
	tx48_set_pin_mux(stk5_lcd_pads, ARRAY_SIZE(stk5_lcd_pads));

	if (karo_load_splashimage(0) == 0) {
		struct da8xx_panel da8xx_panel = { };

		debug("Initializing FB driver\n");
		tx48_lcd_panel_setup(&da8xx_panel, p);
		da8xx_video_init(&da8xx_panel, &lcd_cfg, color_depth);

		debug("Initializing LCD controller\n");
		video_hw_init();
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
	tx48_set_pin_mux(stk5_pads, ARRAY_SIZE(stk5_pads));
}

static void stk5v3_board_init(void)
{
	stk5_board_init();
}

static void stk5v5_board_init(void)
{
	stk5_board_init();

	gpio_request_array(stk5v5_gpios, ARRAY_SIZE(stk5v5_gpios));
	tx48_set_pin_mux(stk5v5_pads, ARRAY_SIZE(stk5v5_pads));
}

/* called with default environment! */
int board_init(void)
{
	int i;

	/* mach type passed to kernel */
#ifdef CONFIG_OF_LIBFDT
	gd->bd->bi_arch_number = -1;
#endif
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM_1 + 0x100;

	if (ctrlc() || (prm_rstst & PRM_RSTST_WDT1_RST)) {
		if (prm_rstst & PRM_RSTST_WDT1_RST)
			printf("WDOG RESET detected\n");
		else
			printf("<CTRL-C> detected; safeboot enabled\n");
	}

	gpio_request_array(tx48_gpios, ARRAY_SIZE(tx48_gpios));
	tx48_set_pin_mux(tx48_pads, ARRAY_SIZE(tx48_pads));

	for (i = 0; i < ARRAY_SIZE(tx48_gpios); i++) {
		int gpio = tx48_gpios[i].gpio;

		if (gpio_get_value(gpio) == 0)
			gpio_direction_output(gpio, 1);
	}

	tx48_set_pin_mux(tx48_pads, ARRAY_SIZE(tx48_i2c_pads));
	return 0;
}

static void show_reset_cause(u32 prm_rstst)
{
	const char *dlm = "";

	printf("RESET cause: ");
	if (prm_rstst & PRM_RSTST_GLOBAL_COLD_RST) {
		printf("%sPOR", dlm);
		dlm = " | ";
	}
	if (prm_rstst & PRM_RSTST_GLOBAL_WARM_SW_RST) {
		printf("%sSW", dlm);
		dlm = " | ";
	}
	if (prm_rstst & PRM_RSTST_WDT1_RST) {
		printf("%sWATCHDOG", dlm);
		dlm = " | ";
	}
	if (prm_rstst & PRM_RSTST_EXTERNAL_WARM_RST) {
		printf("%sWARM", dlm);
		dlm = " | ";
	}
	if (prm_rstst & PRM_RSTST_ICEPICK_RST) {
		printf("%sJTAG", dlm);
		dlm = " | ";
	}
	if (*dlm == '\0')
		printf("unknown");

	printf(" RESET\n");
}

/* called with default environment! */
int checkboard(void)
{
	prm_rstst = readl(PRM_RSTST);
	show_reset_cause(prm_rstst);

	printf("Board: Ka-Ro TX48-7020\n");

	timer_init();
	return 0;
}

static void tx48_set_cpu_clock(void)
{
	unsigned long cpu_clk = getenv_ulong("cpu_clk", 10, 0);
	unsigned long act_cpu_clk;

	if (cpu_clk == 0 || cpu_clk == mpu_clk_rate() / 1000000)
		return;

	if (had_ctrlc() || (prm_rstst & PRM_RSTST_WDT1_RST)) {
		printf("%s detected; skipping cpu clock change\n",
			(prm_rstst & PRM_RSTST_WDT1_RST) ?
			"WDOG RESET" : "<CTRL-C>");
		return;
	}

	mpu_pll_config_val(cpu_clk);

	act_cpu_clk = mpu_clk_rate();
	if (cpu_clk * 1000000 != act_cpu_clk) {
		printf("Failed to set CPU clock to %lu MHz; using %lu.%03lu MHz instead\n",
			cpu_clk, act_cpu_clk / 1000000,
			act_cpu_clk / 1000 % 1000);
	} else {
		printf("CPU clock set to %lu.%03lu MHz\n",
			act_cpu_clk / 1000000, act_cpu_clk / 1000 % 1000);
	}
}

static void tx48_init_mac(void)
{
	uint8_t mac_addr[ETH_ALEN];
	uint32_t mac_hi, mac_lo;

	/* try reading mac address from efuse */
	mac_lo = __raw_readl(MAC_ID0_LO);
	mac_hi = __raw_readl(MAC_ID0_HI);

	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi & 0xFF00) >> 8;
	mac_addr[2] = (mac_hi & 0xFF0000) >> 16;
	mac_addr[3] = (mac_hi & 0xFF000000) >> 24;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo & 0xFF00) >> 8;

	if (!is_valid_ethaddr(mac_addr)) {
		printf("No valid MAC address programmed\n");
		return;
	}
	printf("MAC addr from fuse: %pM\n", mac_addr);
	eth_setenv_enetaddr("ethaddr", mac_addr);
}

/* called with environment from NAND or MMC */
int board_late_init(void)
{
	int ret = 0;
	const char *baseboard;

	env_cleanup();

	tx48_set_cpu_clock();

	if (had_ctrlc())
		setenv_ulong("safeboot", 1);
	else if (prm_rstst & PRM_RSTST_WDT1_RST)
		setenv_ulong("wdreset", 1);
	else
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
	tx48_init_mac();
	clear_ctrlc();
	return ret;
}

#ifdef CONFIG_DRIVER_TI_CPSW
static void tx48_phy_init(void)
{
	debug("%s: Resetting ethernet PHY\n", __func__);

	gpio_direction_output(TX48_ETH_PHY_RST_GPIO, 0);

	udelay(100);

	/* Release nRST */
	gpio_set_value(TX48_ETH_PHY_RST_GPIO, 1);

	/* Wait for PHY internal POR signal to deassert */
	udelay(25000);
}

static void cpsw_control(int enabled)
{
	/* nothing for now */
	/* TODO : VTP was here before */
}

static struct cpsw_slave_data cpsw_slaves[] = {
	{
		.slave_reg_ofs	= 0x208,
		.sliver_reg_ofs	= 0xd80,
		.phy_addr	= 0,
		.phy_if		= PHY_INTERFACE_MODE_RMII,
	},
};

void s_init(void)
{
	/* Nothing to be done here */
}

static struct cpsw_platform_data cpsw_data = {
	.mdio_base		= CPSW_MDIO_BASE,
	.cpsw_base		= CPSW_BASE,
	.mdio_div		= 0xff,
	.channels		= 8,
	.cpdma_reg_ofs		= 0x800,
	.slaves			= ARRAY_SIZE(cpsw_slaves),
	.slave_data		= cpsw_slaves,
	.ale_reg_ofs		= 0xd00,
	.ale_entries		= 1024,
	.host_port_reg_ofs	= 0x108,
	.hw_stats_reg_ofs	= 0x900,
	.mac_control		= (1 << 5) /* MIIEN */,
	.control		= cpsw_control,
	.host_port_num		= 0,
	.version		= CPSW_CTRL_VERSION_2,
};

int board_eth_init(bd_t *bis)
{
	__raw_writel(RMII_MODE_ENABLE, MAC_MII_SEL);
	tx48_phy_init();
	return cpsw_register(&cpsw_data);
}
#endif /* CONFIG_DRIVER_TI_CPSW */

#if defined(CONFIG_OMAP_HSMMC) && !defined(CONFIG_SPL_BUILD)
int cpu_mmc_init(bd_t *bis)
{
	return omap_mmc_init(1, 0, 0, TX48_MMC_CD_GPIO, -1);
}
#endif

void tx48_disable_watchdog(void)
{
	struct wd_timer *wdtimer = (struct wd_timer *)WDT_BASE;

	while (readl(&wdtimer->wdtwwps) & (1 << 4))
		;
	writel(0xaaaa, &wdtimer->wdtwspr);
	while (readl(&wdtimer->wdtwwps) & (1 << 4))
		;
	writel(0x5555, &wdtimer->wdtwspr);
}

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
		gpio_set_value(TX48_LED_GPIO, 1);
		led_state = LED_STATE_ON;
	} else {
		if (get_timer(last) > CONFIG_SYS_HZ) {
			last = get_timer(0);
			if (led_state == LED_STATE_ON) {
				gpio_set_value(TX48_LED_GPIO, 0);
			} else {
				gpio_set_value(TX48_LED_GPIO, 1);
			}
			led_state = 1 - led_state;
		}
	}
}

#ifdef CONFIG_OF_BOARD_SETUP
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
#include <jffs2/jffs2.h>
#include <mtd_node.h>
static struct node_info nodes[] = {
	{ "ti,omap2-nand", MTD_DEV_TYPE_NAND, },
	{ "ti,am3352-gpmc", MTD_DEV_TYPE_NAND, },
};

#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif /* CONFIG_FDT_FIXUP_PARTITIONS */

static const char *tx48_touchpanels[] = {
	"ti,tsc2007",
	"edt,edt-ft5x06",
	"ti,am3359-tscadc",
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
	fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));
	fdt_fixup_ethernet(blob);

	karo_fdt_fixup_touchpanel(blob, tx48_touchpanels,
				ARRAY_SIZE(tx48_touchpanels));
	karo_fdt_fixup_usb_otg(blob, "usb0", "phys", "vcc-supply");
	karo_fdt_fixup_flexcan(blob, stk5_v5);

	karo_fdt_update_fb_mode(blob, video_mode);

	tx48_disable_watchdog();

	if (get_cpu_rev() == 0) {
		karo_fdt_del_prop(blob, "lltc,ltc3589-2", 0x34, "interrupts");
		karo_fdt_del_prop(blob, "lltc,ltc3589-2", 0x34,
				"interrupt-parent");
	}

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */
