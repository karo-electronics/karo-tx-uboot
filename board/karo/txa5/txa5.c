/*
 * Copyright (C) 2015 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: board/atmel/sama5d4_xplained/sama5d4_xplained.c
 *     Copyright (C) 2014 Atmel Bo Shen <voice.shen@atmel.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#define DEBUG

#include <common.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <lcd.h>
#include <netdev.h>
#include <mmc.h>
#include <net.h>
#include <nand.h>
#include <atmel_hlcdc.h>
#include <atmel_mci.h>
#include <linux/fb.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/at91_common.h>
#include <asm/arch/at91_pmc.h>
#include <asm/arch/at91_rstc.h>
#include <asm/arch/clk.h>
#include <asm/arch/sama5d3_smc.h>
#include <asm/arch/sama5d4.h>

#include "../common/karo.h"

#define TXA5_LCD_RST_GPIO	GPIO_PIN_PA(25)
#define TXA5_LCD_PWR_GPIO	GPIO_PIN_PB(15)
#define TXA5_LCD_BACKLIGHT_GPIO	GPIO_PIN_PA(24)

#define TXA5_ETH_RST_GPIO	GPIO_PIN_PD(30)
//#define TXA5_ETH_PWR_GPIO	GPIO_PIN_BU(7)
#define TXA5_ETH_INT_GPIO	GPIO_PIN_PE(1)

#define TXA5_LED_GPIO		GPIO_PIN_PD(29)

DECLARE_GLOBAL_DATA_PTR;

static int wdreset;

#ifdef CONFIG_NAND_ATMEL
static void txa5_nand_hw_init(void)
{
	struct at91_smc *smc = (void *)ATMEL_BASE_SMC;

	at91_periph_clk_enable(ATMEL_ID_SMC);

	/* Configure SMC CS3 for NAND */
	writel(AT91_SMC_SETUP_NWE(1) | AT91_SMC_SETUP_NCS_WR(1) |
	       AT91_SMC_SETUP_NRD(1) | AT91_SMC_SETUP_NCS_RD(1),
	       &smc->cs[3].setup);
	writel(AT91_SMC_PULSE_NWE(2) | AT91_SMC_PULSE_NCS_WR(3) |
	       AT91_SMC_PULSE_NRD(2) | AT91_SMC_PULSE_NCS_RD(3),
	       &smc->cs[3].pulse);
	writel(AT91_SMC_CYCLE_NWE(5) | AT91_SMC_CYCLE_NRD(5),
	       &smc->cs[3].cycle);
	writel(AT91_SMC_TIMINGS_TCLR(2) | AT91_SMC_TIMINGS_TADL(7) |
	       AT91_SMC_TIMINGS_TAR(2)  | AT91_SMC_TIMINGS_TRR(3)   |
	       AT91_SMC_TIMINGS_TWB(7)  | AT91_SMC_TIMINGS_RBNSEL(3)|
	       AT91_SMC_TIMINGS_NFSEL(1), &smc->cs[3].timings);
	writel(AT91_SMC_MODE_RM_NRD | AT91_SMC_MODE_WM_NWE |
	       AT91_SMC_MODE_EXNW_DISABLE |
	       AT91_SMC_MODE_DBW_8 |
	       AT91_SMC_MODE_TDF_CYCLE(3),
	       &smc->cs[3].mode);

	at91_set_a_periph(AT91_PIO_PORTC, 5, 0);	/* D0 */
	at91_set_a_periph(AT91_PIO_PORTC, 6, 0);	/* D1 */
	at91_set_a_periph(AT91_PIO_PORTC, 7, 0);	/* D2 */
	at91_set_a_periph(AT91_PIO_PORTC, 8, 0);	/* D3 */
	at91_set_a_periph(AT91_PIO_PORTC, 9, 0);	/* D4 */
	at91_set_a_periph(AT91_PIO_PORTC, 10, 0);	/* D5 */
	at91_set_a_periph(AT91_PIO_PORTC, 11, 0);	/* D6 */
	at91_set_a_periph(AT91_PIO_PORTC, 12, 0);	/* D7 */
	at91_set_a_periph(AT91_PIO_PORTC, 13, 0);	/* RE */
	at91_set_a_periph(AT91_PIO_PORTC, 14, 0);	/* WE */
	at91_set_a_periph(AT91_PIO_PORTC, 15, 1);	/* NCS */
	at91_set_a_periph(AT91_PIO_PORTC, 16, 1);	/* RDY */
	at91_set_a_periph(AT91_PIO_PORTC, 17, 1);	/* ALE */
	at91_set_a_periph(AT91_PIO_PORTC, 18, 1);	/* CLE */
}
#endif

