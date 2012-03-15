/*
 * Freescale i.MX28 Boot PMIC init
 *
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

#include <common.h>
#include <config.h>
#include <asm/io.h>
#include <asm/arch/imx-regs.h>

#include "mx28_init.h"

static struct mx28_power_regs *power_regs = (void *)MXS_POWER_BASE;

//#define DEBUG
#include "debug.h"

#undef __arch_getl
#undef __arch_putl

#define __arch_getl(a)	arch_getl(a, __func__, __LINE__)
static inline unsigned int arch_getl(volatile void *addr,
			const char *fn, unsigned int ln)
{
	volatile unsigned int *a = addr;
	unsigned int val = *a;
	dprintf("%s@%d: Read %x from %p\n", fn, ln, val, addr);
	return val;
}

#define __arch_putl(v, a)	arch_putl(v, a, __func__, __LINE__)
static inline void arch_putl(unsigned int val, volatile void *addr,
			const char *fn, unsigned int ln)
{
	volatile unsigned int *a = addr;

	dprintf("%s@%d: Writing %x to %p\n", fn, ln, val, addr);
	*a = val;
}

#define __static static

__static int mx28_power_vdd5v_gt_vddio(void)
{
	int power_sts = readl(&power_regs->hw_power_sts);

	dprintf("%s@%d: %d\n", __func__, __LINE__,
		!!(power_sts & POWER_STS_VDD5V_GT_VDDIO));
	return power_sts & POWER_STS_VDD5V_GT_VDDIO;
}

static void memdump(unsigned long addr, unsigned long len)
{
#ifdef DEBUG
	int i;
	uint32_t *ptr = (void *)addr;

	for (i = 0; i < len; i++) {
		if (i % 4 == 0)
			dprintf("\n%x:", &ptr[i]);
		dprintf(" %x", ptr[i]);
	}
	dprintf("\n");
#endif
}

__static void mx28_power_clock2xtal(void)
{
	struct mx28_clkctrl_regs *clkctrl_regs =
		(struct mx28_clkctrl_regs *)MXS_CLKCTRL_BASE;

	dprintf("%s@%d: \n", __func__, __LINE__);
	/* Set XTAL as CPU reference clock */
	writel(CLKCTRL_CLKSEQ_BYPASS_CPU,
		&clkctrl_regs->hw_clkctrl_clkseq_set);
}

__static void mx28_power_clock2pll(void)
{
	struct mx28_clkctrl_regs *clkctrl_regs =
		(struct mx28_clkctrl_regs *)MXS_CLKCTRL_BASE;

	dprintf("%s@%d: \n", __func__, __LINE__);
	writel(CLKCTRL_PLL0CTRL0_POWER,
		&clkctrl_regs->hw_clkctrl_pll0ctrl0_set);
	early_delay(100);
	writel(CLKCTRL_CLKSEQ_BYPASS_CPU,
		&clkctrl_regs->hw_clkctrl_clkseq_clr);
}

__static void mx28_power_clear_auto_restart(void)
{
	struct mx28_rtc_regs *rtc_regs =
		(struct mx28_rtc_regs *)MXS_RTC_BASE;

	dprintf("%s@%d: \n", __func__, __LINE__);
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

__static void mx28_power_set_linreg(void)
{
	dprintf("%s@%d: \n", __func__, __LINE__);
	/* Set linear regulator 25mV below switching converter */
	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_LINREG_OFFSET_MASK,
			POWER_VDDDCTRL_LINREG_OFFSET_1STEPS_ABOVE);

	clrsetbits_le32(&power_regs->hw_power_vddactrl,
			POWER_VDDACTRL_LINREG_OFFSET_MASK,
			POWER_VDDACTRL_LINREG_OFFSET_1STEPS_BELOW);

	clrsetbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDIOCTRL_LINREG_OFFSET_MASK,
			POWER_VDDIOCTRL_LINREG_OFFSET_1STEPS_BELOW);
	dprintf("%s@%d: vddioctrl=%x\n", __func__, __LINE__,
		readl(&power_regs->hw_power_vddioctrl));
}

