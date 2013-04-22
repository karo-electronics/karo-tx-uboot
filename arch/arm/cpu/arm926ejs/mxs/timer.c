/*
 * Freescale i.MX28 timer driver
 *
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
 *
 * Based on code from LTIB:
 * (C) Copyright 2009-2010 Freescale Semiconductor, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/sys_proto.h>

/* Maximum fixed count */
#define TIMER_LOAD_VAL	0xffffffff

DECLARE_GLOBAL_DATA_PTR;

/* Enable this to verify that the code can correctly
 * handle the timer rollover
 */
/* #define DEBUG_TIMER_WRAP */

#ifdef DEBUG_TIMER_WRAP
/*
 * Let the timer wrap 15 seconds after start to catch misbehaving
 * timer related code early
 */
#define TIMER_START		(-time_to_tick(15 * CONFIG_SYS_HZ))
#else
#define TIMER_START		0UL
#endif

/*
 * This driver uses 1kHz clock source.
 */
#define	MXS_INCREMENTER_HZ		1000

static inline unsigned long tick_to_time(unsigned long tick)
{
	return tick / (MXS_INCREMENTER_HZ / CONFIG_SYS_HZ);
}

static inline unsigned long time_to_tick(unsigned long time)
{
	return time * (MXS_INCREMENTER_HZ / CONFIG_SYS_HZ);
}

int timer_init(void)
{
	struct mxs_timrot_regs *timrot_regs =
		(struct mxs_timrot_regs *)MXS_TIMROT_BASE;

	/* Reset Timers and Rotary Encoder module */
	mxs_reset_block(&timrot_regs->hw_timrot_rotctrl_reg);

	/* Set fixed_count to 0 */
	writel(0, &timrot_regs->hw_timrot_fixed_count0);

	/* Set UPDATE bit and 1Khz frequency */
	writel(TIMROT_TIMCTRLn_UPDATE | TIMROT_TIMCTRLn_RELOAD |
		TIMROT_TIMCTRLn_SELECT_1KHZ_XTAL,
		&timrot_regs->hw_timrot_timctrl0);

#ifndef DEBUG_TIMER_WRAP
	/* Set fixed_count to maximum value */
	writel(TIMER_LOAD_VAL, &timrot_regs->hw_timrot_fixed_count0);
#else
	/* Set fixed_count so that the counter will wrap after 20 seconds */
	writel(20 * MXS_INCREMENTER_HZ,
		&timrot_regs->hw_timrot_fixed_count0);
	gd->arch.lastinc = TIMER_LOAD_VAL - 20 * MXS_INCREMENTER_HZ;
#endif
#ifdef DEBUG_TIMER_WRAP
	/* Make the usec counter roll over 30 seconds after startup */
	writel(-30000000, MXS_HW_DIGCTL_MICROSECONDS);
#endif
	writel(TIMROT_TIMCTRLn_UPDATE,
		&timrot_regs->hw_timrot_timctrl0_clr);
#ifdef DEBUG_TIMER_WRAP
	/* Set fixed_count to maximal value for subsequent loads */
	writel(TIMER_LOAD_VAL, &timrot_regs->hw_timrot_fixed_count0);
#endif
	gd->arch.timer_rate_hz = MXS_INCREMENTER_HZ;
	gd->arch.tbl = TIMER_START;
	gd->arch.tbu = 0;
	return 0;
}

/* Note: This function works correctly for TIMER_LOAD_VAL == 0xffffffff!
 * The rollover is handled automagically due to the properties of
 * two's complement arithmetic.
 * For any other value of TIMER_LOAD_VAL the calculations would have
 * to be done modulus(TIMER_LOAD_VAL + 1).
 */
unsigned long long get_ticks(void)
{
	struct mxs_timrot_regs *timrot_regs =
		(struct mxs_timrot_regs *)MXS_TIMROT_BASE;
	/* The timer is counting down, so subtract the register value from
	 * the counter period length to get an incrementing timestamp
	 */
	unsigned long now = -readl(&timrot_regs->hw_timrot_running_count0);
	ulong inc = now - gd->arch.lastinc;

	gd->arch.tbl += inc;
	gd->arch.lastinc = now;
	/* Since the get_timer() function only uses a 32bit value
	 * it doesn't make sense to return a real 64 bit value here.
	 */
	return gd->arch.tbl;
}

ulong get_timer_masked(void)
{
	return tick_to_time(get_ticks());
}

ulong get_timer(ulong base)
{
	/* NOTE: time_to_tick(base) is required to correctly handle rollover! */
	return tick_to_time(get_ticks() - time_to_tick(base));
}

/* We use the HW_DIGCTL_MICROSECONDS register for sub-millisecond timer. */
#define	MXS_HW_DIGCTL_MICROSECONDS	0x8001c0c0

void __udelay(unsigned long usec)
{
	uint32_t start = readl(MXS_HW_DIGCTL_MICROSECONDS);

	while (readl(MXS_HW_DIGCTL_MICROSECONDS) - start <= usec)
		/* use '<=' to guarantee a delay of _at least_
		 * the given number of microseconds.
		 * No need for fancy rollover checks
		 * Two's complement arithmetic applied correctly
		 * does everything that's needed  automagically!
		 */
		;
}

ulong get_tbclk(void)
{
	return gd->arch.timer_rate_hz;
}
