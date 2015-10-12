/*
 * (C) Copyright 2009
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __ASM_ARCH_CLOCK_H
#define __ASM_ARCH_CLOCK_H

#include <common.h>

#ifdef CONFIG_SYS_MX6_HCLK
#define MXC_HCLK	CONFIG_SYS_MX6_HCLK
#else
#define MXC_HCLK	24000000
#endif

#ifdef CONFIG_SYS_MX6_CLK32
#define MXC_CLK32	CONFIG_SYS_MX6_CLK32
#else
#define MXC_CLK32	32768
#endif

enum mxc_clock {
	MXC_ARM_CLK = 0,
	MXC_PER_CLK,
	MXC_AHB_CLK,
	MXC_IPG_CLK,
	MXC_IPG_PERCLK,
	MXC_UART_CLK,
	MXC_CSPI_CLK,
	MXC_AXI_CLK,
	MXC_EMI_SLOW_CLK,
	MXC_DDR_CLK,
	MXC_ESDHC_CLK,
	MXC_ESDHC2_CLK,
	MXC_ESDHC3_CLK,
	MXC_ESDHC4_CLK,
	MXC_SATA_CLK,
	MXC_NFC_CLK,
	MXC_I2C_CLK,
};

enum enet_freq {
	ENET_25MHZ,
	ENET_50MHZ,
	ENET_100MHZ,
	ENET_125MHZ,
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
void setup_gpmi_io_clk(u32 cfg);
void hab_caam_clock_enable(unsigned char enable);
void enable_ocotp_clk(unsigned char enable);
void enable_usboh3_clk(unsigned char enable);
void enable_uart_clk(unsigned char enable);
int enable_usdhc_clk(unsigned char enable, unsigned bus_num);
int enable_sata_clock(void);
void disable_sata_clock(void);
int enable_pcie_clock(void);
int enable_i2c_clk(unsigned char enable, unsigned i2c_num);
int enable_spi_clk(unsigned char enable, unsigned spi_num);
void enable_ipu_clock(void);
int enable_fec_anatop_clock(enum enet_freq freq);
void enable_enet_clk(unsigned char enable);
void enable_qspi_clk(int qspi_num);
void enable_thermal_clk(void);
void ipu_clk_enable(void);
void ipu_clk_disable(void);
void ipu_di_clk_enable(int di);
void ipu_di_clk_disable(int di);
void ldb_clk_enable(int ldb);
void ldb_clk_disable(int ldb);
#endif /* __ASM_ARCH_CLOCK_H */
