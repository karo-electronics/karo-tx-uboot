// SPDX-License-Identifier: GPL-2.0+
/*
 * Derived from imx8 by NXP.
 * 
 * Copyright 2022 Markus Bauer <MB@karo-electronics.com>
 */

#include <common.h>
#include <asm/arch/sys_proto.h>
#include <asm/ptrace.h>
#include <asm/system.h>

unsigned long call_rzg2l_sip(unsigned long id, unsigned long reg0,
			     unsigned long reg1, unsigned long reg2,
			     unsigned long reg3)
{
	struct pt_regs regs;

	regs.regs[0] = id;
	regs.regs[1] = reg0;
	regs.regs[2] = reg1;
	regs.regs[3] = reg2;
	regs.regs[4] = reg3;

	smc_call(&regs);

	return regs.regs[0];
}

/*
 * Do an SMC call to return 2 registers by having reg1 passed in by reference
 */
unsigned long call_rzg2l_sip_ret2(unsigned long id, unsigned long reg0,
				  unsigned long *reg1, unsigned long reg2,
				  unsigned long reg3)
{
	struct pt_regs regs;

	regs.regs[0] = id;
	regs.regs[1] = reg0;
	regs.regs[2] = *reg1;
	regs.regs[3] = reg2;
	regs.regs[4] = reg3;

	smc_call(&regs);

	*reg1 = regs.regs[1];

	return regs.regs[0];
}
