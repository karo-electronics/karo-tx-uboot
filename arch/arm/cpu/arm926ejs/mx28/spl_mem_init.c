/*
 * Freescale i.MX28 RAM init
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
#include <asm/arch/iomux-mx28.h>
#include <asm/arch/imx-regs.h>

#include "mx28_init.h"

extern void mx28_ddr2_setup(void) __attribute__((weak,
		alias("mx28_ddr2_setup_missing")));

static void mx28_ddr2_setup_missing(void)
{
	serial_puts("platform specific mx28_ddr_setup() is missing\n");
}

static void mx28_mem_init_clock(void)
{
	struct mx28_clkctrl_regs *clkctrl_regs =
		(struct mx28_clkctrl_regs *)MXS_CLKCTRL_BASE;

	/* Gate EMI clock */
	writel(CLKCTRL_FRAC0_CLKGATEEMI,
		&clkctrl_regs->hw_clkctrl_frac0_set);

	/* EMI = 205MHz */
	writel(CLKCTRL_FRAC0_EMIFRAC_MASK,
		&clkctrl_regs->hw_clkctrl_frac0_set);
	writel((~21 << CLKCTRL_FRAC0_EMIFRAC_OFFSET) &
		CLKCTRL_FRAC0_EMIFRAC_MASK,
		&clkctrl_regs->hw_clkctrl_frac0_clr);

	/* Ungate EMI clock */
	writel(CLKCTRL_FRAC0_CLKGATEEMI,
		&clkctrl_regs->hw_clkctrl_frac0_clr);

	early_delay(11000);

	writel((2 << CLKCTRL_EMI_DIV_EMI_OFFSET) |
		(1 << CLKCTRL_EMI_DIV_XTAL_OFFSET),
		&clkctrl_regs->hw_clkctrl_emi);

	/* Unbypass EMI */
	writel(CLKCTRL_CLKSEQ_BYPASS_EMI,
		&clkctrl_regs->hw_clkctrl_clkseq_clr);

	early_delay(10000);
}

static void mx28_mem_setup_cpu_and_hbus(void)
{
	struct mx28_clkctrl_regs *clkctrl_regs =
		(struct mx28_clkctrl_regs *)MXS_CLKCTRL_BASE;

	/* CPU = 454MHz and ungate CPU clock */
	clrsetbits_le32(&clkctrl_regs->hw_clkctrl_frac0,
		CLKCTRL_FRAC0_CPUFRAC_MASK | CLKCTRL_FRAC0_CLKGATECPU,
		19 << CLKCTRL_FRAC0_CPUFRAC_OFFSET);

	/* Set CPU bypass */
	writel(CLKCTRL_CLKSEQ_BYPASS_CPU,
		&clkctrl_regs->hw_clkctrl_clkseq_set);

	/* HBUS = 151MHz */
	writel(CLKCTRL_HBUS_DIV_MASK, &clkctrl_regs->hw_clkctrl_hbus_set);
	writel(((~3) << CLKCTRL_HBUS_DIV_OFFSET) & CLKCTRL_HBUS_DIV_MASK,
		&clkctrl_regs->hw_clkctrl_hbus_clr);

	early_delay(10000);

	/* CPU clock divider = 1 */
	clrsetbits_le32(&clkctrl_regs->hw_clkctrl_cpu,
			CLKCTRL_CPU_DIV_CPU_MASK, 1);

	/* Disable CPU bypass */
	writel(CLKCTRL_CLKSEQ_BYPASS_CPU,
		&clkctrl_regs->hw_clkctrl_clkseq_clr);
}

static void mx28_mem_setup_vdda(void)
{
	struct mx28_power_regs *power_regs =
		(struct mx28_power_regs *)MXS_POWER_BASE;

	writel((0xc << POWER_VDDACTRL_TRG_OFFSET) |
		(0x7 << POWER_VDDACTRL_BO_OFFSET_OFFSET) |
		POWER_VDDACTRL_LINREG_OFFSET_1STEPS_BELOW,
		&power_regs->hw_power_vddactrl);
}

static void mx28_mem_setup_vddd(void)
{
	struct mx28_power_regs *power_regs =
		(struct mx28_power_regs *)MXS_POWER_BASE;

	writel((0x1c << POWER_VDDDCTRL_TRG_OFFSET) |
		(0x7 << POWER_VDDDCTRL_BO_OFFSET_OFFSET) |
		POWER_VDDDCTRL_LINREG_OFFSET_1STEPS_BELOW,
		&power_regs->hw_power_vdddctrl);
}

#define	HW_DIGCTRL_SCRATCH0	0x8001c280
#define	HW_DIGCTRL_SCRATCH1	0x8001c290
static void data_abort_memdetect_handler(void) __attribute__((naked));
static void data_abort_memdetect_handler(void)
{
	asm volatile("subs pc, r14, #4");
}

static void mx28_mem_get_size(void)
{
	uint32_t sz, da;
	uint32_t *vt = (uint32_t *)0x20;

	/* Replace the DABT handler. */
	da = vt[4];
	vt[4] = (uint32_t)&data_abort_memdetect_handler;

	sz = get_ram_size((long *)PHYS_SDRAM_1, PHYS_SDRAM_1_SIZE);
	writel(sz, HW_DIGCTRL_SCRATCH0);
	writel(sz, HW_DIGCTRL_SCRATCH1);

	/* Restore the old DABT handler. */
	vt[4] = da;
}

void mx28_mem_init(void)
{
	struct mx28_clkctrl_regs *clkctrl_regs =
		(struct mx28_clkctrl_regs *)MXS_CLKCTRL_BASE;
	struct mx28_pinctrl_regs *pinctrl_regs =
		(struct mx28_pinctrl_regs *)MXS_PINCTRL_BASE;

	/* Set DDR2 mode */
	writel(PINCTRL_EMI_DS_CTRL_DDR_MODE_DDR2,
		&pinctrl_regs->hw_pinctrl_emi_ds_ctrl_set);

	/* Power up PLL0 */
	writel(CLKCTRL_PLL0CTRL0_POWER,
		&clkctrl_regs->hw_clkctrl_pll0ctrl0_set);

	early_delay(11000);

	mx28_mem_init_clock();

	mx28_mem_setup_vdda();

	/*
	 * Configure the DRAM registers
	 */

	/* Clear START bit from DRAM_CTL16 */
	clrbits_le32(MXS_DRAM_BASE + 0x40, 1);

	mx28_ddr2_setup();

	/* Clear SREFRESH bit from DRAM_CTL17 */
	clrbits_le32(MXS_DRAM_BASE + 0x44, 1);

	/* Set START bit in DRAM_CTL16 */
	setbits_le32(MXS_DRAM_BASE + 0x40, 1);

	/* Wait for bit 20 (DRAM init complete) in DRAM_CTL58 */
	while (!(readl(MXS_DRAM_BASE + 0xe8) & (1 << 20)))
		;

	mx28_mem_setup_vddd();

	early_delay(10000);

	mx28_mem_setup_cpu_and_hbus();

	mx28_mem_get_size();
}
