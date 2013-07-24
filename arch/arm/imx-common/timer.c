/*
 * (C) Copyright 2007
 * Sascha Hauer, Pengutronix
 *
 * (C) Copyright 2009 Freescale Semiconductor, Inc.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <asm/io.h>
#include <div64.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/clock.h>

#define DEBUG_TIMER_WRAP

/* General purpose timers registers */
struct mxc_gpt {
	unsigned int control;
	unsigned int prescaler;
	unsigned int status;
	unsigned int nouse[6];
	unsigned int counter;
};

static struct mxc_gpt *cur_gpt = (struct mxc_gpt *)GPT1_BASE_ADDR;

/* General purpose timers bitfields */
#define GPTCR_SWR		(1 << 15)	/* Software reset */
#define GPTCR_FRR		(1 << 9)	/* Freerun / restart */
#define GPTCR_CLKSOURCE_IPG	(1 << 6)	/* Clock source */
#define GPTCR_CLKSOURCE_CKIH	(2 << 6)
#define GPTCR_CLKSOURCE_32kHz	(4 << 6)
#if defined(CONFIG_MX6Q)
#define GPTCR_CLKSOURCE_OSCDIV8	(5 << 6)
#define GPTCR_CLKSOURCE_OSC	(7 << 6)
#elif defined(CONFIG_MX6DL)
#define GPTCR_M24EN		(1 << 10)
#define GPTCR_CLKSOURCE_OSC	((5 << 6) | GPTCR_M24EN)
#else
#define GPTCR_CLKSOURCE_OSC	(5 << 6)
#endif
#define GPTCR_CLKSOURCE_MASK	(7 << 6)
#define GPTCR_TEN		1		/* Timer enable */

#if 1
#define GPT_CLKSOURCE		GPTCR_CLKSOURCE_OSC
#define GPT_REFCLK		24000000
#define GPT_PRESCALER		24
#else
#define GPT_CLKSOURCE		GPTCR_CLKSOURCE_32kHz
#define GPT_REFCLK		32768
#define GPT_PRESCALER		1
#endif
#define GPT_CLK			(GPT_REFCLK / GPT_PRESCALER)

#ifdef DEBUG_TIMER_WRAP
/*
 * Let the timer wrap 30 seconds after start to catch misbehaving
 * timer related code early
 */
#define TIMER_START		(-time_to_tick(30 * CONFIG_SYS_HZ))
#else
#define TIMER_START		0UL
#endif

DECLARE_GLOBAL_DATA_PTR;

static inline unsigned long tick_to_time(unsigned long tick)
{
	unsigned long long t = (unsigned long long)tick * CONFIG_SYS_HZ;
	do_div(t, GPT_CLK);
	return t;
}

static inline unsigned long time_to_tick(unsigned long time)
{
	unsigned long long ticks = (unsigned long long)time;

	ticks *= GPT_CLK;
	do_div(ticks, CONFIG_SYS_HZ);
	return ticks;
}

static inline unsigned long us_to_tick(unsigned long usec)
{
	unsigned long long ticks = (unsigned long long)usec;

	ticks *= GPT_CLK;
	do_div(ticks, 1000 * CONFIG_SYS_HZ);
	return ticks;
}

int timer_init(void)
{
	int i;

	/* setup GP Timer 1 */
	__raw_writel(GPTCR_SWR, &cur_gpt->control);

	/* We have no udelay by now */
	for (i = 0; i < 100; i++)
		__raw_writel(0, &cur_gpt->control);

	__raw_writel(GPT_PRESCALER - 1, &cur_gpt->prescaler);

	/* Freerun Mode, PERCLK1 input */
	i = __raw_readl(&cur_gpt->control);
	i &= ~GPTCR_CLKSOURCE_MASK;
	__raw_writel(i | GPT_CLKSOURCE | GPTCR_TEN, &cur_gpt->control);

	gd->arch.lastinc = __raw_readl(&cur_gpt->counter);
	gd->arch.tbu = 0;
	gd->arch.tbl = TIMER_START;
	gd->arch.timer_rate_hz = GPT_CLK;

	return 0;
}

unsigned long long get_ticks(void)
{
	ulong now = __raw_readl(&cur_gpt->counter); /* current tick value */
	ulong inc = now - gd->arch.lastinc;

	gd->arch.tbl += inc;
	gd->arch.lastinc = now;
	return gd->arch.tbl;
}

ulong get_timer_masked(void)
{
	/*
	 * get_ticks() returns a long long (64 bit), it wraps in
	 * 2^64 / MXC_CLK32 = 2^64 / 2^15 = 2^49 ~ 5 * 10^14 (s) ~
	 * 5 * 10^9 days... and get_ticks() * CONFIG_SYS_HZ wraps in
	 * 5 * 10^6 days - long enough.
	 */
	/*
	 * LW: get_ticks() returns a long long with the top 32 bits always ZERO!
	 * Thus the calculation above is not true.
	 * A 64bit timer value would only make sense if it was
	 * consistently used throughout the code. Thus also the parameter
	 * to get_timer() and its return value would need to be 64bit wide!
	 */
	return tick_to_time(get_ticks());
}

ulong get_timer(ulong base)
{
	return tick_to_time(get_ticks() - time_to_tick(base));
}

#include <asm/gpio.h>

/* delay x useconds AND preserve advance timstamp value */
void __udelay(unsigned long usec)
{
	unsigned long start = __raw_readl(&cur_gpt->counter);
	unsigned long ticks;

	if (usec == 0)
		return;

	ticks = us_to_tick(usec);
	if (ticks == 0)
		ticks++;

	while (__raw_readl(&cur_gpt->counter) - start < ticks)
		/* loop till event */;
}

/*
 * This function is derived from PowerPC code (timebase clock frequency).
 * On ARM it returns the number of timer ticks per second.
 */
ulong get_tbclk(void)
{
	return gd->arch.timer_rate_hz;
}