static inline void txa5_serial0_hw_init(void)
{
	at91_set_a_periph(AT91_PIO_PORTD, 13, 1);	/* TXD0 */
	at91_set_a_periph(AT91_PIO_PORTD, 12, 0);	/* RXD0 */
	at91_set_a_periph(AT91_PIO_PORTD, 10, 1);	/* CTS0 */
	at91_set_a_periph(AT91_PIO_PORTD, 11, 0);	/* RTS0 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_USART0);
}

static inline void txa5_serial1_hw_init(void)
{
	at91_set_a_periph(AT91_PIO_PORTD, 17, 1);	/* TXD1 */
	at91_set_a_periph(AT91_PIO_PORTD, 16, 0);	/* RXD1 */
	at91_set_a_periph(AT91_PIO_PORTD, 14, 1);	/* CTS1 */
	at91_set_a_periph(AT91_PIO_PORTD, 15, 0);	/* RTS1 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_USART1);
}

static inline void txa5_serial3_hw_init(void)
{
	at91_set_b_periph(AT91_PIO_PORTE, 17, 1);	/* TXD3 */
	at91_set_b_periph(AT91_PIO_PORTE, 16, 0);	/* RXD3 */
	at91_set_b_periph(AT91_PIO_PORTE, 5, 1);	/* CTS3 */
	at91_set_b_periph(AT91_PIO_PORTE, 24, 0);	/* RTS3 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_USART3);
}

#ifdef CONFIG_GENERIC_ATMEL_MCI
void txa5_mci1_hw_init(void)
{
	at91_set_c_periph(AT91_PIO_PORTE, 19, 1);	/* MCI1 CDA */
	at91_set_c_periph(AT91_PIO_PORTE, 20, 1);	/* MCI1 DA0 */
	at91_set_c_periph(AT91_PIO_PORTE, 21, 1);	/* MCI1 DA1 */
	at91_set_c_periph(AT91_PIO_PORTE, 22, 1);	/* MCI1 DA2 */
	at91_set_c_periph(AT91_PIO_PORTE, 23, 1);	/* MCI1 DA3 */
	at91_set_c_periph(AT91_PIO_PORTE, 18, 0);	/* MCI1 CLK */

	/*
	 * As the mci io internal pull down is too strong, so if the io needs
	 * external pull up, the pull up resistor will be very small, if so
	 * the power consumption will increase, so disable the interanl pull
	 * down to save the power.
	 */
	at91_set_pio_pulldown(AT91_PIO_PORTE, 18, 0);
	at91_set_pio_pulldown(AT91_PIO_PORTE, 19, 0);
	at91_set_pio_pulldown(AT91_PIO_PORTE, 20, 0);
	at91_set_pio_pulldown(AT91_PIO_PORTE, 21, 0);
	at91_set_pio_pulldown(AT91_PIO_PORTE, 22, 0);
	at91_set_pio_pulldown(AT91_PIO_PORTE, 23, 0);

	/* SD Card Detect */
	at91_set_pio_input(AT91_PIO_PORTE, 6, 1);
	at91_set_pio_deglitch(AT91_PIO_PORTE, 6, 1);

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_MCI1);
}
#endif

