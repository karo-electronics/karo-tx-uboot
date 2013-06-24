/*
 * Board specific setup info
 *
 * (C) Copyright 2010
 * Texas Instruments, <www.ti.com>
 *
 * Author :
 *	Aneesh V	<aneesh@ti.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <asm/arch/omap.h>
#include <asm/arch/spl.h>
#include <linux/linkage.h>

ENTRY(save_boot_params)
	/*
	 * See if the rom code passed pointer is valid:
	 * It is not valid if it is not in non-secure SRAM
	 * This may happen if you are booting with the help of
	 * debugger
	 */
	ldr     r2, =NON_SECURE_SRAM_START
	cmp	r2, r0
	bge	1f
	ldr	r2, =NON_SECURE_SRAM_END
	cmp	r2, r0
	blt	1f

	/*
	 * store the boot params passed from rom code or saved
	 * and passed by SPL
	 */
	cmp	r0, #0
	beq	1f
	ldr	r1, =boot_params
	str	r0, [r1]
#ifdef CONFIG_SPL_BUILD
	/* Store the boot device in spl_boot_device */
	ldrb	r2, [r0, #BOOT_DEVICE_OFFSET]	@ r2 <- value of boot device
	and	r2, #BOOT_DEVICE_MASK
	strb	r2, [r1, #BOOT_DEVICE_OFFSET]	@ spl_boot_device <- r2

	/* boot mode is passed only for devices that can raw/fat mode */
	cmp	r2, #BOOT_DEVICE_XIP
	blt	2f
	cmp	r2, #BOOT_DEVICE_MMC2
	bgt	2f
	/* Store the boot mode (raw/FAT) in omap_bootmode */
	ldr	r2, [r0, #DEV_DESC_PTR_OFFSET]	@ get the device descriptor ptr
	ldr	r2, [r2, #DEV_DATA_PTR_OFFSET]	@ get the pDeviceData ptr
	ldr	r2, [r2, #BOOT_MODE_OFFSET]	@ get the boot mode
	ldr	r3, =omap_bootmode
	str	r2, [r3]
2:
#endif
	ldrb	r2, [r0, #CH_FLAGS_OFFSET]
	strb	r2, [r1, #CH_FLAGS_OFFSET]
1:
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
