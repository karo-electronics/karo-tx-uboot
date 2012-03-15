/*
 * Freescale i.MX28 Boot PMIC init
 *
 * Copyright (C) 2012 Lothar Wassmann <LW@karo-electronics.de>
 * based on: spl_power_init.c
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
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
/* #define DEBUG */

#include <common.h>
#include <config.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>

#include "mx28_init.h"

static struct mx28_power_regs *power_regs = (void *)MXS_POWER_BASE;

#include "debug.h"

#ifdef DEBUG
#undef __arch_getl
#undef __arch_putl

#define __arch_getl(a)	arch_getl(a, __func__, __LINE__)
static inline unsigned int arch_getl(volatile void *addr,
			const char *fn, unsigned int ln)
{
	volatile unsigned int *a = addr;
	unsigned int val = *a;
	dbg(0, "%s@%d: Read %x from %p\n", fn, ln, val, addr);
	return val;
}

#define __arch_putl(v, a)	arch_putl(v, a, __func__, __LINE__)
static inline void arch_putl(unsigned int val, volatile void *addr,
			const char *fn, unsigned int ln)
{
	volatile unsigned int *a = addr;

	dbg(0, "%s@%d: Writing %x to %p\n", fn, ln, val, addr);
	*a = val;
}
#endif

static void memdump(unsigned long addr, unsigned long len)
{
#ifdef DEBUG
	int i;
	uint32_t *ptr = (void *)addr;

	for (i = 0; i < len; i++) {
		if (i % 4 == 0)
			dbg(0, "\n%x:", &ptr[i]);
		dbg(0, " %x", ptr[i]);
	}
	dbg(0, "\n");
#endif
}

static void mx28_power_clock2xtal(void)
{
	struct mx28_clkctrl_regs *clkctrl_regs =
		(struct mx28_clkctrl_regs *)MXS_CLKCTRL_BASE;

	/* Set XTAL as CPU reference clock */
	writel(CLKCTRL_CLKSEQ_BYPASS_CPU,
		&clkctrl_regs->hw_clkctrl_clkseq_set);
}

static void mx28_power_clock2pll(void)
{
	struct mx28_clkctrl_regs *clkctrl_regs =
		(struct mx28_clkctrl_regs *)MXS_CLKCTRL_BASE;

	readl(&clkctrl_regs->hw_clkctrl_pll0ctrl0);
	readl(&clkctrl_regs->hw_clkctrl_pll0ctrl1);
	writel(CLKCTRL_PLL0CTRL0_POWER,
		&clkctrl_regs->hw_clkctrl_pll0ctrl0_set);
	readl(&clkctrl_regs->hw_clkctrl_pll0ctrl0);
	readl(&clkctrl_regs->hw_clkctrl_frac0);
	readl(&clkctrl_regs->hw_clkctrl_frac1);
	readl(&clkctrl_regs->hw_clkctrl_clkseq);
	early_delay(100);
	writel(CLKCTRL_CLKSEQ_BYPASS_CPU,
		&clkctrl_regs->hw_clkctrl_clkseq_clr);
}

