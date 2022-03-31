// SPDX-License-Identifier: GPL-2.0
/*
 * arch/arm/cpu/armv7/rmobile/cpu_info-rcar.c
 *
 * Copyright (C) 2013,2014 Renesas Electronics Corporation
 */
#include <common.h>
#include <asm/io.h>
#include <linux/delay.h>

#define PRR_MASK		0x7fff
#define R8A7796_REV_1_0		0x5200
#define R8A7796_REV_1_1		0x5210

// for reset
#define CPG_BASE_ADDRESS 0x11010000 // H’1101_0000
#define CPG_WDTRST_SEL 0xb14
#define WDT_CH0_BASE 0x12800800
#define WDT_DEFAULT_TIMEOUT 2U
#define F2CYCLE_NSEC(f) (1000000000/f)
#define WDT_CYCLE_MSEC(f, wdttime) ((1024 * 1024 * ((u64)wdttime + 1)) \
                                / ((u64)(f) / 1000000))
#define xinclk 24000000
#define clk_rate 100000000
#define WDTSET 0x04
#define WDTTIM 0x08
#define WDTCNT 0x00
#define WDTINT 0x0C
#define MAX_VAL 0xffff

static u32 rmobile_get_prr(void)
{
#ifdef CONFIG_RCAR_GEN3
	return readl(0xFFF00044);
#else
	return readl(0xFF000044);
#endif
}

u32 rmobile_get_cpu_type(void)
{
	return (rmobile_get_prr() & 0x00007F00) >> 8;
}

u32 rmobile_get_cpu_rev_integer(void)
{
	const u32 prr = rmobile_get_prr();

	if ((prr & PRR_MASK) == R8A7796_REV_1_1)
		return 1;
	else
		return ((prr & 0x000000F0) >> 4) + 1;
}

u32 rmobile_get_cpu_rev_fraction(void)
{
	const u32 prr = rmobile_get_prr();

	if ((prr & PRR_MASK) == R8A7796_REV_1_1)
		return 1;
	else
		return prr & 0x0000000F;
}

void wdt_delay(void)
{
	u32 delay =  F2CYCLE_NSEC(xinclk)*6 + F2CYCLE_NSEC(clk_rate)*9;
	ndelay(delay);
}

void reset_cpu(ulong addr)
{
	// select CPG_WDTRST_SEL to use wdt reset
	writel(0x07770777, CPG_BASE_ADDRESS + CPG_WDTRST_SEL);
	mdelay(50);
	// clear wdt interrupt
	writel(BIT(0), WDT_CH0_BASE + WDTINT);
	wdt_delay();
	/* Initialize timeout value */
	/* We don't care timeout value becase we want to WDT expire ASAP*/
	u32 time_out = (WDT_DEFAULT_TIMEOUT/2) * (1000)/WDT_CYCLE_MSEC(xinclk,0);
	/* Setting period time register only 12 bit set in WDTSET[31:20] */
	time_out <<= 20;
	/* Delay timer before setting watchdog counter*/
	wdt_delay();
	time_out &= 0xFFF00000;
	writel(time_out, WDT_CH0_BASE + WDTSET);
	// Set max value to WDT Lapsed Time Register
	// to cause WDT expire ASAP
	wdt_delay();
	writel(MAX_VAL, WDT_CH0_BASE + WDTTIM);
	wdt_delay();
	// enable wdt timer
	writel(BIT(0), WDT_CH0_BASE + WDTCNT);
}
