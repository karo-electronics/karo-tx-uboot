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
#define UART_RESETDONE		(1 << 0)
#define UART_IDLE_MODE(m)	(((m) << 3) & UART_IDLE_MODE_MASK)
#define UART_IDLE_MODE_MASK	(0x3 << 3)

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

DECLARE_GLOBAL_DATA_PTR;

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
static void enable_uart0_pin_mux(void)
{
	tx48_set_pin_mux(tx48_uart0_pins, ARRAY_SIZE(tx48_uart0_pins));
}

static void enable_mmc0_pin_mux(void)
{
	tx48_set_pin_mux(tx48_mmc_pins, ARRAY_SIZE(tx48_mmc_pins));
}

#define SDRAM_CLK		CONFIG_SYS_DDR_CLK

#define ns_TO_ck(ns)		(((ns) * SDRAM_CLK + 999) / 1000)
#define ck_TO_ns(ck)		((ck) * 1000 / SDRAM_CLK)

#ifdef DEBUG
static inline unsigned ck_val_check(unsigned ck, unsigned offs, unsigned max,
			const char *name)
{
	if (ck < offs) {
		printf("value %u for parameter %s is out of range (min: %u\n",
			ck, name, offs);
		hang();
	}
	if (ck > max) {
		printf("value %u for parameter %s is out of range (max: %u\n",
			ck, name, max);
		hang();
	}
	return ck - offs;
}
#define CK_VAL(ck, offs, max)	ck_val_check(ck, offs, max, #ck)
#else
#define CK_VAL(ck, offs, max)	((ck) - (offs))
#endif

#define DDR3_NT5CB128		1
#define DDR3_H5TQ2G8		2

#if 1
#define SDRAM_TYPE DDR3_NT5CB128
#else
#define SDRAM_TYPE DDR3_H5TQ2G8
#endif

#ifndef SDRAM_TYPE
#error No SDRAM_TYPE specified
#elif (SDRAM_TYPE == DDR3_NT5CB128) || (SDRAM_TYPE == DDR3_H5TQ2G8)
#define tRP			ns_TO_ck(14)
#define tRCD			ns_TO_ck(14)
#define tWR			ns_TO_ck(15)
#define tRAS			ns_TO_ck(35)
#define tRC			ns_TO_ck(49)
#define tRRD			max(ns_TO_ck(8), 4)
#define tWTR			max(ns_TO_ck(8), 4)

#define tXP			max(ns_TO_ck(6), 3)
#define tXPR			max(5, ns_TO_ck(ck_TO_ns(tRFC + 1) + 10))
#define tODT			ns_TO_ck(9)
#define tXSNR			max(5, ns_TO_ck(ck_TO_ns(tRFC + 1) + 10))
#define tXSRD			512
#define tRTP			max(ns_TO_ck(8), 4)
#define tCKE			max(ns_TO_ck(6), 3)

#define tPDLL_UL		512
#define tZQCS			64
#define tRFC			ns_TO_ck(160)
#define tRAS_MAX		0xf

static inline int cwl(u32 sdram_clk)
{
	if (sdram_clk <= 300)
		return 5;
	else if (sdram_clk > 300 && sdram_clk <= 333)
		return 5;
	else if (sdram_clk > 333 && sdram_clk <= 400)
		return 5;
	else if (sdram_clk > 400 && sdram_clk <= 533)
		return 6;
	else if (sdram_clk > 533 && sdram_clk <= 666)
		return 7;
	else if (SDRAM_TYPE != DDR3_H5TQ2G8)
		;
	else if (sdram_clk > 666 && sdram_clk <= 800)
		return 8;

	printf("SDRAM clock out of range\n");
	hang();
}
#define CWL cwl(SDRAM_CLK)

static inline int cl(u32 sdram_clk)
{
	if (sdram_clk <= 300)
		return 5;
	else if (sdram_clk > 300 && sdram_clk <= 333)
		return 5;
	else if (sdram_clk > 333 && sdram_clk <= 400)
		return 6;
	else if (sdram_clk > 400 && sdram_clk <= 533)
		return 8;
	else if (sdram_clk > 533 && sdram_clk <= 666)
		return (SDRAM_TYPE == DDR3_H5TQ2G8) ? 10 : 9;
	else if (SDRAM_TYPE != DDR3_H5TQ2G8)
		;
	else if (sdram_clk > 666 && sdram_clk <= 800)
		return 11;

	printf("SDRAM clock out of range\n");
	hang();
}
#define CL cl(SDRAM_CLK)