static void mx28_power_clear_auto_restart(void)
{
	struct mx28_rtc_regs *rtc_regs =
		(struct mx28_rtc_regs *)MXS_RTC_BASE;

	writel(RTC_CTRL_SFTRST, &rtc_regs->hw_rtc_ctrl_clr);
	while (readl(&rtc_regs->hw_rtc_ctrl) & RTC_CTRL_SFTRST)
		;

	writel(RTC_CTRL_CLKGATE, &rtc_regs->hw_rtc_ctrl_clr);
	while (readl(&rtc_regs->hw_rtc_ctrl) & RTC_CTRL_CLKGATE)
		;
#ifdef CONFIG_MACH_MX28EVK
	/*
	 * Due to the hardware design bug of mx28 EVK-A
	 * we need to set the AUTO_RESTART bit.
	 */
	if (readl(&rtc_regs->hw_rtc_persistent0) & RTC_PERSISTENT0_AUTO_RESTART)
		return;

	while (readl(&rtc_regs->hw_rtc_stat) & RTC_STAT_NEW_REGS_MASK)
		;

	setbits_le32(&rtc_regs->hw_rtc_persistent0,
			RTC_PERSISTENT0_AUTO_RESTART);
	writel(RTC_CTRL_FORCE_UPDATE, &rtc_regs->hw_rtc_ctrl_set);
	writel(RTC_CTRL_FORCE_UPDATE, &rtc_regs->hw_rtc_ctrl_clr);
	while (readl(&rtc_regs->hw_rtc_stat) & RTC_STAT_NEW_REGS_MASK)
		;
	while (readl(&rtc_regs->hw_rtc_stat) & RTC_STAT_STALE_REGS_MASK)
		;
#endif
}

static void mx28_power_set_linreg(void)
{
	/* Set linear regulator 25mV below switching converter */
	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_LINREG_OFFSET_MASK,
			POWER_VDDDCTRL_LINREG_OFFSET_1STEPS_BELOW);

	clrsetbits_le32(&power_regs->hw_power_vddactrl,
			POWER_VDDACTRL_LINREG_OFFSET_MASK,
			POWER_VDDACTRL_LINREG_OFFSET_1STEPS_BELOW);

	clrsetbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDIOCTRL_LINREG_OFFSET_MASK,
			POWER_VDDIOCTRL_LINREG_OFFSET_1STEPS_BELOW);
}

static void mx28_src_power_init(void)
{
	/* Improve efficieny and reduce transient ripple */
	writel(POWER_LOOPCTRL_TOGGLE_DIF | POWER_LOOPCTRL_EN_CM_HYST |
		POWER_LOOPCTRL_EN_DF_HYST, &power_regs->hw_power_loopctrl_set);

	clrsetbits_le32(&power_regs->hw_power_dclimits,
			POWER_DCLIMITS_POSLIMIT_BUCK_MASK,
			0x30 << POWER_DCLIMITS_POSLIMIT_BUCK_OFFSET);

	/* Increase the RCSCALE level for quick DCDC response to dynamic load */
	clrsetbits_le32(&power_regs->hw_power_loopctrl,
			POWER_LOOPCTRL_EN_RCSCALE_MASK,
			POWER_LOOPCTRL_RCSCALE_THRESH |
			POWER_LOOPCTRL_EN_RCSCALE_8X);

	clrsetbits_le32(&power_regs->hw_power_minpwr,
			POWER_MINPWR_HALFFETS, POWER_MINPWR_DOUBLE_FETS);
#if 0
	/* 5V to battery handoff ... FIXME */
	setbits_le32(&power_regs->hw_power_5vctrl, POWER_5VCTRL_DCDC_XFER);
	early_delay(30);
	clrbits_le32(&power_regs->hw_power_5vctrl, POWER_5VCTRL_DCDC_XFER);
#endif
}

void mx28_powerdown(void)
{
	writel(POWER_RESET_UNLOCK_KEY, &power_regs->hw_power_reset);
	writel(POWER_RESET_UNLOCK_KEY | POWER_RESET_PWD_OFF,
		&power_regs->hw_power_reset);
}