__static void mx28_src_power_init(void)
{
	dprintf("%s@%d: \n", __func__, __LINE__);
	/* Improve efficieny and reduce transient ripple */
	writel(POWER_LOOPCTRL_TOGGLE_DIF | POWER_LOOPCTRL_EN_CM_HYST |
		POWER_LOOPCTRL_EN_DF_HYST, &power_regs->hw_power_loopctrl_set);

	clrsetbits_le32(&power_regs->hw_power_dclimits,
			POWER_DCLIMITS_POSLIMIT_BUCK_MASK,
			0x30 << POWER_DCLIMITS_POSLIMIT_BUCK_OFFSET);

	setbits_le32(&power_regs->hw_power_battmonitor,
			POWER_BATTMONITOR_EN_BATADJ);

	/* Increase the RCSCALE level for quick DCDC response to dynamic load */
	clrsetbits_le32(&power_regs->hw_power_loopctrl,
			POWER_LOOPCTRL_EN_RCSCALE_MASK,
			POWER_LOOPCTRL_RCSCALE_THRESH |
			POWER_LOOPCTRL_EN_RCSCALE_8X);

	clrsetbits_le32(&power_regs->hw_power_minpwr,
			POWER_MINPWR_HALFFETS, POWER_MINPWR_DOUBLE_FETS);
#ifndef CONFIG_SPL_FIXED_BATT_SUPPLY
	/* 5V to battery handoff ... FIXME */
	setbits_le32(&power_regs->hw_power_5vctrl, POWER_5VCTRL_DCDC_XFER);
	early_delay(30);
	clrbits_le32(&power_regs->hw_power_5vctrl, POWER_5VCTRL_DCDC_XFER);
#endif
}

void mx28_powerdown(void)
{
	dprintf("%s@%d: \n", __func__, __LINE__);
	writel(POWER_RESET_UNLOCK_KEY, &power_regs->hw_power_reset);
	writel(POWER_RESET_UNLOCK_KEY | POWER_RESET_PWD_OFF,
		&power_regs->hw_power_reset);
}

__static void mx28_fixed_batt_boot(void)
{
	writel(POWER_5VCTRL_PWDN_5VBRNOUT,
		&power_regs->hw_power_5vctrl_set);
	writel(POWER_5VCTRL_ENABLE_DCDC,
		&power_regs->hw_power_5vctrl_set);

	writel(POWER_CHARGE_PWD_BATTCHRG, &power_regs->hw_power_charge_set);

	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_DISABLE_FET |
			POWER_VDDDCTRL_ENABLE_LINREG,
			POWER_VDDDCTRL_DISABLE_STEPPING);

	/* Stop 5V detection */
	writel(POWER_5VCTRL_PWRUP_VBUS_CMPS,
		&power_regs->hw_power_5vctrl_clr);
}

#ifndef CONFIG_SPL_FIXED_BATT_SUPPLY
#define BATT_BO_VALUE	15
#else
/* Brownout at 3.08V */
#define BATT_BO_VALUE	17
#endif

__static void mx28_init_batt_bo(void)
{
	dprintf("%s@%d: \n", __func__, __LINE__);
#ifndef CONFIG_SPL_FIXED_BATT_SUPPLY
	/* Brownout at 3V */
	clrsetbits_le32(&power_regs->hw_power_battmonitor,
		POWER_BATTMONITOR_BRWNOUT_LVL_MASK,
		15 << POWER_BATTMONITOR_BRWNOUT_LVL_OFFSET);

	writel(POWER_CTRL_BATT_BO_IRQ, &power_regs->hw_power_ctrl_clr);
	writel(POWER_CTRL_ENIRQ_BATT_BO, &power_regs->hw_power_ctrl_clr);
#else
	setbits_le32(&power_regs->hw_power_battmonitor,
		POWER_BATTMONITOR_PWDN_BATTBRNOUT);
	writel(POWER_CTRL_BATT_BO_IRQ, &power_regs->hw_power_ctrl_clr);
#endif
}

