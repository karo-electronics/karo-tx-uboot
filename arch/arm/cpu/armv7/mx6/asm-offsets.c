/*
 * Adapted from Linux v2.6.36 kernel: arch/powerpc/kernel/asm-offsets.c
 *
 * This program is used to generate definitions needed by
 * assembly language modules.
 *
 * We use the technique used in the OSF Mach kernel code:
 * generate asm statements containing #defines,
 * compile this file to assembler, and then extract the
 * #defines from the assembly-language output.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <common.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>

#include <linux/kbuild.h>

int main(void)
{
	DEFINE(CCM_CCR, offsetof(struct mxc_ccm_reg, ccr));
	DEFINE(CCM_CCDR, offsetof(struct mxc_ccm_reg, ccdr));
	DEFINE(CCM_CSR, offsetof(struct mxc_ccm_reg, csr));
	DEFINE(CCM_CCSR, offsetof(struct mxc_ccm_reg, ccsr));
	DEFINE(CCM_CACRR, offsetof(struct mxc_ccm_reg, cacrr));
	DEFINE(CCM_CBCDR, offsetof(struct mxc_ccm_reg, cbcdr));
	DEFINE(CCM_CBCMR, offsetof(struct mxc_ccm_reg, cbcmr));
	DEFINE(CCM_CSCMR1, offsetof(struct mxc_ccm_reg, cscmr1));
	DEFINE(CCM_CSCMR2, offsetof(struct mxc_ccm_reg, cscmr2));
	DEFINE(CCM_CSCDR1, offsetof(struct mxc_ccm_reg, cscdr1));
	DEFINE(CCM_CS1CDR, offsetof(struct mxc_ccm_reg, cs1cdr));
	DEFINE(CCM_CS2CDR, offsetof(struct mxc_ccm_reg, cs2cdr));
	DEFINE(CCM_CDCDR, offsetof(struct mxc_ccm_reg, cdcdr));
	DEFINE(CCM_CHSCCDR, offsetof(struct mxc_ccm_reg, chsccdr));
	DEFINE(CCM_CSCDR2, offsetof(struct mxc_ccm_reg, cscdr2));
	DEFINE(CCM_CSCDR3, offsetof(struct mxc_ccm_reg, cscdr3));
	DEFINE(CCM_CSCDR4, offsetof(struct mxc_ccm_reg, cscdr4));
	DEFINE(CCM_CDHIPR, offsetof(struct mxc_ccm_reg, cdhipr));
	DEFINE(CCM_CDCR, offsetof(struct mxc_ccm_reg, cdcr));
	DEFINE(CCM_CTOR, offsetof(struct mxc_ccm_reg, ctor));
	DEFINE(CCM_CLPCR, offsetof(struct mxc_ccm_reg, clpcr));
	DEFINE(CCM_CISR, offsetof(struct mxc_ccm_reg, cisr));
	DEFINE(CCM_CIMR, offsetof(struct mxc_ccm_reg, cimr));
	DEFINE(CCM_CCOSR, offsetof(struct mxc_ccm_reg, ccosr));
	DEFINE(CCM_CGPR, offsetof(struct mxc_ccm_reg, cgpr));
	DEFINE(CCM_CCGR0, offsetof(struct mxc_ccm_reg, CCGR0));
	DEFINE(CCM_CCGR1, offsetof(struct mxc_ccm_reg, CCGR1));
	DEFINE(CCM_CCGR2, offsetof(struct mxc_ccm_reg, CCGR2));
	DEFINE(CCM_CCGR3, offsetof(struct mxc_ccm_reg, CCGR3));
	DEFINE(CCM_CCGR4, offsetof(struct mxc_ccm_reg, CCGR4));
	DEFINE(CCM_CCGR5, offsetof(struct mxc_ccm_reg, CCGR5));
	DEFINE(CCM_CCGR6, offsetof(struct mxc_ccm_reg, CCGR6));
	DEFINE(CCM_CCGR7, offsetof(struct mxc_ccm_reg, CCGR7));
	DEFINE(CCM_CMEOR, offsetof(struct mxc_ccm_reg, cmeor));

	DEFINE(ANATOP_PLL_ENET, offsetof(struct anatop_regs, pll_enet));
	return 0;
}
