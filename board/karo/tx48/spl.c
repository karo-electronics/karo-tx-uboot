/*
 * board/karo/tx48/spl.c
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <errno.h>
#include <miiphy.h>
#include <netdev.h>
#include <serial.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <nand.h>
#include <net.h>
#include <spl.h>
#include <linux/mtd/nand.h>
#include <asm/gpio.h>
#include <asm/cache.h>
#include <asm/omap_common.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/nand.h>
#include <asm/arch/clock.h>
#include <video_fb.h>
#include <asm/arch/da8xx-fb.h>

#define TX48_LED_GPIO		AM33XX_GPIO_NR(1, 26)
#define TX48_ETH_PHY_RST_GPIO	AM33XX_GPIO_NR(3, 8)
#define TX48_LCD_RST_GPIO	AM33XX_GPIO_NR(1, 19)
#define TX48_LCD_PWR_GPIO	AM33XX_GPIO_NR(1, 22)
#define TX48_LCD_BACKLIGHT_GPIO	AM33XX_GPIO_NR(3, 14)

#define GMII_SEL		(CTRL_BASE + 0x650)

/* UART Defines */
#define UART_SYSCFG_OFFSET	0x54
#define UART_SYSSTS_OFFSET	0x58

#define UART_RESET		(0x1 << 1)
#define UART_CLK_RUNNING_MASK	0x1
#define UART_SMART_IDLE_EN	(0x1 << 0x3)

/* Timer Defines */
#define TSICR_REG		0x54
#define TIOCP_CFG_REG		0x10
#define TCLR_REG		0x38

/* RGMII mode define */
#define RGMII_MODE_ENABLE	0xA
#define RMII_MODE_ENABLE	0x5
#define MII_MODE_ENABLE		0x0

#define NO_OF_MAC_ADDR		1
#define ETH_ALEN		6

#define MUX_CFG(value, offset)	{					\
	__raw_writel(value, (CTRL_BASE + (offset)));			\
	}

/* PAD Control Fields */
#define SLEWCTRL	(0x1 << 6)
#define	RXACTIVE	(0x1 << 5)
#define	PULLUP_EN	(0x1 << 4) /* Pull UP Selection */
#define PULLUDEN	(0x0 << 3) /* Pull up enabled */
#define PULLUDDIS	(0x1 << 3) /* Pull up disabled */
#define MODE(val)	(val)

/*
 * PAD CONTROL OFFSETS
 * Field names corresponds to the pad signal name
 */