__static void mx28_switch_vddd_to_dcdc_source(void)
{
	dprintf("%s@%d: \n", __func__, __LINE__);
	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_LINREG_OFFSET_MASK,
			POWER_VDDDCTRL_LINREG_OFFSET_1STEPS_BELOW);

	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_DISABLE_FET | POWER_VDDDCTRL_DISABLE_STEPPING,
			POWER_VDDDCTRL_ENABLE_LINREG);
}

__static void mx28_enable_output_rail_protection(void)
{
	writel(POWER_CTRL_VDDD_BO_IRQ | POWER_CTRL_VDDA_BO_IRQ |
		POWER_CTRL_VDDIO_BO_IRQ, &power_regs->hw_power_ctrl_clr);
#if 0
	setbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_PWDN_BRNOUT);

	setbits_le32(&power_regs->hw_power_vddactrl,
			POWER_VDDACTRL_PWDN_BRNOUT);

	setbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDIOCTRL_PWDN_BRNOUT);
#else
	clrbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_PWDN_BRNOUT);

	clrbits_le32(&power_regs->hw_power_vddactrl,
			POWER_VDDACTRL_PWDN_BRNOUT);

	clrbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDIOCTRL_PWDN_BRNOUT);
#endif
}

__static int mx28_get_vddio_power_source_off(void)
{
	uint32_t tmp;

	dprintf("%s@%d: \n", __func__, __LINE__);
	if (mx28_power_vdd5v_gt_vddio()) {
		tmp = readl(&power_regs->hw_power_vddioctrl);
		if (tmp & POWER_VDDIOCTRL_DISABLE_FET) {
			if ((tmp & POWER_VDDIOCTRL_LINREG_OFFSET_MASK) ==
				POWER_VDDDCTRL_LINREG_OFFSET_0STEPS) {
				dprintf("%s@%d: 1\n", __func__, __LINE__);
				return 1;
			}
		}

		if (!(readl(&power_regs->hw_power_5vctrl) &
			POWER_5VCTRL_ENABLE_DCDC)) {
			dprintf("tmp=%x mask %x = %x == %x?\n",
				tmp, POWER_VDDIOCTRL_LINREG_OFFSET_MASK,
				tmp & POWER_VDDIOCTRL_LINREG_OFFSET_MASK,
				POWER_VDDDCTRL_LINREG_OFFSET_0STEPS);
			if ((tmp & POWER_VDDIOCTRL_LINREG_OFFSET_MASK) ==
				POWER_VDDDCTRL_LINREG_OFFSET_0STEPS) {
				dprintf("%s@%d: 1\n", __func__, __LINE__);
				return 1;
			}
			if ((tmp & POWER_VDDIOCTRL_LINREG_OFFSET_MASK) ==
				POWER_VDDIOCTRL_LINREG_OFFSET_1STEPS_BELOW) {
				dprintf("%s@%d: 1\n", __func__, __LINE__);
				return 1;
			}
		}
	}
	dprintf("%s@%d: 0\n", __func__, __LINE__);
	return 0;
}