#ifdef CONFIG_MACB
void txa5_macb0_hw_init(void)
{
	at91_set_a_periph(AT91_PIO_PORTB, 0, 0);	/* ETXCK_EREFCK */
	at91_set_a_periph(AT91_PIO_PORTB, 6, 0);	/* ERXDV */
	at91_set_a_periph(AT91_PIO_PORTB, 8, 0);	/* ERX0 */
	at91_set_a_periph(AT91_PIO_PORTB, 9, 0);	/* ERX1 */
	at91_set_a_periph(AT91_PIO_PORTB, 7, 0);	/* ERXER */
	at91_set_a_periph(AT91_PIO_PORTB, 2, 0);	/* ETXEN */
	at91_set_a_periph(AT91_PIO_PORTB, 12, 0);	/* ETX0 */
	at91_set_a_periph(AT91_PIO_PORTB, 13, 0);	/* ETX1 */
	at91_set_a_periph(AT91_PIO_PORTB, 17, 0);	/* EMDIO */
	at91_set_a_periph(AT91_PIO_PORTB, 16, 0);	/* EMDC */

	at91_set_pio_input(AT91_PIO_PORTE, 1, 0);	/* IRQ */
	at91_set_pio_deglitch(AT91_PIO_PORTE, 1, 1);

	at91_set_pio_output(AT91_PIO_PORTD, 30, 0);	/* PHY RESET */
	at91_set_pio_pullup(AT91_PIO_PORTD, 30, 1);

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_GMAC0);
}
#endif

int board_early_init_f(void)
{
	at91_periph_clk_enable(ATMEL_ID_PIOA);
	at91_periph_clk_enable(ATMEL_ID_PIOB);
	at91_periph_clk_enable(ATMEL_ID_PIOC);
	at91_periph_clk_enable(ATMEL_ID_PIOD);
	at91_periph_clk_enable(ATMEL_ID_PIOE);

#ifdef CONFIG_USART_ID
#if CONFIG_USART_ID == ATMEL_ID_USART0
	txa5_serial0_hw_init();
#elif CONFIG_USART_ID == ATMEL_ID_USART1
	txa5_serial1_hw_init();
#elif CONFIG_USART_ID == ATMEL_ID_USART3
#else
#error No console UART defined
#endif
	txa5_serial3_hw_init();
#endif /* CONFIG_USART_ID */

	return 0;
}

/* called with default environment! */
int board_init(void)
{
	/* adress of boot parameters */
	gd->bd->bi_boot_params = CONFIG_SYS_SDRAM_BASE + 0x100;

#ifdef CONFIG_NAND_ATMEL
	txa5_nand_hw_init();
#endif
#ifdef CONFIG_GENERIC_ATMEL_MCI
	txa5_mci1_hw_init();
#endif
	return 0;
}

int dram_init(void)
{
	gd->ram_size = get_ram_size((void *)CONFIG_SYS_SDRAM_BASE,
				CONFIG_SYS_SDRAM_SIZE);
	return 0;
}

#ifdef CONFIG_GENERIC_ATMEL_MCI
int board_mmc_init(bd_t *bis)
{
	return atmel_mci_init((void *)ATMEL_BASE_MCI1);
}
#endif /* CONFIG_GENERIC_ATMEL_MCI */

#ifdef CONFIG_MACB
int board_eth_init(bd_t *bis)
{
	int ret;

	txa5_macb0_hw_init();

	/* delay at least 21ms for the PHY internal POR signal to deassert */
	udelay(22000);

	/* Deassert RESET to the external phy */
	at91_set_gpio_value(TXA5_ETH_RST_GPIO, 1);

	ret = macb_eth_initialize(0, (void *)ATMEL_BASE_GMAC0, 0x00);

	return ret;
}
#endif

#ifndef ETH_ALEN
#define ETH_ALEN      6
#endif
#define MAC_ID0_HI	(void *)0xfc060040
#define MAC_ID0_LO	(void *)0xfc060044

