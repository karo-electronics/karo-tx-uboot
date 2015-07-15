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

#ifdef CONFIG_SPL
ENTRY(save_boot_params)

	ldr	r1, =OMAP_SRAM_SCRATCH_BOOT_PARAMS
	str	r0, [r1]
	b	save_boot_params_ret
ENDPROC(save_boot_params)
#endif

ENTRY(omap_smc1)
	PUSH	{r4-r12, lr}	@ save registers - ROM code may pollute
				@ our registers
	MOV	r12, r0		@ Service
	MOV	r0, r1		@ Argument
	DSB
	DMB
#if __GNUC__ >= 4 && __GNUC_MINOR__ >= 6 && defined(__ARM_ARCH_7A__)
	.arch_extension sec
	smc	#0		@ call ROM Code API to set control register
#else
	.word	0xe1600070
#endif
	POP	{r4-r12, pc}
ENDPROC(omap_smc1)