static void mx28_fixed_batt_boot(void)
{
	clrsetbits_le32(&power_regs->hw_power_battmonitor,
			POWER_BATTMONITOR_EN_BATADJ |
			POWER_BATTMONITOR_BATT_VAL_MASK,
			POWER_BATTMONITOR_PWDN_BATTBRNOUT |
			((5000 / 8) << POWER_BATTMONITOR_BATT_VAL_OFFSET));
	writel(POWER_CTRL_BATT_BO_IRQ, &power_regs->hw_power_ctrl_clr);

	setbits_le32(&power_regs->hw_power_5vctrl,
		POWER_5VCTRL_PWDN_5VBRNOUT |
		POWER_5VCTRL_ENABLE_DCDC |
		POWER_5VCTRL_ILIMIT_EQ_ZERO |
		POWER_5VCTRL_PWDN_5VBRNOUT |
		POWER_5VCTRL_PWD_CHARGE_4P2_MASK);

	writel(POWER_CHARGE_PWD_BATTCHRG, &power_regs->hw_power_charge_set);

	clrbits_le32(&power_regs->hw_power_vdddctrl,
		POWER_VDDDCTRL_DISABLE_FET |
		POWER_VDDDCTRL_ENABLE_LINREG |
		POWER_VDDDCTRL_DISABLE_STEPPING);

	/* Stop 5V detection */
	writel(POWER_5VCTRL_PWRUP_VBUS_CMPS,
		&power_regs->hw_power_5vctrl_clr);
}

static void mx28_switch_vdds_to_dcdc_source(void)
{
	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
		POWER_VDDDCTRL_DISABLE_FET |
		POWER_VDDDCTRL_DISABLE_STEPPING,
		POWER_VDDDCTRL_ENABLE_LINREG);

	clrsetbits_le32(&power_regs->hw_power_vddactrl,
		POWER_VDDACTRL_DISABLE_FET |
		POWER_VDDACTRL_DISABLE_STEPPING,
		POWER_VDDACTRL_ENABLE_LINREG);

	clrbits_le32(&power_regs->hw_power_vddioctrl,
		POWER_VDDIOCTRL_DISABLE_FET |
		POWER_VDDIOCTRL_DISABLE_STEPPING);

	clrbits_le32(&power_regs->hw_power_vddmemctrl,
		POWER_VDDMEMCTRL_ENABLE_ILIMIT |
		POWER_VDDMEMCTRL_ENABLE_LINREG);
}

static void mx28_enable_output_rail_protection(void)
{
	writel(POWER_CTRL_VDDD_BO_IRQ | POWER_CTRL_VDDA_BO_IRQ |
		POWER_CTRL_VDDIO_BO_IRQ, &power_regs->hw_power_ctrl_clr);

	setbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_PWDN_BRNOUT);

	setbits_le32(&power_regs->hw_power_vddactrl,
			POWER_VDDACTRL_PWDN_BRNOUT);

	setbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDIOCTRL_PWDN_BRNOUT);
}

static void mx28_power_set_vddio(uint32_t new_target, uint32_t new_brownout)
{
	uint32_t cur_target;

	dbg(3, "%s@%d: \n", __func__, __LINE__);
	new_brownout = (new_target - new_brownout) / 25;
	if (new_brownout > 7) {
		dbg(3, "Bad VDDD brownout offset\n");
		new_brownout = 7;
	}

	cur_target = readl(&power_regs->hw_power_vddioctrl);
	cur_target &= POWER_VDDIOCTRL_TRG_MASK;
	cur_target *= 50;	/* 50 mV step*/
	cur_target += 2800;	/* 2800 mV lowest */

	if (new_target == cur_target) {
		dbg(3, "%s@%d: VDDIO is at %u\n", __func__, __LINE__,
			cur_target, new_target);
		return;
	}

	dbg(3, "%s@%d: stepping VDDIO from %u to %u\n", __func__, __LINE__,
		cur_target, new_target);

	setbits_le32(&power_regs->hw_power_vddioctrl,
		POWER_VDDIOCTRL_BO_OFFSET_MASK);
	clrsetbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDIOCTRL_TRG_MASK, new_target);
	while (!(readl(&power_regs->hw_power_sts) &
			POWER_STS_DC_OK)) {
		static int loops = 100;
		early_delay(100);
		if (--loops < 0) {
			dprintf("Wait for VDDIO_OK timed out\n");
			break;
		}
	}
	dbg(3, "%s@%d: Done\n", __func__, __LINE__);

	clrsetbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDDCTRL_BO_OFFSET_MASK,
			new_brownout << POWER_VDDDCTRL_BO_OFFSET_OFFSET);
	dbg(3, "%s@%d: vddioctrl=%x\n", __func__, __LINE__,
		readl(&power_regs->hw_power_vddioctrl));
}

