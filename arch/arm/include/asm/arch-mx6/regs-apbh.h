/*
 * Freescale i.MX6Q APBH Register Definitions
 *
 * Copyright (C) 2012 Lothar Waßmann <LW@KARO-electronics.de>
 * based on: arch/arm/include/arch-mx28/apbh-regs.h by Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
 *
 * Based on code from LTIB:
 * Copyright 2008-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef __REGS_APBH_H__
#define __REGS_APBH_H__

#include <asm/arch/imx-regs.h>

#define	MXS_APBH_BASE		0x00110000

#ifndef	__ASSEMBLY__
struct apbh_regs {
	mx6_reg_32(hw_apbh_ctrl0);				/* 0x000 */
	mx6_reg_32(hw_apbh_ctrl1);				/* 0x010 */
	mx6_reg_32(hw_apbh_ctrl2);				/* 0x020 */
	mx6_reg_32(hw_apbh_channel_ctrl);			/* 0x030 */
	reg_32(hw_apbh_devsel);					/* 0x040 */
	reg_32(hw_apbh_dma_burst_size);				/* 0x050 */
	reg_32(hw_apbh_debug);					/* 0x060 */

	reg_32(reserved[9]);					/* 0x064-0x0fc */

	union {
		struct {
			reg_32(hw_apbh_ch_curcmdar);
			reg_32(hw_apbh_ch_nxtcmdar);
			reg_32(hw_apbh_ch_cmd);
			reg_32(hw_apbh_ch_bar);
			reg_32(hw_apbh_ch_sema);
			reg_32(hw_apbh_ch_debug1);
			reg_32(hw_apbh_ch_debug2);
		} ch[16];
		struct {
			reg_32(hw_apbh_ch0_curcmdar);		/* 0x100 */
			reg_32(hw_apbh_ch0_nxtcmdar);		/* 0x110 */
			reg_32(hw_apbh_ch0_cmd);		/* 0x120 */
			reg_32(hw_apbh_ch0_bar);		/* 0x130 */
			reg_32(hw_apbh_ch0_sema);		/* 0x140 */
			reg_32(hw_apbh_ch0_debug1);		/* 0x150 */
			reg_32(hw_apbh_ch0_debug2);		/* 0x160 */
			reg_32(hw_apbh_ch1_curcmdar);		/* 0x170 */
			reg_32(hw_apbh_ch1_nxtcmdar);		/* 0x180 */
			reg_32(hw_apbh_ch1_cmd);		/* 0x190 */
			reg_32(hw_apbh_ch1_bar);		/* 0x1a0 */
			reg_32(hw_apbh_ch1_sema);		/* 0x1b0 */
			reg_32(hw_apbh_ch1_debug1);		/* 0x1c0 */
			reg_32(hw_apbh_ch1_debug2);		/* 0x1d0 */
			reg_32(hw_apbh_ch2_curcmdar);		/* 0x1e0 */
			reg_32(hw_apbh_ch2_nxtcmdar);		/* 0x1f0 */
			reg_32(hw_apbh_ch2_cmd);		/* 0x200 */
			reg_32(hw_apbh_ch2_bar);		/* 0x210 */
			reg_32(hw_apbh_ch2_sema);		/* 0x220 */
			reg_32(hw_apbh_ch2_debug1);		/* 0x230 */
			reg_32(hw_apbh_ch2_debug2);		/* 0x240 */
			reg_32(hw_apbh_ch3_curcmdar);		/* 0x250 */
			reg_32(hw_apbh_ch3_nxtcmdar);		/* 0x260 */
			reg_32(hw_apbh_ch3_cmd);		/* 0x270 */
			reg_32(hw_apbh_ch3_bar);		/* 0x280 */
			reg_32(hw_apbh_ch3_sema);		/* 0x290 */
			reg_32(hw_apbh_ch3_debug1);		/* 0x2a0 */
			reg_32(hw_apbh_ch3_debug2);		/* 0x2b0 */
			reg_32(hw_apbh_ch4_curcmdar);		/* 0x2c0 */
			reg_32(hw_apbh_ch4_nxtcmdar);		/* 0x2d0 */
			reg_32(hw_apbh_ch4_cmd);		/* 0x2e0 */
			reg_32(hw_apbh_ch4_bar);		/* 0x2f0 */
			reg_32(hw_apbh_ch4_sema);		/* 0x300 */
			reg_32(hw_apbh_ch4_debug1);		/* 0x310 */
			reg_32(hw_apbh_ch4_debug2);		/* 0x320 */
			reg_32(hw_apbh_ch5_curcmdar);		/* 0x330 */
			reg_32(hw_apbh_ch5_nxtcmdar);		/* 0x340 */
			reg_32(hw_apbh_ch5_cmd);		/* 0x350 */
			reg_32(hw_apbh_ch5_bar);		/* 0x360 */
			reg_32(hw_apbh_ch5_sema);		/* 0x370 */
			reg_32(hw_apbh_ch5_debug1);		/* 0x380 */
			reg_32(hw_apbh_ch5_debug2);		/* 0x390 */
			reg_32(hw_apbh_ch6_curcmdar);		/* 0x3a0 */
			reg_32(hw_apbh_ch6_nxtcmdar);		/* 0x3b0 */
			reg_32(hw_apbh_ch6_cmd);		/* 0x3c0 */
			reg_32(hw_apbh_ch6_bar);		/* 0x3d0 */
			reg_32(hw_apbh_ch6_sema);		/* 0x3e0 */
			reg_32(hw_apbh_ch6_debug1);		/* 0x3f0 */
			reg_32(hw_apbh_ch6_debug2);		/* 0x400 */
			reg_32(hw_apbh_ch7_curcmdar);		/* 0x410 */
			reg_32(hw_apbh_ch7_nxtcmdar);		/* 0x420 */
			reg_32(hw_apbh_ch7_cmd);		/* 0x430 */
			reg_32(hw_apbh_ch7_bar);		/* 0x440 */
			reg_32(hw_apbh_ch7_sema);		/* 0x450 */
			reg_32(hw_apbh_ch7_debug1);		/* 0x460 */
			reg_32(hw_apbh_ch7_debug2);		/* 0x470 */
			reg_32(hw_apbh_ch8_curcmdar);		/* 0x480 */
			reg_32(hw_apbh_ch8_nxtcmdar);		/* 0x490 */
			reg_32(hw_apbh_ch8_cmd);		/* 0x4a0 */
			reg_32(hw_apbh_ch8_bar);		/* 0x4b0 */
			reg_32(hw_apbh_ch8_sema);		/* 0x4c0 */
			reg_32(hw_apbh_ch8_debug1);		/* 0x4d0 */
			reg_32(hw_apbh_ch8_debug2);		/* 0x4e0 */
			reg_32(hw_apbh_ch9_curcmdar);		/* 0x4f0 */
			reg_32(hw_apbh_ch9_nxtcmdar);		/* 0x500 */
			reg_32(hw_apbh_ch9_cmd);		/* 0x510 */
			reg_32(hw_apbh_ch9_bar);		/* 0x520 */
			reg_32(hw_apbh_ch9_sema);		/* 0x530 */
			reg_32(hw_apbh_ch9_debug1);		/* 0x540 */
			reg_32(hw_apbh_ch9_debug2);		/* 0x550 */
			reg_32(hw_apbh_ch10_curcmdar);		/* 0x560 */
			reg_32(hw_apbh_ch10_nxtcmdar);		/* 0x570 */
			reg_32(hw_apbh_ch10_cmd);		/* 0x580 */
			reg_32(hw_apbh_ch10_bar);		/* 0x590 */
			reg_32(hw_apbh_ch10_sema);		/* 0x5a0 */
			reg_32(hw_apbh_ch10_debug1);		/* 0x5b0 */
			reg_32(hw_apbh_ch10_debug2);		/* 0x5c0 */
			reg_32(hw_apbh_ch11_curcmdar);		/* 0x5d0 */
			reg_32(hw_apbh_ch11_nxtcmdar);		/* 0x5e0 */
			reg_32(hw_apbh_ch11_cmd);		/* 0x5f0 */
			reg_32(hw_apbh_ch11_bar);		/* 0x600 */
			reg_32(hw_apbh_ch11_sema);		/* 0x610 */
			reg_32(hw_apbh_ch11_debug1);		/* 0x620 */
			reg_32(hw_apbh_ch11_debug2);		/* 0x630 */
			reg_32(hw_apbh_ch12_curcmdar);		/* 0x640 */
			reg_32(hw_apbh_ch12_nxtcmdar);		/* 0x650 */
			reg_32(hw_apbh_ch12_cmd);		/* 0x660 */
			reg_32(hw_apbh_ch12_bar);		/* 0x670 */
			reg_32(hw_apbh_ch12_sema);		/* 0x680 */
			reg_32(hw_apbh_ch12_debug1);		/* 0x690 */
			reg_32(hw_apbh_ch12_debug2);		/* 0x6a0 */
			reg_32(hw_apbh_ch13_curcmdar);		/* 0x6b0 */
			reg_32(hw_apbh_ch13_nxtcmdar);		/* 0x6c0 */
			reg_32(hw_apbh_ch13_cmd);		/* 0x6d0 */
			reg_32(hw_apbh_ch13_bar);		/* 0x6e0 */
			reg_32(hw_apbh_ch13_sema);		/* 0x6f0 */
			reg_32(hw_apbh_ch13_debug1);		/* 0x700 */
			reg_32(hw_apbh_ch13_debug2);		/* 0x710 */
			reg_32(hw_apbh_ch14_curcmdar);		/* 0x720 */
			reg_32(hw_apbh_ch14_nxtcmdar);		/* 0x730 */
			reg_32(hw_apbh_ch14_cmd);		/* 0x740 */
			reg_32(hw_apbh_ch14_bar);		/* 0x750 */
			reg_32(hw_apbh_ch14_sema);		/* 0x760 */
			reg_32(hw_apbh_ch14_debug1);		/* 0x770 */
			reg_32(hw_apbh_ch14_debug2);		/* 0x780 */
			reg_32(hw_apbh_ch15_curcmdar);		/* 0x790 */
			reg_32(hw_apbh_ch15_nxtcmdar);		/* 0x7a0 */
			reg_32(hw_apbh_ch15_cmd);		/* 0x7b0 */
			reg_32(hw_apbh_ch15_bar);		/* 0x7c0 */
			reg_32(hw_apbh_ch15_sema);		/* 0x7d0 */
			reg_32(hw_apbh_ch15_debug1);		/* 0x7e0 */
			reg_32(hw_apbh_ch15_debug2);		/* 0x7f0 */
		};
	};
	mx6_reg_32(hw_apbh_version);				/* 0x800 */
};
#endif