__static int mx28_get_vddd_power_source_off(void)
{
	uint32_t tmp;

	dprintf("%s@%d: \n", __func__, __LINE__);
	tmp = readl(&power_regs->hw_power_vdddctrl);
	if (tmp & POWER_VDDDCTRL_DISABLE_FET) {
		if ((tmp & POWER_VDDDCTRL_LINREG_OFFSET_MASK) ==
			POWER_VDDDCTRL_LINREG_OFFSET_0STEPS) {
			dprintf("%s@%d: 1\n", __func__, __LINE__);
			return 1;
		}
	}

	if (readl(&power_regs->hw_power_sts) & POWER_STS_VDD5V_GT_VDDIO) {
		if (!(readl(&power_regs->hw_power_5vctrl) &
			POWER_5VCTRL_ENABLE_DCDC)) {
			dprintf("%s@%d: 1\n", __func__, __LINE__);
			return 1;
		}
	}

	if ((tmp & POWER_VDDDCTRL_ENABLE_LINREG)) {
		if ((tmp & POWER_VDDDCTRL_LINREG_OFFSET_MASK) ==
			POWER_VDDDCTRL_LINREG_OFFSET_1STEPS_BELOW) {
			dprintf("%s@%d: 1\n", __func__, __LINE__);
			return 1;
		}
	}
	dprintf("%s@%d: 0\n", __func__, __LINE__);
	return 0;
}

__static void mx28_power_set_vddio(uint32_t new_target, uint32_t new_brownout)
{
	uint32_t cur_target, diff, bo_int = 0;
	uint32_t powered_by_linreg;

	dprintf("%s@%d: \n", __func__, __LINE__);
	new_brownout = (new_target - new_brownout) / 25;
	if (new_brownout > 7) {
		dprintf("Bad VDDD brownout offset\n");
		new_brownout = 7;
	}

	cur_target = readl(&power_regs->hw_power_vddioctrl);
	cur_target &= POWER_VDDIOCTRL_TRG_MASK;
	cur_target *= 50;	/* 50 mV step*/
	cur_target += 2800;	/* 2800 mV lowest */

	powered_by_linreg = mx28_get_vddio_power_source_off();
	if (new_target != cur_target)
		dprintf("%s@%d: stepping VDDIO from %u to %u\n", __func__, __LINE__,
			cur_target, new_target);
	else
		dprintf("%s@%d: VDDIO is at %u\n", __func__, __LINE__,
			cur_target, new_target);
	if (new_target > cur_target) {

		if (powered_by_linreg) {
			bo_int = readl(&power_regs->hw_power_vddioctrl);
			clrbits_le32(&power_regs->hw_power_vddioctrl,
					POWER_CTRL_ENIRQ_VDDIO_BO);
		}

		setbits_le32(&power_regs->hw_power_vddioctrl,
				POWER_VDDIOCTRL_BO_OFFSET_MASK);
		do {
			if (new_target - cur_target > 100)
				diff = cur_target + 100;
			else
				diff = new_target;

			diff -= 2800;
			diff /= 50;

			clrsetbits_le32(&power_regs->hw_power_vddioctrl,
				POWER_VDDIOCTRL_TRG_MASK, diff);
			dprintf("%s@%d: vddioctrl=%x\n", __func__, __LINE__,
				readl(&power_regs->hw_power_vddioctrl));

			if (powered_by_linreg)
				early_delay(1500);
			else {
				while (!(readl(&power_regs->hw_power_sts) &
						POWER_STS_DC_OK)) {
					early_delay(1);
				}
			}
			dprintf("%s@%d: sts=%x\n", __func__, __LINE__,
				readl(&power_regs->hw_power_sts));

			cur_target = readl(&power_regs->hw_power_vddioctrl);
			cur_target &= POWER_VDDIOCTRL_TRG_MASK;
			cur_target *= 50;	/* 50 mV step*/
			cur_target += 2800;	/* 2800 mV lowest */
		} while (new_target > cur_target);

		if (powered_by_linreg) {
			writel(POWER_CTRL_VDDIO_BO_IRQ,
				&power_regs->hw_power_ctrl_clr);
			if (bo_int & POWER_CTRL_ENIRQ_VDDIO_BO)
				setbits_le32(&power_regs->hw_power_vddioctrl,
						POWER_CTRL_ENIRQ_VDDIO_BO);
			dprintf("%s@%d: vddioctrl=%x\n", __func__, __LINE__,
				readl(&power_regs->hw_power_vddioctrl));
		}
		dprintf("%s@%d: Done\n", __func__, __LINE__);
	} else {
		do {
			if (cur_target - new_target > 100)
				diff = cur_target - 100;
			else
				diff = new_target;

			diff -= 2800;
			diff /= 50;

			clrsetbits_le32(&power_regs->hw_power_vddioctrl,
				POWER_VDDIOCTRL_TRG_MASK, diff);

			if (powered_by_linreg)
				early_delay(1500);
			else {
				while (!(readl(&power_regs->hw_power_sts) &
						POWER_STS_DC_OK)) {
					early_delay(1);
				}
			}
			dprintf("%s@%d: sts=%x\n", __func__, __LINE__,
				readl(&power_regs->hw_power_sts));

			cur_target = readl(&power_regs->hw_power_vddioctrl);
			cur_target &= POWER_VDDIOCTRL_TRG_MASK;
			cur_target *= 50;	/* 50 mV step*/
			cur_target += 2800;	/* 2800 mV lowest */
		} while (new_target < cur_target);
		dprintf("%s@%d: Done\n", __func__, __LINE__);
	}

	clrsetbits_le32(&power_regs->hw_power_vddioctrl,
			POWER_VDDDCTRL_BO_OFFSET_MASK,
			new_brownout << POWER_VDDDCTRL_BO_OFFSET_OFFSET);
	dprintf("%s@%d: vddioctrl=%x\n", __func__, __LINE__,
		readl(&power_regs->hw_power_vddioctrl));
}

