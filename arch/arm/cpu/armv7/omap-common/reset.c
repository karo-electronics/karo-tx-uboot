/*
 *
 * Common layer for reset related functionality of OMAP based socs.
 *
 * (C) Copyright 2012
 * Texas Instruments, <www.ti.com>
 *
 * Sricharan R <r.sricharan@ti.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <config.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <linux/compiler.h>

void __weak reset_cpu(unsigned long ignored)
{
	/* clear the reset status flags */
	writel(readl(PRM_RSTST), PRM_RSTST);
	writel(PRM_RSTCTRL_RESET, PRM_RSTCTRL);
}

u32 __weak warm_reset(void)
{
	return (readl(PRM_RSTST) & PRM_RSTST_WARM_RESET_MASK);
}

void __weak setup_warmreset_time(void)
{
}