#define	APBH_CTRL0_SFTRST				(1 << 31)
#define	APBH_CTRL0_CLKGATE				(1 << 30)
#define	APBH_CTRL0_AHB_BURST8_EN			(1 << 29)
#define	APBH_CTRL0_APB_BURST_EN				(1 << 28)
#define	APBH_CTRL0_RSVD0_MASK				(0xfff << 16)
#define	APBH_CTRL0_RSVD0_OFFSET				16
#define	APBH_CTRL0_CLKGATE_CHANNEL_MASK			0xffff
#define	APBH_CTRL0_CLKGATE_CHANNEL_OFFSET		0
#define	APBH_CTRL0_CLKGATE_CHANNEL_SSP0			0x0001
#define	APBH_CTRL0_CLKGATE_CHANNEL_SSP1			0x0002
#define	APBH_CTRL0_CLKGATE_CHANNEL_SSP2			0x0004
#define	APBH_CTRL0_CLKGATE_CHANNEL_SSP3			0x0008
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND0		0x0010
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND1		0x0020
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND2		0x0040
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND3		0x0080
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND4		0x0100
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND5		0x0200
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND6		0x0400
#define	APBH_CTRL0_CLKGATE_CHANNEL_NAND7		0x0800
#define	APBH_CTRL0_CLKGATE_CHANNEL_HSADC		0x1000
#define	APBH_CTRL0_CLKGATE_CHANNEL_LCDIF		0x2000

