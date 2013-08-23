/*
 * Board specific setup info
 *
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Author :
 *	Aneesh V	<aneesh@ti.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <asm/arch/omap.h>
#include <asm/omap_common.h>
#include <asm/arch/spl.h>
#include <linux/linkage.h>

ENTRY(save_boot_params)
	ldr	r1, =OMAP_SRAM_SCRATCH_BOOT_PARAMS
	str	r0, [r1]
	bx	lr
ENDPROC(save_boot_params)

ENTRY(set_pl310_ctrl_reg)
	PUSH	{r4-r11, lr}	@ save registers - ROM code may pollute
				@ our registers
	LDR	r12, =0x102	@ Set PL310 control register - value in R0
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6 && defined(__ARM_ARCH_7A__)
	.arch_extension sec
	smc	#0		@ call ROM Code API to set control register
#else
	.word	0xe1600070
#endif
	POP	{r4-r11, pc}
ENDPROC(set_pl310_ctrl_reg)
