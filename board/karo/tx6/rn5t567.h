/*
 * Copyright (C) 2015 Lothar Waßmann <LW@KARO-electronics.de>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define RN5T567_SLPCNT		0x0e
#define RN5T567_REPCNT		0x0f
#define RN5T567_PWRONTIMSET	0x10
#define RN5T567_NOETIMSET	0x11
#define RN5T567_PWRIRSEL	0x15
#define RN5T567_DC1_SLOT	0x16
#define RN5T567_DC2_SLOT	0x17
#define RN5T567_DC3_SLOT	0x18
#define RN5T567_DC4_SLOT	0x19
#define RN5T567_LDO1_SLOT	0x1b
#define RN5T567_LDO2_SLOT	0x1c
#define RN5T567_LDO3_SLOT	0x1d
#define RN5T567_LDO4_SLOT	0x1e
#define RN5T567_LDO5_SLOT	0x1f
#define RN5T567_PSO0_SLOT	0x25
#define RN5T567_PSO1_SLOT	0x26
#define RN5T567_PSO2_SLOT	0x27
#define RN5T567_PSO3_SLOT	0x28
#define RN5T567_LDORTC1_SLOT	0x2a
#define RN5T567_DC1CTL		0x2c
#define RN5T567_DC1CTL2		0x2d
#define RN5T567_DC2CTL		0x2e
#define RN5T567_DC2CTL2		0x2f
#define RN5T567_DC3CTL		0x30
#define RN5T567_DC3CTL2		0x31
#define RN5T567_DC4CTL		0x32
#define RN5T567_DC4CTL2		0x33
#define RN5T567_DC1DAC		0x36
#define RN5T567_DC2DAC		0x37
#define RN5T567_DC3DAC		0x38
#define RN5T567_DC4DAC		0x39
#define RN5T567_DC1DAC_SLP	0x3b
#define RN5T567_DC2DAC_SLP	0x3c
#define RN5T567_DC3DAC_SLP	0x3d
#define RN5T567_DC4DAC_SLP	0x3e
#define RN5T567_DCIREN		0x40
#define RN5T567_DCIRQ		0x41
#define RN5T567_DCIRMON		0x42
#define RN5T567_LDOEN1		0x44
#define RN5T567_LDOEN2		0x45
#define RN5T567_LDODIS		0x46
#define RN5T567_LDO1DAC		0x4c
#define RN5T567_LDO2DAC		0x4d
#define RN5T567_LDO3DAC		0x4e
#define RN5T567_LDO4DAC		0x4f
#define RN5T567_LDO5DAC		0x50
#define RN5T567_LDORTC1DAC	0x56 /* VBACKUP */
#define RN5T567_LDORTC2DAC	0x57
#define RN5T567_LDO1DAC_SLP	0x58
#define RN5T567_LDO2DAC_SLP	0x59
#define RN5T567_LDO3DAC_SLP	0x5a
#define RN5T567_LDO4DAC_SLP	0x5b
#define RN5T567_LDO5DAC_SLP	0x5c
#define RN5T567_IOSEL		0x90
#define RN5T567_IOOUT		0x91
#define RN5T567_GPEDGE1		0x92
#define RN5T567_EN_GPIR		0x94
#define RN5T567_INTPOL		0x9c
#define RN5T567_INTEN		0x9d

#define NOETIMSET_DIS_OFF_NOE_TIM	(1 << 3)

#define DCnCTL_EN		(1 << 0)
#define DCnCTL_DIS		(1 << 1)
#define DCnMODE(m)		(((m) & 0x3) << 4)
#define DCnMODE_SLP(m)		(((m) & 0x3) << 6)
#define MODE_AUTO		0
#define MODE_PWM		1
#define MODE_PSM		2

#define DCnCTL2_LIMSDEN		(1 << 0)
#define DCnCTL2_LIM_NONE	(0 << 1)
#define DCnCTL2_LIM_LOW		(1 << 1)
#define DCnCTL2_LIM_MED		(2 << 1)
#define DCnCTL2_LIM_HIGH	(3 << 1)
#define DCnCTL2_SR_LOW		(2 << 4)
#define DCnCTL2_SR_MED		(1 << 4)
#define DCnCTL2_SR_HIGH		(0 << 4)
#define DCnCTL2_OSC_LOW		(0 << 6)
#define DCnCTL2_OSC_MED		(1 << 6)
#define DCnCTL2_OSC_HIGH	(2 << 6)

/* calculate voltages in 10mV */
#define rn5t_v2r(v,n,m)			DIV_ROUND(((((v) * 10 < (n)) ? (n) : (v) * 10) - (n)), m)
#define rn5t_r2v(r,n,m)			(((r) * (m) + (n)) / 10)

/* DCDC1-4 */
#define rn5t_mV_to_regval(mV)		rn5t_v2r(mV, 6000, 125)
#define rn5t_regval_to_mV(r)		rn5t_r2v(r, 6000, 125)

/* LDO1-2, 4-5 */
#define rn5t_mV_to_regval2(mV)		(rn5t_v2r(mV, 9000, 500) << 1)
#define rn5t_regval2_to_mV(r)		rn5t_r2v((r) >> 1, 9000, 500)

/* LDO3 */
#define rn5t_mV_to_regval3(mV)		(rn5t_v2r(mV, 6000, 500) << 1)
#define rn5t_regval3_to_mV(r)		rn5t_r2v((r) >> 1, 6000, 500)

/* LDORTC */
#define rn5t_mV_to_regval_rtc(mV)	(rn5t_v2r(mV, 12000, 500) << 1)
#define rn5t_regval_rtc_to_mV(r)	rn5t_r2v((r) >> 1, 12000, 500)