static void txa5_init_mac(void)
{
	uint8_t mac_addr[ETH_ALEN];
	uint32_t mac_hi, mac_lo;

	/* try reading mac address from efuse */
	mac_lo = __raw_readl(MAC_ID0_LO);
	mac_hi = __raw_readl(MAC_ID0_HI);

	mac_addr[0] = mac_hi & 0xFF;
	mac_addr[1] = (mac_hi >> 8) & 0xFF;
	mac_addr[2] = (mac_hi >> 16) & 0xFF;
	mac_addr[3] = (mac_hi >> 24) & 0xFF;
	mac_addr[4] = mac_lo & 0xFF;
	mac_addr[5] = (mac_lo >> 8) & 0xFF;

	if (!is_valid_ethaddr(mac_addr)) {
		printf("No valid MAC address programmed\n");
		return;
	}
	printf("MAC addr from fuse: %pM\n", mac_addr);
	eth_setenv_enetaddr("ethaddr", mac_addr);
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
		at91_set_gpio_value(TXA5_LED_GPIO, 1);
		led_state = LED_STATE_ON;
	} else {
		if (get_timer(last) > CONFIG_SYS_HZ) {
			last = get_timer(0);
			if (led_state == LED_STATE_ON) {
				at91_set_gpio_value(TXA5_LED_GPIO, 0);
			} else {
				at91_set_gpio_value(TXA5_LED_GPIO, 1);
			}
			led_state = 1 - led_state;
		}
	}
}

#ifdef CONFIG_LCD
vidinfo_t panel_info = {
	/* set to max. size supported by SoC */
	.vl_col = 2048,
	.vl_row = 2048,
	.vl_bpix = LCD_COLOR16,
	.vl_tft = 1,
	.mmio = (void *)ATMEL_BASE_LCDC,
};

#define FB_SYNC_OE_LOW_ACT	(1 << 31)
#define FB_SYNC_CLK_LAT_FALL	(1 << 30)

static struct fb_videomode txa5_fb_modes[] = {
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

static int lcd_enabled = 1;
static int lcd_bl_polarity;
int lcd_output_bpp;

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
		at91_set_gpio_value(TXA5_LCD_PWR_GPIO, 1);
		udelay(100);
		at91_set_gpio_value(TXA5_LCD_RST_GPIO, 1);
		udelay(300000);
		at91_set_gpio_value(TXA5_LCD_BACKLIGHT_GPIO,
				lcd_backlight_polarity());
	}
}

void lcd_disable(void)
{
}

void lcd_panel_disable(void)
{
	if (lcd_enabled) {
		debug("Switching LCD off\n");
		at91_set_gpio_value(TXA5_LCD_BACKLIGHT_GPIO,
				!lcd_backlight_polarity());
		at91_set_gpio_value(TXA5_LCD_PWR_GPIO, 0);
		at91_set_gpio_value(TXA5_LCD_RST_GPIO, 0);
	}
}

static void txa5_lcd_panel_setup(struct fb_videomode *fb)
{
	panel_info.vl_clk = PICOS2KHZ(fb->pixclock) * 1000;

	panel_info.vl_col = fb->xres;
	panel_info.vl_left_margin = fb->left_margin;
	panel_info.vl_hsync_len = fb->hsync_len;
	panel_info.vl_right_margin = fb->right_margin;

	panel_info.vl_row = fb->yres;
	panel_info.vl_upper_margin = fb->upper_margin;
	panel_info.vl_vsync_len = fb->vsync_len;
	panel_info.vl_lower_margin = fb->lower_margin;

	if (!(fb->sync & FB_SYNC_HOR_HIGH_ACT))
		panel_info.vl_sync |= LCDC_LCDCFG5_HSPOL;
	if (!(fb->sync & FB_SYNC_VERT_HIGH_ACT))
		panel_info.vl_sync |= LCDC_LCDCFG5_VSPOL;
	if (fb->sync & FB_SYNC_OE_LOW_ACT)
		panel_info.vl_sync |= LCDC_LCDCFG5_DISPPOL;

	panel_info.vl_clk_pol = !!(fb->sync & FB_SYNC_CLK_LAT_FALL);
}

