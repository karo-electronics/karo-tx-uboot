/*
 * Copyright (C) 2014 Lothar Waßmann <LW@KARO-electronics.de>
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

#include <common.h>
#include <i2c.h>

#include "pmic.h"

#define RN5T567_NOETIMSET	0x11
#define RN5T567_LDORTC1_SLOT	0x2a
#define RN5T567_DC1CTL		0x2c
#define RN5T567_DC1CTL2		0x2d
#define RN5T567_DC2CTL		0x2e
#define RN5T567_DC2CTL2		0x2f
#define RN5T567_DC3CTL		0x30
#define RN5T567_DC3CTL2		0x31
#define RN5T567_DC1DAC		0x36 /* CORE */
#define RN5T567_DC2DAC		0x37 /* SOC */
#define RN5T567_DC3DAC		0x38 /* DDR */
#define RN5T567_DC1DAC_SLP	0x3b
#define RN5T567_DC2DAC_SLP	0x3c
#define RN5T567_DC3DAC_SLP	0x3d
#define RN5T567_LDOEN1		0x44
#define RN5T567_LDODIS		0x46
#define RN5T567_LDOEN2		0x48
#define RN5T567_LDO3DAC		0x4e /* IO */
#define RN5T567_LDORTC1DAC	0x56 /* VBACKUP */

#define NOETIMSET_DIS_OFF_NOE_TIM	(1 << 3)

#define VDD_RTC_VAL		mV_to_regval_rtc(3000 * 10)
#define VDD_HIGH_VAL		mV_to_regval3(3000 * 10)
#define VDD_HIGH_VAL_LP		mV_to_regval3(3000 * 10)
#define VDD_CORE_VAL		mV_to_regval(1425 * 10)
#define VDD_CORE_VAL_LP		mV_to_regval(900 * 10)
#define VDD_SOC_VAL		mV_to_regval(1425 * 10)
#define VDD_SOC_VAL_LP		mV_to_regval(900 * 10)
#define VDD_DDR_VAL		mV_to_regval(1500 * 10)
#define VDD_DDR_VAL_LP		mV_to_regval(1500 * 10)

/* calculate voltages in 10mV */
/* DCDC1-3 */
#define mV_to_regval(mV)	DIV_ROUND(((((mV) < 6000) ? 6000 : (mV)) - 6000), 125)
#define regval_to_mV(v)		(((v) * 125 + 6000))

/* LDO1-2 */
#define mV_to_regval2(mV)	DIV_ROUND(((((mV) < 9000) ? 9000 : (mV)) - 9000), 250)
#define regval2_to_mV(v)	(((v) * 250 + 9000))

/* LDO3 */
#define mV_to_regval3(mV)	DIV_ROUND(((((mV) < 6000) ? 6000 : (mV)) - 6000), 250)
#define regval3_to_mV(v)	(((v) * 250 + 6000))

/* LDORTC */
#define mV_to_regval_rtc(mV)	DIV_ROUND(((((mV) < 17000) ? 17000 : (mV)) - 17000), 250)
#define regval_rtc_to_mV(v)	(((v) * 250 + 17000))

static struct rn5t567_regs {
	u8 addr;
	u8 val;
	u8 mask;
} rn5t567_regs[] = {
	{ RN5T567_NOETIMSET, NOETIMSET_DIS_OFF_NOE_TIM | 0x5, },
#if 0
	{ RN5T567_DC1DAC, VDD_CORE_VAL, },
	{ RN5T567_DC2DAC, VDD_SOC_VAL, },
	{ RN5T567_DC3DAC, VDD_DDR_VAL, },
	{ RN5T567_DC1DAC_SLP, VDD_CORE_VAL_LP, },
	{ RN5T567_DC2DAC_SLP, VDD_SOC_VAL_LP, },
	{ RN5T567_DC3DAC_SLP, VDD_DDR_VAL_LP, },
	{ RN5T567_LDOEN1, 0x01f, ~0x1f, },
	{ RN5T567_LDOEN2, 0x10, ~0x30, },
	{ RN5T567_LDODIS, 0x00, },
	{ RN5T567_LDO3DAC, VDD_HIGH_VAL, },
	{ RN5T567_LDORTCDAC, VDD_RTC_VAL, },
	{ RN5T567_LDORTC1_SLOT, 0x0f, ~0x3f, },
#endif
};

static struct rn5t567_regs debug_regs[] __maybe_unused = {
	{ 0x00,  4, },
	{ 0x09,  4, },
	{ 0x10, 16, },
	{ 0x25, 26, },
	{ 0x44,  3, },
	{ 0x4c,  5, },
	{ 0x56,  1, },
	{ 0x58,  5, },
	{ 0x97,  2, },
	{ 0xb0,  1, },
	{ 0xbc,  1, },
};

static int rn5t567_setup_regs(struct rn5t567_regs *r, size_t count)
{
	int ret;
	int i;

	for (i = 0; i < count; i++, r++) {
#ifdef DEBUG
		unsigned char value;

		ret = i2c_read(CONFIG_SYS_I2C_SLAVE, r->addr, 1, &value, 1);
		if ((value & ~r->mask) != r->val) {
			printf("Changing PMIC reg %02x from %02x to %02x\n",
				r->addr, value, r->val);
		}
		if (ret) {
			printf("%s: failed to read PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
#endif
//		value = (value & ~r->mask) | r->val;
		ret = i2c_write(CONFIG_SYS_I2C_SLAVE,
				r->addr, 1, &r->val, 1);
		if (ret) {
			printf("%s: failed to write PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
#ifdef DEBUG
		ret = i2c_read(CONFIG_SYS_I2C_SLAVE, r->addr, 1, &value, 1);
		printf("PMIC reg %02x is %02x\n", r->addr, value);
#endif
	}
#if 0
	for (i = 0; i < ARRAY_SIZE(debug_regs); i++) {
		int j;

		r = &debug_regs[i];
		for (j = r->addr; j < r->addr + r->val; j++) {
			unsigned char value;

			ret = i2c_read(CONFIG_SYS_I2C_SLAVE, j, 1, &value, 1);
			printf("PMIC reg %02x = %02x\n",
				j, value);
		}
	}
#endif
	debug("%s() complete\n", __func__);
	return 0;
}

int setup_pmic_voltages(void)
{
	int ret;
	unsigned char value;

	ret = i2c_probe(CONFIG_SYS_I2C_SLAVE);
	if (ret != 0) {
		printf("Failed to initialize I2C\n");
		return ret;
	}

	ret = i2c_read(CONFIG_SYS_I2C_SLAVE, 0x11, 1, &value, 1);
	if (ret) {
		printf("%s: i2c_read error: %d\n", __func__, ret);
		return ret;
	}

	ret = rn5t567_setup_regs(rn5t567_regs, ARRAY_SIZE(rn5t567_regs));
	if (ret)
		return ret;

	printf("VDDCORE set to %umV\n",
		DIV_ROUND(regval_to_mV(VDD_CORE_VAL), 10));
	printf("VDDSOC  set to %umV\n",
		DIV_ROUND(regval_to_mV(VDD_SOC_VAL), 10));

	return ret;
}