struct pad_signals {
	int gpmc_ad0;
	int gpmc_ad1;
	int gpmc_ad2;
	int gpmc_ad3;
	int gpmc_ad4;
	int gpmc_ad5;
	int gpmc_ad6;
	int gpmc_ad7;
	int gpmc_ad8;
	int gpmc_ad9;
	int gpmc_ad10;
	int gpmc_ad11;
	int gpmc_ad12;
	int gpmc_ad13;
	int gpmc_ad14;
	int gpmc_ad15;
	int gpmc_a0;
	int gpmc_a1;
	int gpmc_a2;
	int gpmc_a3;
	int gpmc_a4;
	int gpmc_a5;
	int gpmc_a6;
	int gpmc_a7;
	int gpmc_a8;
	int gpmc_a9;
	int gpmc_a10;
	int gpmc_a11;
	int gpmc_wait0;
	int gpmc_wpn;
	int gpmc_be1n;
	int gpmc_csn0;
	int gpmc_csn1;
	int gpmc_csn2;
	int gpmc_csn3;
	int gpmc_clk;
	int gpmc_advn_ale;
	int gpmc_oen_ren;
	int gpmc_wen;
	int gpmc_be0n_cle;
	int lcd_data0;
	int lcd_data1;
	int lcd_data2;
	int lcd_data3;
	int lcd_data4;
	int lcd_data5;
	int lcd_data6;
	int lcd_data7;
	int lcd_data8;
	int lcd_data9;
	int lcd_data10;
	int lcd_data11;
	int lcd_data12;
	int lcd_data13;
	int lcd_data14;
	int lcd_data15;
	int lcd_vsync;
	int lcd_hsync;
	int lcd_pclk;
	int lcd_ac_bias_en;
	int mmc0_dat3;
	int mmc0_dat2;
	int mmc0_dat1;
	int mmc0_dat0;
	int mmc0_clk;
	int mmc0_cmd;
	int mii1_col;
	int mii1_crs;
	int mii1_rxerr;
	int mii1_txen;
	int mii1_rxdv;
	int mii1_txd3;
	int mii1_txd2;
	int mii1_txd1;
	int mii1_txd0;
	int mii1_txclk;
	int mii1_rxclk;
	int mii1_rxd3;
	int mii1_rxd2;
	int mii1_rxd1;
	int mii1_rxd0;
	int rmii1_refclk;
	int mdio_data;
	int mdio_clk;
	int spi0_sclk;
	int spi0_d0;
	int spi0_d1;
	int spi0_cs0;
	int spi0_cs1;
	int ecap0_in_pwm0_out;
	int uart0_ctsn;
	int uart0_rtsn;
	int uart0_rxd;
	int uart0_txd;
	int uart1_ctsn;
	int uart1_rtsn;
	int uart1_rxd;
	int uart1_txd;
	int i2c0_sda;
	int i2c0_scl;
	int mcasp0_aclkx;
	int mcasp0_fsx;
	int mcasp0_axr0;
	int mcasp0_ahclkr;
	int mcasp0_aclkr;
	int mcasp0_fsr;
	int mcasp0_axr1;
	int mcasp0_ahclkx;
	int xdma_event_intr0;
	int xdma_event_intr1;
	int nresetin_out;
	int porz;
	int nnmi;
	int osc0_in;
	int osc0_out;
	int rsvd1;
	int tms;
	int tdi;
	int tdo;
	int tck;
	int ntrst;
	int emu0;
	int emu1;
	int osc1_in;
	int osc1_out;
	int pmic_power_en;
	int rtc_porz;
	int rsvd2;
	int ext_wakeup;
	int enz_kaldo_1p8v;
	int usb0_dm;
	int usb0_dp;
	int usb0_ce;
	int usb0_id;
	int usb0_vbus;
	int usb0_drvvbus;
	int usb1_dm;
	int usb1_dp;
	int usb1_ce;
	int usb1_id;
	int usb1_vbus;
	int usb1_drvvbus;
	int ddr_resetn;
	int ddr_csn0;
	int ddr_cke;
	int ddr_ck;
	int ddr_nck;
	int ddr_casn;
	int ddr_rasn;
	int ddr_wen;
	int ddr_ba0;
	int ddr_ba1;
	int ddr_ba2;
	int ddr_a0;
	int ddr_a1;
	int ddr_a2;
	int ddr_a3;
	int ddr_a4;
	int ddr_a5;
	int ddr_a6;
	int ddr_a7;
	int ddr_a8;
	int ddr_a9;
	int ddr_a10;
	int ddr_a11;
	int ddr_a12;
	int ddr_a13;
	int ddr_a14;
	int ddr_a15;
	int ddr_odt;
	int ddr_d0;
	int ddr_d1;
	int ddr_d2;
	int ddr_d3;
	int ddr_d4;
	int ddr_d5;
	int ddr_d6;
	int ddr_d7;
	int ddr_d8;
	int ddr_d9;
	int ddr_d10;
	int ddr_d11;
	int ddr_d12;
	int ddr_d13;
	int ddr_d14;
	int ddr_d15;
	int ddr_dqm0;
	int ddr_dqm1;
	int ddr_dqs0;
	int ddr_dqsn0;
	int ddr_dqs1;
	int ddr_dqsn1;
	int ddr_vref;
	int ddr_vtp;
	int ddr_strben0;
	int ddr_strben1;
	int ain7;
	int ain6;
	int ain5;
	int ain4;
	int ain3;
	int ain2;
	int ain1;
	int ain0;
	int vrefp;
	int vrefn;
};

struct pin_mux {
	short reg_offset;
	uint8_t val;
};

#define PAD_CTRL_BASE	0x800
#define OFFSET(x)	(unsigned int) (&((struct pad_signals *) \
				(PAD_CTRL_BASE))->x)