__static void mx28_power_set_vddd(uint32_t new_target, uint32_t new_brownout)
{
	uint32_t cur_target, diff, bo_int = 0;
	uint32_t powered_by_linreg;

	dprintf("%s@%d: \n", __func__, __LINE__);
	new_brownout = (new_target - new_brownout) / 25;
	if (new_brownout > 7) {
		dprintf("Bad VDDD brownout offset\n");
		new_brownout = 7;
	}
	cur_target = readl(&power_regs->hw_power_vdddctrl);
	cur_target &= POWER_VDDDCTRL_TRG_MASK;
	cur_target *= 25;	/* 25 mV step*/
	cur_target += 800;	/* 800 mV lowest */

	powered_by_linreg = mx28_get_vddd_power_source_off();
	if (new_target != cur_target)
		dprintf("%s@%d: stepping VDDD from %u to %u\n", __func__, __LINE__,
			cur_target, new_target);
	else
		dprintf("%s@%d: VDDD is at %u\n", __func__, __LINE__,
			cur_target, new_target);
	if (new_target > cur_target) {
		if (powered_by_linreg) {
			bo_int = readl(&power_regs->hw_power_vdddctrl);
			clrbits_le32(&power_regs->hw_power_vdddctrl,
					POWER_CTRL_ENIRQ_VDDD_BO);
		}

		setbits_le32(&power_regs->hw_power_vdddctrl,
				POWER_VDDDCTRL_BO_OFFSET_MASK);

		do {
			if (new_target - cur_target > 100)
				diff = cur_target + 100;
			else
				diff = new_target;

			diff -= 800;
			diff /= 25;

			clrsetbits_le32(&power_regs->hw_power_vdddctrl,
				POWER_VDDDCTRL_TRG_MASK, diff);

			if (powered_by_linreg)
				early_delay(1500);
			else {
				while (!(readl(&power_regs->hw_power_sts) &
						POWER_STS_DC_OK)) {
					early_delay(1);
				}
			}
			dprintf("%s@%d: sts=%x\n", __func__, __LINE__,
				readl(&power_regs->hw_power_sts));

			cur_target = readl(&power_regs->hw_power_vdddctrl);
			cur_target &= POWER_VDDDCTRL_TRG_MASK;
			cur_target *= 25;	/* 25 mV step*/
			cur_target += 800;	/* 800 mV lowest */
		} while (new_target > cur_target);

		if (powered_by_linreg) {
			writel(POWER_CTRL_VDDD_BO_IRQ,
				&power_regs->hw_power_ctrl_clr);
			if (bo_int & POWER_CTRL_ENIRQ_VDDD_BO)
				setbits_le32(&power_regs->hw_power_vdddctrl,
						POWER_CTRL_ENIRQ_VDDD_BO);
		}
		dprintf("%s@%d: Done\n", __func__, __LINE__);
	} else {
		do {
			if (cur_target - new_target > 100)
				diff = cur_target - 100;
			else
				diff = new_target;

			diff -= 800;
			diff /= 25;

			clrsetbits_le32(&power_regs->hw_power_vdddctrl,
					POWER_VDDDCTRL_TRG_MASK, diff);

			if (powered_by_linreg)
				early_delay(1500);
			else {
				while (!(readl(&power_regs->hw_power_sts) &
						POWER_STS_DC_OK)) {
					early_delay(1);
				}
			}
			dprintf("%s@%d: sts=%x\n", __func__, __LINE__,
				readl(&power_regs->hw_power_sts));

			cur_target = readl(&power_regs->hw_power_vdddctrl);
			cur_target &= POWER_VDDDCTRL_TRG_MASK;
			cur_target *= 25;	/* 25 mV step*/
			cur_target += 800;	/* 800 mV lowest */
		} while (new_target < cur_target);
		dprintf("%s@%d: Done\n", __func__, __LINE__);
	}

	clrsetbits_le32(&power_regs->hw_power_vdddctrl,
			POWER_VDDDCTRL_BO_OFFSET_MASK,
			new_brownout << POWER_VDDDCTRL_BO_OFFSET_OFFSET);
}

