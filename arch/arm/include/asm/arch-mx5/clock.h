/*
 * (C) Copyright 2009
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ASM_ARCH_CLOCK_H
#define __ASM_ARCH_CLOCK_H

#include <common.h>

#ifdef CONFIG_SYS_MX5_HCLK
#define MXC_HCLK	CONFIG_SYS_MX5_HCLK
#else
#define MXC_HCLK	24000000
#endif

#ifdef CONFIG_SYS_MX5_CLK32
#define MXC_CLK32	CONFIG_SYS_MX5_CLK32
#else
#define MXC_CLK32	32768
#endif

enum mxc_clock {
	MXC_ARM_CLK = 0,
	MXC_AHB_CLK,
	MXC_IPG_CLK,
	MXC_IPG_PERCLK,
	MXC_UART_CLK,
	MXC_CSPI_CLK,
	MXC_ESDHC_CLK,
	MXC_ESDHC2_CLK,
	MXC_ESDHC3_CLK,
	MXC_ESDHC4_CLK,
	MXC_FEC_CLK,
	MXC_SATA_CLK,
	MXC_DDR_CLK,
	MXC_NFC_CLK,
	MXC_PERIPH_CLK,
	MXC_I2C_CLK,
};


struct clk {
	const char *name;
	int id;
	/* Source clock this clk depends on */
	struct clk *parent;
	/* Secondary clock to enable/disable with this clock */
	struct clk *secondary;
	/* Current clock rate */
	unsigned long rate;
	/* Reference count of clock enable/disable */
	__s8 usecount;
	/* Register bit position for clock's enable/disable control. */
	u8 enable_shift;
	/* Register address for clock's enable/disable control. */
	void *enable_reg;
	u32 flags;
	/*
	 * Function ptr to recalculate the clock's rate based on parent
	 * clock's rate
	 */
	void (*recalc) (struct clk *);
	/*
	 * Function ptr to set the clock to a new rate. The rate must match a
	 * supported rate returned from round_rate. Leave blank if clock is not
	* programmable
	 */
	int (*set_rate) (struct clk *, unsigned long);
	/*
	 * Function ptr to round the requested clock rate to the nearest
	 * supported rate that is less than or equal to the requested rate.
	 */
	unsigned long (*round_rate) (struct clk *, unsigned long);
	/*
	 * Function ptr to enable the clock. Leave blank if clock can not
	 * be gated.
	 */
	int (*enable) (struct clk *);
	/*
	 * Function ptr to disable the clock. Leave blank if clock can not
	 * be gated.
	 */
	void (*disable) (struct clk *);
	/* Function ptr to set the parent clock of the clock. */
	int (*set_parent) (struct clk *, struct clk *);
};

u32 imx_get_uartclk(void);
u32 imx_get_fecclk(void);
unsigned int mxc_get_clock(enum mxc_clock clk);
int mxc_set_clock(u32 ref, u32 freq, enum mxc_clock clk);
void set_usb_phy_clk(void);
void enable_usb_phy1_clk(unsigned char enable);
void enable_usb_phy2_clk(unsigned char enable);
void set_usboh3_clk(void);
void enable_usboh3_clk(unsigned char enable);
void mxc_set_sata_internal_clock(void);
int enable_i2c_clk(unsigned char enable, unsigned i2c_num);
void enable_nfc_clk(unsigned char enable);
void ipu_clk_enable(void);
void ipu_clk_disable(void);
void ipu_di_clk_enable(int di);
void ipu_di_clk_disable(int di);
void ldb_clk_enable(int ldb);
void ldb_clk_disable(int ldb);

#endif /* __ASM_ARCH_CLOCK_H */