static void txa5_lcd_hw_init(void)
{
	at91_set_pio_output(AT91_PIO_PORTA, 24, 0);	/* LCDPWM */
	at91_set_pio_output(AT91_PIO_PORTA, 25, 0);	/* LCD RST */
	at91_set_pio_output(AT91_PIO_PORTB, 15, 0);	/* LCD PWR */

	at91_set_a_periph(AT91_PIO_PORTA, 26, 0);	/* LCDVSYNC */
	at91_set_a_periph(AT91_PIO_PORTA, 27, 0);	/* LCDHSYNC */
	at91_set_a_periph(AT91_PIO_PORTA, 28, 0);	/* LCDDOTCK */
	at91_set_a_periph(AT91_PIO_PORTA, 29, 0);	/* LCDDEN */

	at91_set_a_periph(AT91_PIO_PORTA,  0, 0);	/* LCDD0 */
	at91_set_a_periph(AT91_PIO_PORTA,  1, 0);	/* LCDD1 */
	at91_set_a_periph(AT91_PIO_PORTA,  2, 0);	/* LCDD2 */
	at91_set_a_periph(AT91_PIO_PORTA,  3, 0);	/* LCDD3 */
	at91_set_a_periph(AT91_PIO_PORTA,  4, 0);	/* LCDD4 */
	at91_set_a_periph(AT91_PIO_PORTA,  5, 0);	/* LCDD5 */
	at91_set_a_periph(AT91_PIO_PORTA,  6, 0);	/* LCDD6 */
	at91_set_a_periph(AT91_PIO_PORTA,  7, 0);	/* LCDD7 */

	at91_set_a_periph(AT91_PIO_PORTA,  8, 0);	/* LCDD9 */
	at91_set_a_periph(AT91_PIO_PORTA,  9, 0);	/* LCDD8 */
	at91_set_a_periph(AT91_PIO_PORTA, 10, 0);	/* LCDD10 */
	at91_set_a_periph(AT91_PIO_PORTA, 11, 0);	/* LCDD11 */
	at91_set_a_periph(AT91_PIO_PORTA, 12, 0);	/* LCDD12 */
	at91_set_a_periph(AT91_PIO_PORTA, 13, 0);	/* LCDD13 */
	at91_set_a_periph(AT91_PIO_PORTA, 14, 0);	/* LCDD14 */
	at91_set_a_periph(AT91_PIO_PORTA, 15, 0);	/* LCDD15 */

	at91_set_a_periph(AT91_PIO_PORTA, 16, 0);	/* LCDD16 */
	at91_set_a_periph(AT91_PIO_PORTA, 17, 0);	/* LCDD17 */
	at91_set_a_periph(AT91_PIO_PORTA, 18, 0);	/* LCDD18 */
	at91_set_a_periph(AT91_PIO_PORTA, 19, 0);	/* LCDD19 */
	at91_set_a_periph(AT91_PIO_PORTA, 20, 0);	/* LCDD20 */
	at91_set_a_periph(AT91_PIO_PORTA, 21, 0);	/* LCDD21 */
	at91_set_a_periph(AT91_PIO_PORTA, 22, 0);	/* LCDD22 */
	at91_set_a_periph(AT91_PIO_PORTA, 23, 0);	/* LCDD23 */

	/* Enable clock */
	at91_periph_clk_enable(ATMEL_ID_LCDC);
}

#ifdef CONFIG_LCD_INFO
void lcd_show_board_info(void)
{
	ulong dram_size, nand_size;
	int i;
	char temp[32];

	lcd_printf("2015 Ka-Ro electronics GmbH\n");
	lcd_printf("%s CPU at %s MHz\n", get_cpu_name(),
		   strmhz(temp, get_cpu_clk_rate()));

	dram_size = 0;
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++)
		dram_size += gd->bd->bi_dram[i].size;

	nand_size = 0;
#ifdef CONFIG_NAND_ATMEL
	for (i = 0; i < CONFIG_SYS_MAX_NAND_DEVICE; i++)
		nand_size += nand_info[i].size;
#endif
	lcd_printf("%ld MB SDRAM, %ld MB NAND\n",
		   dram_size >> 20, nand_size >> 20);
}
#endif /* CONFIG_LCD_INFO */