void mx28_power_init(void)
{
	dprintf("%s: %s %s\n", __func__, __DATE__, __TIME__);
	dprintf("ctrl=%x\n", readl(&power_regs->hw_power_ctrl));
	dprintf("sts=%x\n", readl(&power_regs->hw_power_sts));

	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_power_clock2xtal();
	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_power_clear_auto_restart();
	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_power_set_linreg();
	dprintf("%s@%d\n", __func__, __LINE__);

	mx28_src_power_init();
	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_fixed_batt_boot();
	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_power_clock2pll();
	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_init_batt_bo();
	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_switch_vddd_to_dcdc_source();

	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_enable_output_rail_protection();

	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_power_set_vddio(3300, 3150);

	dprintf("%s@%d\n", __func__, __LINE__);
	mx28_power_set_vddd(1500, 1325);
	dprintf("%s@%d\n", __func__, __LINE__);

	writel(POWER_CTRL_VDDD_BO_IRQ | POWER_CTRL_VDDA_BO_IRQ |
		POWER_CTRL_VDDIO_BO_IRQ | POWER_CTRL_VDD5V_DROOP_IRQ |
		POWER_CTRL_VBUS_VALID_IRQ | POWER_CTRL_BATT_BO_IRQ |
		POWER_CTRL_DCDC4P2_BO_IRQ, &power_regs->hw_power_ctrl_clr);

	dprintf("sts=%x\n", readl(&power_regs->hw_power_sts));
	dprintf("vddioctrl=%x\n", readl(&power_regs->hw_power_vddioctrl));
	dprintf("vdddctrl=%x\n", readl(&power_regs->hw_power_vdddctrl));
	dprintf("5vctrl=%x\n", readl(&power_regs->hw_power_5vctrl));
	dprintf("dcdc4p2=%x\n", readl(&power_regs->hw_power_dcdc4p2));
	dprintf("%s@%d: Finished\n", __func__, __LINE__);
	memdump(0x80044000, 0x60);
	early_delay(1000);
}

#ifdef	CONFIG_SPL_MX28_PSWITCH_WAIT
void mx28_power_wait_pswitch(void)
{
	dprintf("%s@%d: \n", __func__, __LINE__);
	while (!(readl(&power_regs->hw_power_sts) & POWER_STS_PSWITCH_MASK))
		;
}
#endif
