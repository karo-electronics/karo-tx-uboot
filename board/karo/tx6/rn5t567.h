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

#define RN5T567_NOETIMSET	0x11
#define RN5T567_LDORTC1_SLOT	0x2a
#define RN5T567_DC1CTL		0x2c
#define RN5T567_DC1CTL2		0x2d
#define RN5T567_DC2CTL		0x2e
#define RN5T567_DC2CTL2		0x2f
#define RN5T567_DC3CTL		0x30
#define RN5T567_DC3CTL2		0x31
#define RN5T567_DC1DAC		0x36
#define RN5T567_DC2DAC		0x37
#define RN5T567_DC3DAC		0x38
#define RN5T567_DC4DAC		0x39
#define RN5T567_DC1DAC_SLP	0x3b
#define RN5T567_DC2DAC_SLP	0x3c
#define RN5T567_DC3DAC_SLP	0x3d
#define RN5T567_DC4DAC_SLP	0x3e
#define RN5T567_LDOEN1		0x44
#define RN5T567_LDOEN2		0x45
#define RN5T567_LDODIS		0x46
#define RN5T567_LDO1DAC		0x4c
#define RN5T567_LDO2DAC		0x4d
#define RN5T567_LDO3DAC		0x4e
#define RN5T567_LDO4DAC		0x4f
#define RN5T567_LDO5DAC		0x50
#define RN5T567_LDORTC1DAC	0x56 /* VBACKUP */

#define NOETIMSET_DIS_OFF_NOE_TIM	(1 << 3)

#define DC2_DC2EN		(1 << 0)
#define DC2_DC2DIS		(1 << 1)

/* calculate voltages in 10mV */
#define rn5t_v2r(v,n,m)			DIV_ROUND(((((v) < (n)) ? (n) : (v)) - (n)), (m))
#define rn5t_r2v(r,n,m)			(((r) * (m) + (n)) / 10)

/* DCDC1-4 */
#define rn5t_mV_to_regval(mV)		rn5t_v2r((mV) * 10, 6000, 125)
#define rn5t_regval_to_mV(r)		rn5t_r2v(r, 6000, 125)

/* LDO1-2, 4-5 */
#define rn5t_mV_to_regval2(mV)		rn5t_v2r((mV) * 10, 9000, 250)
#define rn5t_regval2_to_mV(r)		rn5t_r2v(r, 9000, 250)

/* LDO3 */
#define rn5t_mV_to_regval3(mV)		rn5t_v2r((mV) * 10, 6000, 250)
#define rn5t_regval3_to_mV(r)		rn5t_r2v(r, 6000, 250)

/* LDORTC */
#define rn5t_mV_to_regval_rtc(mV)	rn5t_v2r((mV) * 10, 17000, 250)
#define rn5t_regval_rtc_to_mV(r)	rn5t_r2v(r, 17000, 250)