#define ROW_ADDR_BITS		14
#define SDRAM_PG_SIZE		1024
#else
#error Unsupported SDRAM_TYPE specified
#endif

#define SDRAM_CONFIG_VAL	(					\
		(3 << 29) /* SDRAM type: 0: DDR1 1: LPDDR1 2: DDR2 3: DDR3 */ | \
		(0 << 27) /* IBANK pos */ |				\
		(2 << 24) /* termination resistor value 0: disable 1: RZQ/4 2: RZQ/2 3: RZQ/6 4: RZQ/12 5: RZQ/8 */ | \
		(0 << 23) /* DDR2 differential DQS */ |			\
		(1 << 21) /* dynamic ODT 0: off 1: RZQ/4 2: RZQ/2 */ |	\
		(0 << 20) /* DLL disable */ |				\
		(1 << 18) /* drive strength 0: RZQ/6 1: RZQ/7 */ |	\
		((CWL - 5) << 16) /* CWL 0: 5 ... 3: 8 */ |		\
		(1 << 14) /* SDRAM data bus width 0: 32 1: 16 */ |	\
		(((CL - 4) * 2) << 10) /* CAS latency 2: 5 4: 6 6: 8 ... 14: 11 (DDR3) */ | \
		((ROW_ADDR_BITS - 9) << 7) /* # of row addr bits 0: 9 ... 7: 16 */ | \
		(3 << 4) /* # of SDRAM internal banks 0: 1 1: 2 2: 4 3: 8 */ | \
		(0 << 3) /* # of CS lines */ |				\
		((ffs(SDRAM_PG_SIZE / 256) - 1) << 0) /* page size 0: 256 1: 512 2: 1024 3:2048 */ | \
		0)

#define SDREF_VAL		(					\
		(0 << 31) /* */ |					\
		(1 << 29) /* self refresh temperature range 1: extended temp range */ | \
		(0 << 28) /* auto self refresh enable */ |		\
		(0 << 24) /* partial array self refresh */ |		\
		((SDRAM_CLK * 7800 / 1000) << 0) /* refresh interval */ | \
		0)

#define tFAW		ns_TO_ck(45)

#define SDRAM_TIM1_VAL	((CK_VAL(tRP, 1, 16) << 25) |	\
			 (CK_VAL(tRCD, 1, 16) << 21) |	\
			 (CK_VAL(tWR, 1, 16) << 17) |	\
			 (CK_VAL(tRAS, 1, 32) << 12) |	\
			 (CK_VAL(tRC, 1, 64) << 6) |	\
			 (CK_VAL(tRRD, 1, 8) << 3) |	\
			 (CK_VAL(tWTR, 1, 8) << 0))

#define SDRAM_TIM2_VAL	((CK_VAL(max(tCKE, tXP), 1, 8) << 28) |	\
			 (CK_VAL(tODT, 0, 8) << 25) |		\
			 (CK_VAL(tXSNR, 1, 128) << 16) |	\
			 (CK_VAL(tXSRD, 1, 1024) << 6) |	\
			 (CK_VAL(tRTP, 1, 8) << 3) |		\
			 (CK_VAL(tCKE, 1, 8) << 0))

#define SDRAM_TIM3_VAL	((CK_VAL(DIV_ROUND_UP(tPDLL_UL, 128), 0, 16) << 28) | \
			 (CK_VAL(tZQCS, 1, 64) << 15) |			\
			 (CK_VAL(tRFC, 1, 1024) << 4) |			\
			 (CK_VAL(tRAS_MAX, 0, 16) << 0))

#define ZQ_CONFIG_VAL		(					\
		(1 << 31) /* ZQ calib for CS1 */ |			\
		(0 << 30) /* ZQ calib for CS0 */ |			\
		(0 << 29) /* dual calib */ |				\
		(1 << 28) /* ZQ calib on SR/PWDN exit */ |		\
		(2 << 18) /* ZQCL intervals for ZQINIT */ |		\
		(4 << 16) /* ZQCS intervals for ZQCL */ |		\
		(80 << 0) /* refr periods between ZQCS commands */ |	\
		0)

static struct ddr_data tx48_ddr3_data = {
	/* reset defaults */
	.datardsratio0 = 0x04010040,
	.datawdsratio0 = 0x0,
	.datafwsratio0 = 0x0,
	.datawrsratio0 = 0x04010040,
	.datadldiff0 = 0x4,
};