#define	APBH_CTRL1_CH15_CMDCMPLT_IRQ_EN			(1 << 31)
#define	APBH_CTRL1_CH14_CMDCMPLT_IRQ_EN			(1 << 30)
#define	APBH_CTRL1_CH13_CMDCMPLT_IRQ_EN			(1 << 29)
#define	APBH_CTRL1_CH12_CMDCMPLT_IRQ_EN			(1 << 28)
#define	APBH_CTRL1_CH11_CMDCMPLT_IRQ_EN			(1 << 27)
#define	APBH_CTRL1_CH10_CMDCMPLT_IRQ_EN			(1 << 26)
#define	APBH_CTRL1_CH9_CMDCMPLT_IRQ_EN			(1 << 25)
#define	APBH_CTRL1_CH8_CMDCMPLT_IRQ_EN			(1 << 24)
#define	APBH_CTRL1_CH7_CMDCMPLT_IRQ_EN			(1 << 23)
#define	APBH_CTRL1_CH6_CMDCMPLT_IRQ_EN			(1 << 22)
#define	APBH_CTRL1_CH5_CMDCMPLT_IRQ_EN			(1 << 21)
#define	APBH_CTRL1_CH4_CMDCMPLT_IRQ_EN			(1 << 20)
#define	APBH_CTRL1_CH3_CMDCMPLT_IRQ_EN			(1 << 19)
#define	APBH_CTRL1_CH2_CMDCMPLT_IRQ_EN			(1 << 18)
#define	APBH_CTRL1_CH1_CMDCMPLT_IRQ_EN			(1 << 17)
#define	APBH_CTRL1_CH0_CMDCMPLT_IRQ_EN			(1 << 16)
#define	APBH_CTRL1_CH_CMDCMPLT_IRQ_EN_OFFSET		16
#define	APBH_CTRL1_CH_CMDCMPLT_IRQ_EN_MASK		(0xffff << 16)
#define	APBH_CTRL1_CH15_CMDCMPLT_IRQ			(1 << 15)
#define	APBH_CTRL1_CH14_CMDCMPLT_IRQ			(1 << 14)
#define	APBH_CTRL1_CH13_CMDCMPLT_IRQ			(1 << 13)
#define	APBH_CTRL1_CH12_CMDCMPLT_IRQ			(1 << 12)
#define	APBH_CTRL1_CH11_CMDCMPLT_IRQ			(1 << 11)
#define	APBH_CTRL1_CH10_CMDCMPLT_IRQ			(1 << 10)
#define	APBH_CTRL1_CH9_CMDCMPLT_IRQ			(1 << 9)
#define	APBH_CTRL1_CH8_CMDCMPLT_IRQ			(1 << 8)
#define	APBH_CTRL1_CH7_CMDCMPLT_IRQ			(1 << 7)
#define	APBH_CTRL1_CH6_CMDCMPLT_IRQ			(1 << 6)
#define	APBH_CTRL1_CH5_CMDCMPLT_IRQ			(1 << 5)
#define	APBH_CTRL1_CH4_CMDCMPLT_IRQ			(1 << 4)
#define	APBH_CTRL1_CH3_CMDCMPLT_IRQ			(1 << 3)
#define	APBH_CTRL1_CH2_CMDCMPLT_IRQ			(1 << 2)
#define	APBH_CTRL1_CH1_CMDCMPLT_IRQ			(1 << 1)
#define	APBH_CTRL1_CH0_CMDCMPLT_IRQ			(1 << 0)

