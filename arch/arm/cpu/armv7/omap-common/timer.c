/*
 * (C) Copyright 2008
 * Texas Instruments
 *
 * Richard Woodruff <r-woodruff2@ti.com>
 * Syed Moahmmed Khasim <khasim@ti.com>
 *
 * (C) Copyright 2002
 * Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Marius Groeger <mgroeger@sysgo.de>
 * Alex Zuepke <azu@sysgo.de>
 *
 * (C) Copyright 2002
 * Gary Jennejohn, DENX Software Engineering, <garyj@denx.de>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <div64.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>

DECLARE_GLOBAL_DATA_PTR;

static struct gptimer *timer_base = (struct gptimer *)CONFIG_SYS_TIMERBASE;

/*
 * Nothing really to do with interrupts, just starts up a counter.
 */

#if CONFIG_SYS_PTV > 7
#error Invalid CONFIG_SYS_PTV value
#elif CONFIG_SYS_PTV >= 0
#define TIMER_CLOCK		(V_SCLK / (2 << CONFIG_SYS_PTV))
#define TCLR_VAL		((CONFIG_SYS_PTV << 2) | TCLR_PRE | TCLR_AR | TCLR_ST)
#else
#define TIMER_CLOCK		V_SCLK
#define TCLR_VAL		(TCLR_AR | TCLR_ST)
#endif
#define TIMER_LOAD_VAL		0

#define TIOCP_CFG_SOFTRESET	(1 << 0)

#if TIMER_CLOCK < CONFIG_SYS_HZ
#error TIMER_CLOCK must be >= CONFIG_SYS_HZ
#endif

/*
 * Start timer so that it will overflow 15 sec after boot,
 * to catch misbehaving timer code early on!
*/
#define TIMER_START		(-time_to_tick(15 * CONFIG_SYS_HZ))

static inline unsigned long tick_to_time(unsigned long tick)
{
	return tick / (TIMER_CLOCK / CONFIG_SYS_HZ);
}

static inline unsigned long time_to_tick(unsigned long time)
{
	return time * (TIMER_CLOCK / CONFIG_SYS_HZ);
}

static inline unsigned long us_to_ticks(unsigned long usec)
{
	return usec * (TIMER_CLOCK / CONFIG_SYS_HZ / 1000);
}

int timer_init(void)
{
#if !defined(CONFIG_SPL) || defined(CONFIG_SPL_BUILD)
	/* Reset the Timer */
	writel(TIOCP_CFG_SOFTRESET, &timer_base->tiocp_cfg);

	/* Wait until the reset is done */
	while (readl(&timer_base->tiocp_cfg) & TIOCP_CFG_SOFTRESET)
		;

	/* preload the counter to make overflow occur early */
	writel(TIMER_START, &timer_base->tldr);
	writel(~0, &timer_base->ttgr);

	/* start the counter ticking up, reload value on overflow */
	writel(TIMER_LOAD_VAL, &timer_base->tldr);
	/* enable timer */
	writel(TCLR_VAL, &timer_base->tclr);
#endif
	gd->arch.lastinc = -30 * TIMER_CLOCK;
	gd->arch.tbl = TIMER_START;
	gd->arch.tbu = 0;
	gd->arch.timer_rate_hz = TIMER_CLOCK;

	return 0;
}

/*
 * timer without interrupts
 */
ulong get_timer(ulong base)
{
	return tick_to_time(get_ticks() - time_to_tick(base));
}

/* delay x useconds */
void __udelay(unsigned long usec)
{
	unsigned long start = readl(&timer_base->tcrr);
	unsigned long ticks = us_to_ticks(usec);

	if (usec == 0)
		return;

	if (ticks == 0)
		ticks++;

	while (readl(&timer_base->tcrr) - start < ticks)
		/* NOP */ ;
}

ulong get_timer_masked(void)
{
	/* current tick value */
	return tick_to_time(get_ticks());
}

/*
 * This function is derived from PowerPC code (read timebase as long long).
 * On ARM it just returns the timer value.
 */
unsigned long long get_ticks(void)
{
	ulong now = readl(&timer_base->tcrr);
	ulong inc = now - gd->arch.lastinc;

	if (gd->arch.tbl + inc < gd->arch.tbl)
		gd->arch.tbu++;
	gd->arch.tbl += inc;
	gd->arch.lastinc = now;
	return ((unsigned long long)gd->arch.tbu << 32) | gd->arch.tbl;
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
ulong get_tbclk(void)
{
	return gd->arch.timer_rate_hz;
}