static struct cmd_control tx48_ddr3_cmd_ctrl_data = {
	/* reset defaults */
	.cmd0csratio = 0x80,
	.cmd0dldiff = 0x04,
	.cmd1csratio = 0x80,
	.cmd1dldiff = 0x04,
	.cmd2csratio = 0x80,
	.cmd2dldiff = 0x04,
};

static void ddr3_calib_start(void)
{
	static struct emif_reg_struct *emif_reg = (void *)EMIF4_0_CFG_BASE;
	int loops = 0;
	u32 regval;
	u32 emif_status;

	debug("Starting DDR3 calibration\n");

	/* wait for DDR PHY ready */
	while (!((emif_status = readl(&emif_reg->emif_status)) & (1 << 2))) {
		if (loops++ > 100000)
			break;
		udelay(1);
	}
	debug("EMIF status: %08x after %u loops\n", emif_status, loops);

	/* enable DDR3 write levelling */
	loops = 0;
	writel(EMIF_REG_RDWRLVLFULL_START_MASK, &emif_reg->emif_rd_wr_lvl_ctl);
	do {
		regval = readl(&emif_reg->emif_rd_wr_lvl_ctl);
		if (!(regval & EMIF_REG_RDWRLVLFULL_START_MASK))
			break;
		udelay(1);
	} while (loops++ < 100000);
	if (regval & EMIF_REG_RDWRLVLFULL_START_MASK) {
		printf("Full WRLVL timed out\n");
	} else {
		debug("Full Write Levelling done after %u us\n", loops);
	}
	writel(0, &emif_reg->emif_rd_wr_lvl_rmp_ctl);
	writel(0, &emif_reg->emif_rd_wr_lvl_rmp_win);
	writel(0x0f808080, &emif_reg->emif_rd_wr_lvl_ctl);
	debug("DDR3 calibration done\n");
}

static void tx48_ddr_init(void)
{
	struct emif_regs r = {0};

	debug("Initialising SDRAM timing for %u MHz DDR clock\n", SDRAM_CLK);

	r.sdram_config = SDRAM_CONFIG_VAL;
	r.ref_ctrl = SDREF_VAL;
	r.sdram_tim1 = SDRAM_TIM1_VAL;
	r.sdram_tim2 = SDRAM_TIM2_VAL;
	r.sdram_tim3 = SDRAM_TIM3_VAL;
	r.zq_config = ZQ_CONFIG_VAL;
	r.emif_ddr_phy_ctlr_1 = 0x0000030b;

	config_ddr(SDRAM_CLK, 0x04, &tx48_ddr3_data,
		&tx48_ddr3_cmd_ctrl_data, &r, 0);

	ddr3_calib_start();

	debug("%s: config_ddr done\n", __func__);
}

#ifdef CONFIG_HW_WATCHDOG
static inline void tx48_wdog_disable(void)
{
}
#else
static inline void tx48_wdog_disable(void)
{
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
}
#endif

void s_init(void)
{
	struct uart_sys *uart_base = (struct uart_sys *)DEFAULT_UART_BASE;
	int timeout = 1000;

	gd = &gdata;

	/*
         * Save the boot parameters passed from romcode.
         * We cannot delay the saving further than this,
         * to prevent overwrites.
         */
	save_omap_boot_params();

	/* Setup the PLLs and the clocks for the peripherals */
	pll_init();

	tx48_wdog_disable();

	enable_uart0_pin_mux();

	/* UART softreset */
	writel(readl(&uart_base->uartsyscfg) | UART_RESET,
		&uart_base->uartsyscfg);
	while (!(readl(&uart_base->uartsyssts) & UART_RESETDONE)) {
		udelay(1);
		if (timeout-- <= 0)
			break;
	}

	/* Disable smart idle */
	writel((readl(&uart_base->uartsyscfg) & ~UART_IDLE_MODE_MASK) |
		UART_IDLE_MODE(1), &uart_base->uartsyscfg);

	preloader_console_init();

	if (timeout <= 0)
		printf("Timeout waiting for UART RESET\n");

	timer_init();

	tx48_ddr_init();

	gpmc_init();

	/* Enable MMC0 */
	enable_mmc0_pin_mux();

	gpio_request_array(tx48_gpios, ARRAY_SIZE(tx48_gpios));
	tx48_set_pin_mux(tx48_pins, ARRAY_SIZE(tx48_pins));
}