unsigned int has_lcdc(void)
{
	int color_depth = 16;
	const char *video_mode = karo_get_vmode(getenv("video_mode"));
	const char *vm;
	unsigned long val;
	int refresh = 60;
	struct fb_videomode *p = &txa5_fb_modes[0];
	struct fb_videomode fb_mode;
	int xres_set = 0, yres_set = 0, bpp_set = 0, refresh_set = 0;
	int lcd_bus_width;

	if (!lcd_enabled) {
		debug("LCD disabled\n");
		return 0;
	}

	if (had_ctrlc() || wdreset) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		setenv("splashimage", NULL);
		return 0;
	}

	karo_fdt_move_fdt();

	if (video_mode == NULL) {
		debug("Disabling LCD\n");
		lcd_enabled = 0;
		return 0;
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
			return 0;
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
					case 16:
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
		for (p = &txa5_fb_modes[0]; p->name != NULL; p++) {
			printf(" %s", p->name);
		}
		printf("\n");
		return 0;
	}
	if (p->xres > panel_info.vl_col || p->yres > panel_info.vl_row) {
		printf("video resolution: %dx%d exceeds hardware limits: %dx%d\n",
			p->xres, p->yres, panel_info.vl_col, panel_info.vl_row);
		lcd_enabled = 0;
		return 0;
	}
	panel_info.vl_col = p->xres;
	panel_info.vl_row = p->yres;

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

	txa5_lcd_hw_init();

	lcd_bus_width = karo_fdt_get_lcd_bus_width(working_fdt, 24);
	switch (lcd_bus_width) {
	case 16:
	case 18:
	case 24:
		lcd_output_bpp = lcd_bus_width;
		break;

	default:
		lcd_enabled = 0;
		printf("Invalid LCD bus width: %d\n", lcd_bus_width);
		return 0;
	}
	if (karo_load_splashimage(0) == 0) {
		debug("Initializing FB driver\n");
		txa5_lcd_panel_setup(p);
		return 1;
	} else {
		debug("Skipping initialization of LCD controller\n");
		return 0;
	}
}
#else
#define lcd_enabled 0
#endif /* CONFIG_LCD */

static void stk5_board_init(void)
{
#if 0
	gpio_request_array(stk5_gpios, ARRAY_SIZE(stk5_gpios));
	txa5_set_pin_mux(stk5_pads, ARRAY_SIZE(stk5_pads));
#endif
}

static void stk5v3_board_init(void)
{
	stk5_board_init();
}

static void stk5v5_board_init(void)
{
	stk5_board_init();

#if 0
	gpio_request_array(stk5v5_gpios, ARRAY_SIZE(stk5v5_gpios));
	txa5_set_pin_mux(stk5v5_pads, ARRAY_SIZE(stk5v5_pads));
#endif
}

/* called with environment from NAND or MMC */
int board_late_init(void)
{
	int ret = 0;
	const char *baseboard;

	env_cleanup();

//	txa5_set_cpu_clock();

	if (had_ctrlc())
		setenv_ulong("safeboot", 1);
#if 0
	else if (prm_rstst & PRM_RSTST_WDT1_RST)
		setenv_ulong("wdreset", 1);
#endif
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
	txa5_init_mac();
	clear_ctrlc();
	return ret;
}

#if defined(CONFIG_OF_BOARD_SETUP)
#ifdef CONFIG_FDT_FIXUP_PARTITIONS
#include <jffs2/jffs2.h>
#include <mtd_node.h>
static struct node_info nodes[] = {
	{ "atmel,sama5d4-nand", MTD_DEV_TYPE_NAND, },
};
#else
#define fdt_fixup_mtdparts(b,n,c) do { } while (0)
#endif

static const char *txa5_touchpanels[] = {
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

	karo_fdt_fixup_touchpanel(blob, txa5_touchpanels,
				ARRAY_SIZE(txa5_touchpanels));
//	karo_fdt_fixup_usb_otg(blob, "usbotg", "fsl,usbphy", "vbus-supply");
//	karo_fdt_fixup_flexcan(blob, stk5_v5);
	karo_fdt_update_fb_mode(blob, video_mode);

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */
