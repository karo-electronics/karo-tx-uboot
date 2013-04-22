/*
 * clock.c
 *
 * clocks for AM33XX based boards
 *
 * Copyright (C) 2011, Texas Instruments, Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR /PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/hardware.h>
#include <asm/io.h>

#define PRCM_MOD_EN		0x2
#define PRCM_FORCE_WAKEUP	0x2
#define PRCM_FUNCTL		0x0

#define PRCM_EMIF_CLK_ACTIVITY	BIT(2)
#define PRCM_L3_GCLK_ACTIVITY	BIT(4)

#define PLL_BYPASS_MODE		0x4
#define ST_MN_BYPASS		0x00000100
#define ST_DPLL_CLK		0x00000001
#define CLK_SEL_MASK		0x7ffff
#define CLK_DIV_MASK		0x1f
#define CLK_DIV2_MASK		0x7f
#define CLK_SEL_SHIFT		0x8
#define CLK_MODE_MASK		0x7
#define CLK_MODE_SEL		0x7
#define DPLL_CLKDCOLDO_GATE_CTRL 0x300


const struct cm_perpll *cmper = (struct cm_perpll *)CM_PER;
const struct cm_wkuppll *cmwkup = (struct cm_wkuppll *)CM_WKUP;
const struct cm_dpll *cmdpll = (struct cm_dpll *)CM_DPLL;
const struct cm_rtc *cmrtc = (struct cm_rtc *)CM_RTC;

#ifdef CONFIG_SPL_BUILD
#define enable_clk(reg, val) __enable_clk(#reg, &reg, val)

static void __enable_clk(const char *name, const void *reg, u32 mask)
{
	unsigned long timeout = 10000000;

	writel(mask, reg);
	while (readl(reg) != mask)
		/* poor man's timeout, since timers not initialized */
		if (timeout-- == 0)
			/* no error message, since console not yet available */
			break;
}

static void enable_interface_clocks(void)
{
	/* Enable all the Interconnect Modules */
	enable_clk(cmper->l3clkctrl, PRCM_MOD_EN);
	enable_clk(cmper->l4lsclkctrl, PRCM_MOD_EN);
	enable_clk(cmper->l4fwclkctrl, PRCM_MOD_EN);
	enable_clk(cmwkup->wkl4wkclkctrl, PRCM_MOD_EN);
	enable_clk(cmper->l3instrclkctrl, PRCM_MOD_EN);
	enable_clk(cmper->l4hsclkctrl, PRCM_MOD_EN);
#ifdef CONFIG_HW_WATCHDOG
	enable_clk(cmwkup->wdtimer1ctrl, PRCM_MOD_EN);
#endif
	/* GPIO0 */
	enable_clk(cmwkup->wkgpio0clkctrl, PRCM_MOD_EN);
}

/*
 * Force power domain wake up transition
 * Ensure that the corresponding interface clock is active before
 * using the peripheral
 */
static void power_domain_wkup_transition(void)
{
	writel(PRCM_FORCE_WAKEUP, &cmper->l3clkstctrl);
	writel(PRCM_FORCE_WAKEUP, &cmper->l4lsclkstctrl);
	writel(PRCM_FORCE_WAKEUP, &cmwkup->wkclkstctrl);
	writel(PRCM_FORCE_WAKEUP, &cmper->l4fwclkstctrl);
	writel(PRCM_FORCE_WAKEUP, &cmper->l3sclkstctrl);
}

/*
 * Enable the peripheral clock for required peripherals
 */
static void enable_per_clocks(void)
{
	/* Enable the control module though RBL would have done it*/
	enable_clk(cmwkup->wkctrlclkctrl, PRCM_MOD_EN);
	/* Enable the timer2 clock */
	enable_clk(cmper->timer2clkctrl, PRCM_MOD_EN);
	/* Select the Master osc 24 MHZ as Timer2 clock source */
	writel(0x1, &cmdpll->clktimer2clk);

#ifdef CONFIG_SYS_NS16550_COM1
	/* UART0 */
	enable_clk(cmwkup->wkup_uart0ctrl, PRCM_MOD_EN);
#endif
#ifdef CONFIG_SYS_NS16550_COM2
	enable_clk(cmper->uart1clkctrl, PRCM_MOD_EN);
#endif
#ifdef CONFIG_SYS_NS16550_COM3
	enable_clk(cmper->uart2clkctrl, PRCM_MOD_EN);
#endif
#ifdef CONFIG_SYS_NS16550_COM4
	enable_clk(cmper->uart3clkctrl, PRCM_MOD_EN);
#endif
#ifdef CONFIG_SYS_NS16550_COM5
	enable_clk(cmper->uart4clkctrl, PRCM_MOD_EN);
#endif
#ifdef CONFIG_SYS_NS16550_COM6
	enable_clk(cmper->uart5clkctrl, PRCM_MOD_EN);
#endif
	/* GPMC */
	enable_clk(cmper->gpmcclkctrl, PRCM_MOD_EN);

	/* ELM */
	enable_clk(cmper->elmclkctrl, PRCM_MOD_EN);

	/* Ethernet */
	enable_clk(cmper->cpswclkstctrl, PRCM_MOD_EN);
	enable_clk(cmper->cpgmac0clkctrl, PRCM_MOD_EN);

	/* MMC */
#ifndef CONFIG_OMAP_MMC_DEV_0
	enable_clk(cmper->mmc0clkctrl, PRCM_MOD_EN);
#endif
#ifdef CONFIG_OMAP_MMC_DEV_1
	enable_clk(cmper->mmc1clkctrl, PRCM_MOD_EN);
#endif
	/* LCD */
	enable_clk(cmper->lcdclkctrl, PRCM_MOD_EN);

	/* i2c0 */
	enable_clk(cmwkup->wkup_i2c0ctrl, PRCM_MOD_EN);

	/* GPIO1-3 */
	enable_clk(cmper->gpio1clkctrl, PRCM_MOD_EN);
	enable_clk(cmper->gpio2clkctrl, PRCM_MOD_EN);
	enable_clk(cmper->gpio3clkctrl, PRCM_MOD_EN);

	/* i2c1 */
	enable_clk(cmper->i2c1clkctrl, PRCM_MOD_EN);

	/* spi0 */
	enable_clk(cmper->spi0clkctrl, PRCM_MOD_EN);

	/* rtc */
	enable_clk(cmrtc->rtcclkctrl, PRCM_MOD_EN);

	/* usb0 */
	enable_clk(cmper->usb0clkctrl, PRCM_MOD_EN);
}

static void mpu_pll_config(void)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddpllmpu);
	clksel = readl(&cmwkup->clkseldpllmpu);
	div_m2 = readl(&cmwkup->divm2dpllmpu);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddpllmpu);
	while (readl(&cmwkup->idlestdpllmpu) != ST_MN_BYPASS)
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (MPUPLL_M << CLK_SEL_SHIFT) | MPUPLL_N;
	writel(clksel, &cmwkup->clkseldpllmpu);

	div_m2 &= ~CLK_DIV_MASK;
	div_m2 |= MPUPLL_M2;
	writel(div_m2, &cmwkup->divm2dpllmpu);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllmpu);

	while (readl(&cmwkup->idlestdpllmpu) != ST_DPLL_CLK)
		;
}

static void core_pll_config(void)
{
	u32 clkmode, clksel, div_m4, div_m5, div_m6;

	clkmode = readl(&cmwkup->clkmoddpllcore);
	clksel = readl(&cmwkup->clkseldpllcore);
	div_m4 = readl(&cmwkup->divm4dpllcore);
	div_m5 = readl(&cmwkup->divm5dpllcore);
	div_m6 = readl(&cmwkup->divm6dpllcore);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddpllcore);

	while (readl(&cmwkup->idlestdpllcore) != ST_MN_BYPASS)
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= ((COREPLL_M << CLK_SEL_SHIFT) | COREPLL_N);
	writel(clksel, &cmwkup->clkseldpllcore);

	div_m4 &= ~CLK_DIV_MASK;
	div_m4 |= COREPLL_M4;
	writel(div_m4, &cmwkup->divm4dpllcore);

	div_m5 &= ~CLK_DIV_MASK;
	div_m5 |= COREPLL_M5;
	writel(div_m5, &cmwkup->divm5dpllcore);

	div_m6 &= ~CLK_DIV_MASK;
	div_m6 |= COREPLL_M6;
	writel(div_m6, &cmwkup->divm6dpllcore);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllcore);

	while (readl(&cmwkup->idlestdpllcore) != ST_DPLL_CLK)
		;
}

static void per_pll_config(void)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddpllper);
	clksel = readl(&cmwkup->clkseldpllper);
	div_m2 = readl(&cmwkup->divm2dpllper);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddpllper);

	while (readl(&cmwkup->idlestdpllper) != ST_MN_BYPASS)
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (PERPLL_M << CLK_SEL_SHIFT) | PERPLL_N;
	writel(clksel, &cmwkup->clkseldpllper);

	div_m2 &= ~CLK_DIV2_MASK;
	div_m2 |= PERPLL_M2;
	writel(div_m2, &cmwkup->divm2dpllper);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllper);

	while (readl(&cmwkup->idlestdpllper) != ST_DPLL_CLK)
		;

	writel(DPLL_CLKDCOLDO_GATE_CTRL, &cmwkup->clkdcoldodpllper);
}

static void disp_pll_config(void)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddplldisp);
	clksel = readl(&cmwkup->clkseldplldisp);
	div_m2 = readl(&cmwkup->divm2dplldisp);

	/* Set the PLL to bypass Mode */
	writel(PLL_BYPASS_MODE, &cmwkup->clkmoddplldisp);

	while (!(readl(&cmwkup->idlestdplldisp) & ST_MN_BYPASS))
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (DISPPLL_M << CLK_SEL_SHIFT) | DISPPLL_N;
	writel(clksel, &cmwkup->clkseldplldisp);

	div_m2 &= ~CLK_DIV2_MASK;
	div_m2 |= DISPPLL_M2;
	writel(div_m2, &cmwkup->divm2dplldisp);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddplldisp);

	while (!(readl(&cmwkup->idlestdplldisp) & ST_DPLL_CLK))
		;
}

void ddr_pll_config(unsigned int ddrpll_m)
{
	u32 clkmode, clksel, div_m2;

	clkmode = readl(&cmwkup->clkmoddpllddr);
	clksel = readl(&cmwkup->clkseldpllddr);
	div_m2 = readl(&cmwkup->divm2dpllddr);

	/* Set the PLL to bypass Mode */
	clkmode &= ~CLK_MODE_MASK;
	clkmode |= PLL_BYPASS_MODE;
	writel(clkmode, &cmwkup->clkmoddpllddr);

	/* Wait till bypass mode is enabled */
	while (!(readl(&cmwkup->idlestdpllddr) & ST_MN_BYPASS))
		;

	clksel &= ~CLK_SEL_MASK;
	clksel |= (ddrpll_m << CLK_SEL_SHIFT) | DDRPLL_N;
	writel(clksel, &cmwkup->clkseldpllddr);

	div_m2 &= ~CLK_DIV_MASK;
	div_m2 |= DDRPLL_M2;
	writel(div_m2, &cmwkup->divm2dpllddr);

	clkmode &= ~CLK_MODE_MASK;
	clkmode |= CLK_MODE_SEL;
	writel(clkmode, &cmwkup->clkmoddpllddr);

	/* Wait till dpll is locked */
	while ((readl(&cmwkup->idlestdpllddr) & ST_DPLL_CLK) != ST_DPLL_CLK)
		;
}

void enable_emif_clocks(void)
{
	/* Enable the  EMIF_FW Functional clock */
	writel(PRCM_MOD_EN, &cmper->emiffwclkctrl);
	/* Enable EMIF0 Clock */
	writel(PRCM_MOD_EN, &cmper->emifclkctrl);
	/* Poll if module is functional */
	while ((readl(&cmper->emifclkctrl)) != PRCM_MOD_EN)
		;
}

/*
 * Configure the PLL/PRCM for necessary peripherals
 */
void pll_init()
{
	mpu_pll_config();
	core_pll_config();
	per_pll_config();
	disp_pll_config();

	/* Enable the required interconnect clocks */
	enable_interface_clocks();

	/* Power domain wake up transition */
	power_domain_wkup_transition();

	/* Enable the required peripherals */
	enable_per_clocks();
}
#endif

#define M(mn) (((mn) & CLK_SEL_MASK) >> CLK_SEL_SHIFT)
#define N(mn) ((mn) & CLK_DIV2_MASK)

unsigned long __clk_get_rate(u32 m_n, u32 div_m2)
{
	unsigned long rate;

	div_m2 &= CLK_DIV_MASK;
	debug("M=%u N=%u M2=%u\n", M(m_n), N(m_n), div_m2);
	rate = V_OSCK / 1000 * M(m_n) / (N(m_n) + 1) / div_m2;
	debug("CLK = %lu.%03luMHz\n", rate / 1000, rate % 1000);
	return rate * 1000;
}

unsigned long lcdc_clk_rate(void)
{
	return clk_get_rate(cmwkup, disp);
}