static struct pin_mux tx48_pins[] = {
#ifdef CONFIG_CMD_NAND
	{ OFFSET(gpmc_ad0), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD0 */
	{ OFFSET(gpmc_ad1), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD1 */
	{ OFFSET(gpmc_ad2), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD2 */
	{ OFFSET(gpmc_ad3), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD3 */
	{ OFFSET(gpmc_ad4), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD4 */
	{ OFFSET(gpmc_ad5), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD5 */
	{ OFFSET(gpmc_ad6), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD6 */
	{ OFFSET(gpmc_ad7), MODE(0) | PULLUP_EN | RXACTIVE, },	/* NAND AD7 */
	{ OFFSET(gpmc_wait0), MODE(0) | RXACTIVE | PULLUP_EN, }, /* NAND WAIT */
	{ OFFSET(gpmc_wpn), MODE(7) | PULLUP_EN | RXACTIVE, },	/* NAND_WPN */
	{ OFFSET(gpmc_csn0), MODE(0) | PULLUDEN, },	/* NAND_CS0 */
	{ OFFSET(gpmc_advn_ale), MODE(0) | PULLUDEN, }, /* NAND_ADV_ALE */
	{ OFFSET(gpmc_oen_ren), MODE(0) | PULLUDEN, },	/* NAND_OE */
	{ OFFSET(gpmc_wen), MODE(0) | PULLUDEN, },	/* NAND_WEN */
	{ OFFSET(gpmc_be0n_cle), MODE(0) | PULLUDEN, },	/* NAND_BE_CLE */
#endif
	/* I2C0 */
	{ OFFSET(i2c0_sda), MODE(0) | RXACTIVE | PULLUDEN | SLEWCTRL, }, /* I2C_DATA */
	{ OFFSET(i2c0_scl), MODE(0) | RXACTIVE | PULLUDEN | SLEWCTRL, }, /* I2C_SCLK */

#ifndef CONFIG_NO_ETH
	/* RMII1 */
	{ OFFSET(mii1_crs), MODE(1) | RXACTIVE, },      /* RMII1_CRS */
	{ OFFSET(mii1_rxerr), MODE(1) | RXACTIVE | PULLUDEN, },	 /* RMII1_RXERR */
	{ OFFSET(mii1_txen), MODE(1), },		     /* RMII1_TXEN */
	{ OFFSET(mii1_txd1), MODE(1), },		     /* RMII1_TXD1 */
	{ OFFSET(mii1_txd0), MODE(1), },		     /* RMII1_TXD0 */
	{ OFFSET(mii1_rxd1), MODE(1) | RXACTIVE | PULLUP_EN, },	/* RMII1_RXD1 */
	{ OFFSET(mii1_rxd0), MODE(1) | RXACTIVE | PULLUP_EN, },	/* RMII1_RXD0 */
	{ OFFSET(mdio_data), MODE(0) | RXACTIVE | PULLUP_EN, }, /* MDIO_DATA */
	{ OFFSET(mdio_clk), MODE(0) | PULLUP_EN, },	/* MDIO_CLK */
	{ OFFSET(rmii1_refclk), MODE(0) | RXACTIVE, },	/* RMII1_REFCLK */
	{ OFFSET(emu0), MODE(7) | RXACTIVE},	     /* nINT */
	{ OFFSET(emu1), MODE(7), },		     /* nRST */
#endif
};

static struct gpio tx48_gpios[] = {
	/* configure this pin early to prevent flicker of the LCD */
	{ TX48_LCD_BACKLIGHT_GPIO, GPIOF_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

static struct pin_mux tx48_mmc_pins[] = {
#ifdef CONFIG_OMAP_HSMMC
	/* MMC1 */
	{ OFFSET(mii1_rxd2), MODE(4) | RXACTIVE | PULLUP_EN, },	/* MMC1_DAT3 */
	{ OFFSET(mii1_rxd3), MODE(4) | RXACTIVE | PULLUP_EN, },	/* MMC1_DAT2 */
	{ OFFSET(mii1_rxclk), MODE(4) | RXACTIVE | PULLUP_EN, }, /* MMC1_DAT1 */
	{ OFFSET(mii1_txclk), MODE(4) | RXACTIVE | PULLUP_EN, }, /* MMC1_DAT0 */
	{ OFFSET(gpmc_csn1), MODE(2) | RXACTIVE | PULLUP_EN, },	/* MMC1_CLK */
	{ OFFSET(gpmc_csn2), MODE(2) | RXACTIVE | PULLUP_EN, },	/* MMC1_CMD */
	{ OFFSET(mcasp0_fsx), MODE(4) | RXACTIVE, },	/* MMC1_CD */
#endif
};

/*
 * Configure the pin mux for the module
 */
static inline void tx48_set_pin_mux(const struct pin_mux *pin_mux,
			int num_pins)
{
	int i;

	for (i = 0; i < num_pins; i++)
		MUX_CFG(pin_mux[i].val, pin_mux[i].reg_offset);
}

static struct pin_mux tx48_uart0_pins[] = {
#ifdef CONFIG_SYS_NS16550_COM1
	/* UART0 for early boot messages */
	{ OFFSET(uart0_rxd), MODE(0) | PULLUP_EN | RXACTIVE, },	/* UART0_RXD */
	{ OFFSET(uart0_txd), MODE(0) | PULLUDEN, },		/* UART0_TXD */
	{ OFFSET(uart0_ctsn), MODE(0) | PULLUP_EN | RXACTIVE, },/* UART0_CTS */
	{ OFFSET(uart0_rtsn), MODE(0) | PULLUDEN, },		/* UART0_RTS */
#endif
#ifdef CONFIG_SYS_NS16550_COM2
	/* UART1 */
	{ OFFSET(uart1_rxd), MODE(0) | PULLUP_EN | RXACTIVE, },	/* UART1_RXD */
	{ OFFSET(uart1_txd), MODE(0) | PULLUDEN, },		/* UART1_TXD */
	{ OFFSET(uart1_ctsn), MODE(0) | PULLUP_EN | RXACTIVE, },/* UART1_CTS */
	{ OFFSET(uart1_rtsn), MODE(0) | PULLUDEN, },		/* UART1_RTS */
#endif
#ifdef CONFIG_SYS_NS16550_COM3
	/* UART5 */
	{ OFFSET(mii1_rxdv), MODE(3) | PULLUP_EN | RXACTIVE, },	/* UART5_RXD */
	{ OFFSET(mii1_col), MODE(3) | PULLUDEN, },		/* UART5_TXD */
	{ OFFSET(mmc0_dat1), MODE(2) | PULLUP_EN | RXACTIVE, },	/* UART5_CTS */
	{ OFFSET(mmc0_dat0), MODE(2) | PULLUDEN, },		/* UART5_RTS */
#endif
};

/*
 * early system init of muxing and clocks.
 */
void enable_uart0_pin_mux(void)
{
	tx48_set_pin_mux(tx48_uart0_pins, ARRAY_SIZE(tx48_uart0_pins));
}

void enable_mmc0_pin_mux(void)
{
	tx48_set_pin_mux(tx48_mmc_pins, ARRAY_SIZE(tx48_mmc_pins));
}

static const struct ddr_data tx48_ddr3_data = {
	.datardsratio0 = MT41J128MJT125_RD_DQS,
	.datawdsratio0 = MT41J128MJT125_WR_DQS,
	.datafwsratio0 = MT41J128MJT125_PHY_FIFO_WE,
	.datawrsratio0 = MT41J128MJT125_PHY_WR_DATA,
	.datadldiff0 = PHY_DLL_LOCK_DIFF,
};

static const struct cmd_control tx48_ddr3_cmd_ctrl_data = {
	.cmd0csratio = MT41J128MJT125_RATIO,
	.cmd0dldiff = MT41J128MJT125_DLL_LOCK_DIFF,
	.cmd0iclkout = MT41J128MJT125_INVERT_CLKOUT,

	.cmd1csratio = MT41J128MJT125_RATIO,
	.cmd1dldiff = MT41J128MJT125_DLL_LOCK_DIFF,
	.cmd1iclkout = MT41J128MJT125_INVERT_CLKOUT,

	.cmd2csratio = MT41J128MJT125_RATIO,
	.cmd2dldiff = MT41J128MJT125_DLL_LOCK_DIFF,
	.cmd2iclkout = MT41J128MJT125_INVERT_CLKOUT,
};

static struct emif_regs tx48_ddr3_emif_reg_data = {
	.sdram_config = MT41J128MJT125_EMIF_SDCFG,
	.ref_ctrl = MT41J128MJT125_EMIF_SDREF,
	.sdram_tim1 = MT41J128MJT125_EMIF_TIM1,
	.sdram_tim2 = MT41J128MJT125_EMIF_TIM2,
	.sdram_tim3 = MT41J128MJT125_EMIF_TIM3,
	.zq_config = MT41J128MJT125_ZQ_CFG,
	.emif_ddr_phy_ctlr_1 = MT41J128MJT125_EMIF_READ_LATENCY,
};

void s_init(void)
{
#ifndef CONFIG_HW_WATCHDOG
	struct wd_timer *wdtimer = (struct wd_timer *)WDT_BASE;

	/* WDT1 is already running when the bootloader gets control
	 * Disable it to avoid "random" resets
	 */
	writel(0xAAAA, &wdtimer->wdtwspr);
	while (readl(&wdtimer->wdtwwps) != 0x0)
		;
	writel(0x5555, &wdtimer->wdtwspr);
	while (readl(&wdtimer->wdtwwps) != 0x0)
		;
#endif
	/* Setup the PLLs and the clocks for the peripherals */
	pll_init();

	/* UART softreset */
	u32 regVal;
	struct uart_sys *uart_base = (struct uart_sys *)DEFAULT_UART_BASE;

	enable_uart0_pin_mux();

	regVal = readl(&uart_base->uartsyscfg);
	regVal |= UART_RESET;
	writel(regVal, &uart_base->uartsyscfg);
	while ((readl(&uart_base->uartsyssts) &
		UART_CLK_RUNNING_MASK) != UART_CLK_RUNNING_MASK)
		;

	/* Disable smart idle */
	regVal = readl(&uart_base->uartsyscfg);
	regVal |= UART_SMART_IDLE_EN;
	writel(regVal, &uart_base->uartsyscfg);

	/* Initialize the Timer */
	timer_init();

	preloader_console_init();

	config_ddr(303, MT41J128MJT125_IOCTRL_VALUE, &tx48_ddr3_data,
		&tx48_ddr3_cmd_ctrl_data, &tx48_ddr3_emif_reg_data);

	/* Enable MMC0 */
	enable_mmc0_pin_mux();

	gpio_request_array(tx48_gpios, ARRAY_SIZE(tx48_gpios));
	tx48_set_pin_mux(tx48_pins, ARRAY_SIZE(tx48_pins));
}