#define	APBH_CTRL2_CH15_ERROR_STATUS			(1 << 31)
#define	APBH_CTRL2_CH14_ERROR_STATUS			(1 << 30)
#define	APBH_CTRL2_CH13_ERROR_STATUS			(1 << 29)
#define	APBH_CTRL2_CH12_ERROR_STATUS			(1 << 28)
#define	APBH_CTRL2_CH11_ERROR_STATUS			(1 << 27)
#define	APBH_CTRL2_CH10_ERROR_STATUS			(1 << 26)
#define	APBH_CTRL2_CH9_ERROR_STATUS			(1 << 25)
#define	APBH_CTRL2_CH8_ERROR_STATUS			(1 << 24)
#define	APBH_CTRL2_CH7_ERROR_STATUS			(1 << 23)
#define	APBH_CTRL2_CH6_ERROR_STATUS			(1 << 22)
#define	APBH_CTRL2_CH5_ERROR_STATUS			(1 << 21)
#define	APBH_CTRL2_CH4_ERROR_STATUS			(1 << 20)
#define	APBH_CTRL2_CH3_ERROR_STATUS			(1 << 19)
#define	APBH_CTRL2_CH2_ERROR_STATUS			(1 << 18)
#define	APBH_CTRL2_CH1_ERROR_STATUS			(1 << 17)
#define	APBH_CTRL2_CH0_ERROR_STATUS			(1 << 16)
#define	APBH_CTRL2_CH15_ERROR_IRQ			(1 << 15)
#define	APBH_CTRL2_CH14_ERROR_IRQ			(1 << 14)
#define	APBH_CTRL2_CH13_ERROR_IRQ			(1 << 13)
#define	APBH_CTRL2_CH12_ERROR_IRQ			(1 << 12)
#define	APBH_CTRL2_CH11_ERROR_IRQ			(1 << 11)
#define	APBH_CTRL2_CH10_ERROR_IRQ			(1 << 10)
#define	APBH_CTRL2_CH9_ERROR_IRQ			(1 << 9)
#define	APBH_CTRL2_CH8_ERROR_IRQ			(1 << 8)
#define	APBH_CTRL2_CH7_ERROR_IRQ			(1 << 7)
#define	APBH_CTRL2_CH6_ERROR_IRQ			(1 << 6)
#define	APBH_CTRL2_CH5_ERROR_IRQ			(1 << 5)
#define	APBH_CTRL2_CH4_ERROR_IRQ			(1 << 4)
#define	APBH_CTRL2_CH3_ERROR_IRQ			(1 << 3)
#define	APBH_CTRL2_CH2_ERROR_IRQ			(1 << 2)
#define	APBH_CTRL2_CH1_ERROR_IRQ			(1 << 1)
#define	APBH_CTRL2_CH0_ERROR_IRQ			(1 << 0)