static void mx28_power_set_vddd(uint32_t new_target, uint32_t new_brownout)
{
	uint32_t cur_target;

	dbg(3, "%s@%d: \n", __func__, __LINE__);
	new_brownout = (new_target - new_brownout) / 25;
	if (new_brownout > 7) {
		dbg(3, "Bad VDDD brownout offset\n");
		new_brownout = 7;
	}
	cur_target = readl(&power_regs->hw_power_vdddctrl);
	cur_target &= POWER_VDDDCTRL_TRG_MASK;
	cur_target *= 25;	/* 25 mV step*/
	cur_target += 800;	/* 800 mV lowest */

	if (new_target == cur_target) {
		dbg(3, "%s@%d: VDDD is at %u\n", __func__, __LINE__,
			cur_target, new_target);
		return;
	}

	dbg(3, "%s@%d: stepping VDDD from %u to %u\n", __func__, __LINE__,
		cur_target, new_target);

	setbits_le32(&power_regs->hw_power_vdddctrl,
		POWER_VDDDCTRL_BO_OFFSET_MASK);
	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_TRG_MASK, new_target);
	while (!(readl(&power_regs->hw_power_sts) &
			POWER_STS_DC_OK)) {
		static int loops = 100;
		early_delay(100);
		if (--loops < 0) {
			dprintf("Wait for VDDD_OK timed out\n");
			break;
		}
	}
	dbg(3, "%s@%d: Done\n", __func__, __LINE__);

	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_BO_OFFSET_MASK,
			new_brownout << POWER_VDDDCTRL_BO_OFFSET_OFFSET);
}

void mx28_power_init(void)
{
	dprintf("%s: %s %s\n", __func__, __DATE__, __TIME__);

	mx28_power_clock2xtal();
	mx28_power_clear_auto_restart();
	mx28_power_set_linreg();

	mx28_src_power_init();
	early_delay(10000);
	mx28_fixed_batt_boot();
	early_delay(10000);
	mx28_power_clock2pll();
	early_delay(10000);
	early_delay(10000);
	mx28_switch_vdds_to_dcdc_source();
	early_delay(10000);

	mx28_enable_output_rail_protection();

	mx28_power_set_vddio(3300, 3150);

	mx28_power_set_vddd(1500, 1325);

	writel(POWER_CTRL_VDDD_BO_IRQ | POWER_CTRL_VDDA_BO_IRQ |
		POWER_CTRL_VDDIO_BO_IRQ | POWER_CTRL_VDD5V_DROOP_IRQ |
		POWER_CTRL_VBUS_VALID_IRQ | POWER_CTRL_BATT_BO_IRQ |
		POWER_CTRL_DCDC4P2_BO_IRQ, &power_regs->hw_power_ctrl_clr);

	dbg(0, "sts=%x\n", readl(&power_regs->hw_power_sts));
	dbg(0, "vddioctrl=%x\n", readl(&power_regs->hw_power_vddioctrl));
	dbg(0, "vdddctrl=%x\n", readl(&power_regs->hw_power_vdddctrl));
	dbg(0, "5vctrl=%x\n", readl(&power_regs->hw_power_5vctrl));
	dbg(0, "dcdc4p2=%x\n", readl(&power_regs->hw_power_dcdc4p2));
	dbg(0, "%s@%d: Finished\n", __func__, __LINE__);
	memdump(0x80044000, 0x60);
}

#ifdef	CONFIG_SPL_MX28_PSWITCH_WAIT
void mx28_power_wait_pswitch(void)
{
	dbg(3, "%s@%d: \n", __func__, __LINE__);
	while (!(readl(&power_regs->hw_power_sts) & POWER_STS_PSWITCH_MASK))
		;
}
#endif
