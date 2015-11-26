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

#include "../common/karo.h"
#include "pmic.h"

static int rn5t567_setup_regs(uchar slave_addr, struct pmic_regs *r,
			size_t count)
{
	int ret;
	int i;

	for (i = 0; i < count; i++, r++) {
#ifdef DEBUG
		unsigned char value;

		ret = i2c_read(slave_addr, r->addr, 1, &value, 1);
		if (ret) {
			printf("%s: failed to read PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
		if ((value & ~r->mask) != r->val) {
			printf("Changing PMIC reg %02x from %02x to %02x\n",
				r->addr, value, r->val);
		}
#endif
		ret = i2c_write(slave_addr, r->addr, 1, &r->val, 1);
		if (ret) {
			printf("%s: failed to write PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
#ifdef DEBUG
		ret = i2c_read(slave_addr, r->addr, 1, &value, 1);
		if (ret) {
			printf("%s: failed to read PMIC register %02x: %d\n",
				__func__, r->addr, ret);
			return ret;
		}
		if (value != r->val) {
			printf("Failed to set PMIC reg %02x to %02x; actual value: %02x\n",
				r->addr, r->val, value);
		}
#endif
	}
	return 0;
}

int rn5t567_pmic_setup(uchar slave_addr, struct pmic_regs *regs,
		size_t count)
{
	int ret;
	unsigned char value;

	ret = i2c_read(slave_addr, 0x11, 1, &value, 1);
	if (ret) {
		printf("%s: i2c_read error: %d\n", __func__, ret);
		return ret;
	}

	ret = rn5t567_setup_regs(slave_addr, regs, count);
	if (ret)
		return ret;

	ret = i2c_read(slave_addr, RN5T567_DC1DAC, 1, &value, 1);
	if (ret == 0) {
		printf("VDDCORE set to %umV\n", rn5t_regval_to_mV(value));
	} else {
		printf("Failed to read VDDCORE register setting\n");
	}
#ifndef CONFIG_SOC_MX6UL
	ret = i2c_read(slave_addr, RN5T567_DC2DAC, 1, &value, 1);
	if (ret == 0) {
		printf("VDDSOC  set to %umV\n", rn5t_regval_to_mV(value));
	} else {
		printf("Failed to read VDDSOC register setting\n");
	}
#endif
	return ret;
}