#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_MASK		(0xffff << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_OFFSET		16
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_SSP0		(0x0001 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_SSP1		(0x0002 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_SSP2		(0x0004 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_SSP3		(0x0008 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND0		(0x0010 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND1		(0x0020 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND2		(0x0040 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND3		(0x0080 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND4		(0x0100 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND5		(0x0200 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND6		(0x0400 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_NAND7		(0x0800 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_HSADC		(0x1000 << 16)
#define	APBH_CHANNEL_CTRL_RESET_CHANNEL_LCDIF		(0x2000 << 16)
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_MASK		0xffff
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_OFFSET		0
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_SSP0		0x0001
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_SSP1		0x0002
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_SSP2		0x0004
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_SSP3		0x0008
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND0		0x0010
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND1		0x0020
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND2		0x0040
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND3		0x0080
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND4		0x0100
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND5		0x0200
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND6		0x0400
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_NAND7		0x0800
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_HSADC		0x1000
#define	APBH_CHANNEL_CTRL_FREEZE_CHANNEL_LCDIF		0x2000

#define	APBH_DEVSEL_CH15_MASK				(0x3 << 30)
#define	APBH_DEVSEL_CH15_OFFSET				30
#define	APBH_DEVSEL_CH14_MASK				(0x3 << 28)
#define	APBH_DEVSEL_CH14_OFFSET				28
#define	APBH_DEVSEL_CH13_MASK				(0x3 << 26)
#define	APBH_DEVSEL_CH13_OFFSET				26
#define	APBH_DEVSEL_CH12_MASK				(0x3 << 24)
#define	APBH_DEVSEL_CH12_OFFSET				24
#define	APBH_DEVSEL_CH11_MASK				(0x3 << 22)
#define	APBH_DEVSEL_CH11_OFFSET				22
#define	APBH_DEVSEL_CH10_MASK				(0x3 << 20)
#define	APBH_DEVSEL_CH10_OFFSET				20
#define	APBH_DEVSEL_CH9_MASK				(0x3 << 18)
#define	APBH_DEVSEL_CH9_OFFSET				18
#define	APBH_DEVSEL_CH8_MASK				(0x3 << 16)
#define	APBH_DEVSEL_CH8_OFFSET				16
#define	APBH_DEVSEL_CH7_MASK				(0x3 << 14)
#define	APBH_DEVSEL_CH7_OFFSET				14
#define	APBH_DEVSEL_CH6_MASK				(0x3 << 12)
#define	APBH_DEVSEL_CH6_OFFSET				12
#define	APBH_DEVSEL_CH5_MASK				(0x3 << 10)
#define	APBH_DEVSEL_CH5_OFFSET				10
#define	APBH_DEVSEL_CH4_MASK				(0x3 << 8)
#define	APBH_DEVSEL_CH4_OFFSET				8
#define	APBH_DEVSEL_CH3_MASK				(0x3 << 6)
#define	APBH_DEVSEL_CH3_OFFSET				6
#define	APBH_DEVSEL_CH2_MASK				(0x3 << 4)
#define	APBH_DEVSEL_CH2_OFFSET				4
#define	APBH_DEVSEL_CH1_MASK				(0x3 << 2)
#define	APBH_DEVSEL_CH1_OFFSET				2
#define	APBH_DEVSEL_CH0_MASK				(0x3 << 0)
#define	APBH_DEVSEL_CH0_OFFSET				0

#define	APBH_DMA_BURST_SIZE_CH15_MASK			(0x3 << 30)
#define	APBH_DMA_BURST_SIZE_CH15_OFFSET			30
#define	APBH_DMA_BURST_SIZE_CH14_MASK			(0x3 << 28)
#define	APBH_DMA_BURST_SIZE_CH14_OFFSET			28
#define	APBH_DMA_BURST_SIZE_CH13_MASK			(0x3 << 26)
#define	APBH_DMA_BURST_SIZE_CH13_OFFSET			26
#define	APBH_DMA_BURST_SIZE_CH12_MASK			(0x3 << 24)
#define	APBH_DMA_BURST_SIZE_CH12_OFFSET			24
#define	APBH_DMA_BURST_SIZE_CH11_MASK			(0x3 << 22)
#define	APBH_DMA_BURST_SIZE_CH11_OFFSET			22
#define	APBH_DMA_BURST_SIZE_CH10_MASK			(0x3 << 20)
#define	APBH_DMA_BURST_SIZE_CH10_OFFSET			20
#define	APBH_DMA_BURST_SIZE_CH9_MASK			(0x3 << 18)
#define	APBH_DMA_BURST_SIZE_CH9_OFFSET			18
#define	APBH_DMA_BURST_SIZE_CH8_MASK			(0x3 << 16)
#define	APBH_DMA_BURST_SIZE_CH8_OFFSET			16
#define	APBH_DMA_BURST_SIZE_CH8_BURST0			(0x0 << 16)
#define	APBH_DMA_BURST_SIZE_CH8_BURST4			(0x1 << 16)
#define	APBH_DMA_BURST_SIZE_CH8_BURST8			(0x2 << 16)
#define	APBH_DMA_BURST_SIZE_CH7_MASK			(0x3 << 14)
#define	APBH_DMA_BURST_SIZE_CH7_OFFSET			14
#define	APBH_DMA_BURST_SIZE_CH6_MASK			(0x3 << 12)
#define	APBH_DMA_BURST_SIZE_CH6_OFFSET			12
#define	APBH_DMA_BURST_SIZE_CH5_MASK			(0x3 << 10)
#define	APBH_DMA_BURST_SIZE_CH5_OFFSET			10
#define	APBH_DMA_BURST_SIZE_CH4_MASK			(0x3 << 8)
#define	APBH_DMA_BURST_SIZE_CH4_OFFSET			8
#define	APBH_DMA_BURST_SIZE_CH3_MASK			(0x3 << 6)
#define	APBH_DMA_BURST_SIZE_CH3_OFFSET			6
#define	APBH_DMA_BURST_SIZE_CH3_BURST0			(0x0 << 6)
#define	APBH_DMA_BURST_SIZE_CH3_BURST4			(0x1 << 6)
#define	APBH_DMA_BURST_SIZE_CH3_BURST8			(0x2 << 6)

#define	APBH_DMA_BURST_SIZE_CH2_MASK			(0x3 << 4)
#define	APBH_DMA_BURST_SIZE_CH2_OFFSET			4
#define	APBH_DMA_BURST_SIZE_CH2_BURST0			(0x0 << 4)
#define	APBH_DMA_BURST_SIZE_CH2_BURST4			(0x1 << 4)
#define	APBH_DMA_BURST_SIZE_CH2_BURST8			(0x2 << 4)
#define	APBH_DMA_BURST_SIZE_CH1_MASK			(0x3 << 2)
#define	APBH_DMA_BURST_SIZE_CH1_OFFSET			2
#define	APBH_DMA_BURST_SIZE_CH1_BURST0			(0x0 << 2)
#define	APBH_DMA_BURST_SIZE_CH1_BURST4			(0x1 << 2)
#define	APBH_DMA_BURST_SIZE_CH1_BURST8			(0x2 << 2)

#define	APBH_DMA_BURST_SIZE_CH0_MASK			0x3
#define	APBH_DMA_BURST_SIZE_CH0_OFFSET			0
#define	APBH_DMA_BURST_SIZE_CH0_BURST0			0x0
#define	APBH_DMA_BURST_SIZE_CH0_BURST4			0x1
#define	APBH_DMA_BURST_SIZE_CH0_BURST8			0x2

#define	APBH_DEBUG_GPMI_ONE_FIFO			(1 << 0)

#define	APBH_CHn_CURCMDAR_CMD_ADDR_MASK			0xffffffff
#define	APBH_CHn_CURCMDAR_CMD_ADDR_OFFSET		0

#define	APBH_CHn_NXTCMDAR_CMD_ADDR_MASK			0xffffffff
#define	APBH_CHn_NXTCMDAR_CMD_ADDR_OFFSET		0

#define	APBH_CHn_CMD_XFER_COUNT_MASK			(0xffff << 16)
#define	APBH_CHn_CMD_XFER_COUNT_OFFSET			16
#define	APBH_CHn_CMD_CMDWORDS_MASK			(0xf << 12)
#define	APBH_CHn_CMD_CMDWORDS_OFFSET			12
#define	APBH_CHn_CMD_HALTONTERMINATE			(1 << 8)
#define	APBH_CHn_CMD_WAIT4ENDCMD			(1 << 7)
#define	APBH_CHn_CMD_SEMAPHORE				(1 << 6)
#define	APBH_CHn_CMD_NANDWAIT4READY			(1 << 5)
#define	APBH_CHn_CMD_NANDLOCK				(1 << 4)
#define	APBH_CHn_CMD_IRQONCMPLT				(1 << 3)
#define	APBH_CHn_CMD_CHAIN				(1 << 2)
#define	APBH_CHn_CMD_COMMAND_MASK			0x3
#define	APBH_CHn_CMD_COMMAND_OFFSET			0
#define	APBH_CHn_CMD_COMMAND_NO_DMA_XFER		0x0
#define	APBH_CHn_CMD_COMMAND_DMA_WRITE			0x1
#define	APBH_CHn_CMD_COMMAND_DMA_READ			0x2
#define	APBH_CHn_CMD_COMMAND_DMA_SENSE			0x3

#define	APBH_CHn_BAR_ADDRESS_MASK			0xffffffff
#define	APBH_CHn_BAR_ADDRESS_OFFSET			0

#define	APBH_CHn_SEMA_RSVD2_MASK			(0xff << 24)
#define	APBH_CHn_SEMA_RSVD2_OFFSET			24
#define	APBH_CHn_SEMA_PHORE_MASK			(0xff << 16)
#define	APBH_CHn_SEMA_PHORE_OFFSET			16
#define	APBH_CHn_SEMA_RSVD1_MASK			(0xff << 8)
#define	APBH_CHn_SEMA_RSVD1_OFFSET			8
#define	APBH_CHn_SEMA_INCREMENT_SEMA_MASK		(0xff << 0)
#define	APBH_CHn_SEMA_INCREMENT_SEMA_OFFSET		0

#define	APBH_CHn_DEBUG1_REQ				(1 << 31)
#define	APBH_CHn_DEBUG1_BURST				(1 << 30)
#define	APBH_CHn_DEBUG1_KICK				(1 << 29)
#define	APBH_CHn_DEBUG1_END				(1 << 28)
#define	APBH_CHn_DEBUG1_SENSE				(1 << 27)
#define	APBH_CHn_DEBUG1_READY				(1 << 26)
#define	APBH_CHn_DEBUG1_LOCK				(1 << 25)
#define	APBH_CHn_DEBUG1_NEXTCMDADDRVALID		(1 << 24)
#define	APBH_CHn_DEBUG1_RD_FIFO_EMPTY			(1 << 23)
#define	APBH_CHn_DEBUG1_RD_FIFO_FULL			(1 << 22)
#define	APBH_CHn_DEBUG1_WR_FIFO_EMPTY			(1 << 21)
#define	APBH_CHn_DEBUG1_WR_FIFO_FULL			(1 << 20)
#define	APBH_CHn_DEBUG1_RSVD1_MASK			(0x7fff << 5)
#define	APBH_CHn_DEBUG1_RSVD1_OFFSET			5
#define	APBH_CHn_DEBUG1_STATEMACHINE_MASK		0x1f
#define	APBH_CHn_DEBUG1_STATEMACHINE_OFFSET		0
#define	APBH_CHn_DEBUG1_STATEMACHINE_IDLE		0x00
#define	APBH_CHn_DEBUG1_STATEMACHINE_REQ_CMD1		0x01
#define	APBH_CHn_DEBUG1_STATEMACHINE_REQ_CMD3		0x02
#define	APBH_CHn_DEBUG1_STATEMACHINE_REQ_CMD2		0x03
#define	APBH_CHn_DEBUG1_STATEMACHINE_XFER_DECODE	0x04
#define	APBH_CHn_DEBUG1_STATEMACHINE_REQ_WAIT		0x05
#define	APBH_CHn_DEBUG1_STATEMACHINE_REQ_CMD4		0x06
#define	APBH_CHn_DEBUG1_STATEMACHINE_PIO_REQ		0x07
#define	APBH_CHn_DEBUG1_STATEMACHINE_READ_FLUSH		0x08
#define	APBH_CHn_DEBUG1_STATEMACHINE_READ_WAIT		0x09
#define	APBH_CHn_DEBUG1_STATEMACHINE_WRITE		0x0c
#define	APBH_CHn_DEBUG1_STATEMACHINE_READ_REQ		0x0d
#define	APBH_CHn_DEBUG1_STATEMACHINE_CHECK_CHAIN	0x0e
#define	APBH_CHn_DEBUG1_STATEMACHINE_XFER_COMPLETE	0x0f
#define	APBH_CHn_DEBUG1_STATEMACHINE_TERMINATE		0x14
#define	APBH_CHn_DEBUG1_STATEMACHINE_WAIT_END		0x15
#define	APBH_CHn_DEBUG1_STATEMACHINE_WRITE_WAIT		0x1c
#define	APBH_CHn_DEBUG1_STATEMACHINE_HALT_AFTER_TERM	0x1d
#define	APBH_CHn_DEBUG1_STATEMACHINE_CHECK_WAIT		0x1e
#define	APBH_CHn_DEBUG1_STATEMACHINE_WAIT_READY		0x1f

#define	APBH_CHn_DEBUG2_APB_BYTES_MASK			(0xffff << 16)
#define	APBH_CHn_DEBUG2_APB_BYTES_OFFSET		16
#define	APBH_CHn_DEBUG2_AHB_BYTES_MASK			0xffff
#define	APBH_CHn_DEBUG2_AHB_BYTES_OFFSET		0

#define	APBH_VERSION_MAJOR_MASK				(0xff << 24)
#define	APBH_VERSION_MAJOR_OFFSET			24
#define	APBH_VERSION_MINOR_MASK				(0xff << 16)
#define	APBH_VERSION_MINOR_OFFSET			16
#define	APBH_VERSION_STEP_MASK				0xffff
#define	APBH_VERSION_STEP_OFFSET			0

#endif	/* __REGS_APBH_H__ */
