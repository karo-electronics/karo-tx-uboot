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
#include <cpsw.h>
#include <serial.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <nand.h>
#include <net.h>
#include <spl.h>
#include <linux/mtd/nand.h>
#include <asm/gpio.h>
#include <asm/cache.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/hardware.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/mux.h>
#include <asm/arch/ddr_defs.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clock.h>
#include <asm/arch/clocks_am33xx.h>
#include <asm/arch/mem.h>
#include <video_fb.h>
#include <asm/arch/da8xx-fb.h>

#include "flash.h"

DECLARE_GLOBAL_DATA_PTR;

#define TX48_LCD_BACKLIGHT_GPIO	AM33XX_GPIO_NR(3, 14)

#define UART_RESETDONE		(1 << 0)
#define UART_IDLE_MODE(m)	(((m) << 3) & UART_IDLE_MODE_MASK)
#define UART_IDLE_MODE_MASK	(0x3 << 3)

struct pin_mux {
	short reg_offset;
	uint8_t val;
};

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

static struct pin_mux tx48_pins[] = {
#ifdef CONFIG_NAND
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

static struct gpio tx48_gpios[] = {
	/* configure this pin early to prevent flicker of the LCD */
	{ TX48_LCD_BACKLIGHT_GPIO, GPIOFLAG_OUTPUT_INIT_HIGH, "LCD BACKLIGHT", },
};

void set_mux_conf_regs(void)
{
	gpio_request_array(tx48_gpios, ARRAY_SIZE(tx48_gpios));
	tx48_set_pin_mux(tx48_pins, ARRAY_SIZE(tx48_pins));
}

static struct pin_mux tx48_uart_pins[] = {
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
void set_uart_mux_conf(void)
{
	tx48_set_pin_mux(tx48_uart_pins, ARRAY_SIZE(tx48_uart_pins));
}

static const u32 gpmc_nand_cfg[GPMC_MAX_REG] = {
	TX48_NAND_GPMC_CONFIG1,
	TX48_NAND_GPMC_CONFIG2,
	TX48_NAND_GPMC_CONFIG3,
	TX48_NAND_GPMC_CONFIG4,
	TX48_NAND_GPMC_CONFIG5,
	TX48_NAND_GPMC_CONFIG6,
};

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

#if 1
static struct ddr_data tx48_ddr3_data = {
	/* reset defaults */
	.datardsratio0 = 0x04010040,
	.datawdsratio0 = 0x0,
	.datafwsratio0 = 0x0,
	.datawrsratio0 = 0x04010040,
};

static struct cmd_control tx48_ddr3_cmd_ctrl_data = {
	/* reset defaults */
	.cmd0csratio = 0x80,
	.cmd1csratio = 0x80,
	.cmd2csratio = 0x80,
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

void sdram_init(void)
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

	config_ddr(SDRAM_CLK, NULL, &tx48_ddr3_data,
		&tx48_ddr3_cmd_ctrl_data, &r, 0);

	ddr3_calib_start();

	debug("%s: config_ddr done\n", __func__);
}
#endif
#if 0
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
#endif

#define OSC     (V_OSCK/1000000)
static const struct dpll_params tx48_ddr_params = {
	266, OSC-1, 1, -1, -1, -1, -1,
};

const struct dpll_params *get_dpll_ddr_params(void)
{
	return &tx48_ddr_params;
}

void am33xx_spl_board_init(void)
{
	struct gpmc *gpmc_cfg = (struct gpmc *)GPMC_BASE;

	enable_gpmc_cs_config(gpmc_nand_cfg, &gpmc_cfg->cs[0],
			CONFIG_SYS_NAND_BASE, CONFIG_SYS_NAND_SIZE);
}
